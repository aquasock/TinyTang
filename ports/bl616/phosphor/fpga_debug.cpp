// TinyTang: Tang-Control's register transport (utils/fpga_debug.cpp, Apache-2.0),
// ported.
//
// What is Tang-Control's and unchanged: the request layout, the sequence
// numbering, the CRC, and every check a response must pass before it is
// believed -- version, length, CRC, then opcode, sequence and address against
// the request outstanding.
//
// What changed is only the plumbing.  Tang-Control has a receive task that
// parses frames and hands a response over through a semaphore.  This firmware
// already has a frame cache (fpga_frames.c) and a wait that takes a response
// out of it by type (tang_fpga_wait), so a transaction here is synchronous:
// take the link lock, send, wait for the reply, release.  The lock is held
// across the wait on purpose -- the core has one response channel, and a
// request from anyone else sent before this reply arrives can overtake it
// (EXTCTL-005).  That is the one place this code deliberately differs from
// tang_fpga_wait's own note, which is written for the legacy commands.

#include "fpga_debug.h"
#include "fpga_ext_frame.h"

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "bflb_mtimer.h"
#include "tang_fpga_link.h"
}

namespace {

constexpr size_t REQUEST_PAYLOAD_LENGTH = 14;
constexpr size_t RESPONSE_PAYLOAD_LENGTH = 15;

fpga_debug_stats counters;
uint16_t next_sequence = 1;

uint16_t read_be16(const uint8_t *data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

uint32_t read_be32(const uint8_t *data)
{
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) | data[3];
}

void count(uint32_t &counter)
{
    taskENTER_CRITICAL();
    ++counter;
    taskEXIT_CRITICAL();
}

// Tang-Control's fpga_debug_handle_response, as a test on one candidate.
bool accept(const uint8_t *payload, int length, uint8_t opcode,
            uint16_t sequence, uint32_t address, fpga_debug_result *result)
{
    if (length != static_cast<int>(RESPONSE_PAYLOAD_LENGTH) ||
        payload[0] != FPGA_EXT_VERSION) {
        count(counters.malformed);
        return false;
    }
    if (fpga_ext_packet_crc(FPGA_EXT_COMMAND, payload, 13) != read_be16(&payload[13])) {
        count(counters.crc_errors);
        return false;
    }
    const uint8_t got_opcode = payload[1] & 0x7fu;
    const uint16_t got_sequence = read_be16(&payload[3]);
    const uint32_t got_address = read_be32(&payload[5]);
    if (got_sequence != sequence || got_opcode != opcode || got_address != address) {
        // A late answer to an earlier request that timed out.  Keep waiting
        // for this one's.
        count(counters.unexpected);
        return false;
    }
    result->status = payload[2];
    result->opcode = got_opcode;
    result->sequence = got_sequence;
    result->address = got_address;
    result->data = read_be32(&payload[9]);
    return true;
}

// Send one request whose payload starts with version, opcode, a sequence
// placeholder and the address, then wait for the matching 0x10 response.  The
// payload buffer must have two spare bytes for the CRC.  Caller holds the lock.
bool transaction_locked(uint8_t command, uint8_t *payload, size_t length,
                        fpga_debug_result *result, uint32_t timeout_ms)
{
    if (result == nullptr) {
        return false;
    }
    const uint16_t sequence = next_sequence++;
    if (next_sequence == 0) {
        next_sequence = 1;
    }
    const size_t sealed = fpga_ext_seal_request(command, payload, length, sequence);
    const uint8_t opcode = payload[1];
    const uint32_t address = read_be32(&payload[4]);

    count(counters.requests);
    if (tang_fpga_frame(command, payload, sealed) != 0) {
        count(counters.timeouts);
        return false;
    }

    const uint64_t deadline = bflb_mtimer_get_time_ms() + timeout_ms;
    for (;;) {
        // A pre-empted task can wake past the deadline with the reply already
        // queued, so the ring is always checked once more before giving up.
        const uint64_t now = bflb_mtimer_get_time_ms();
        const uint32_t remaining =
            now < deadline ? static_cast<uint32_t>(deadline - now) : 0;
        uint8_t reply[RESPONSE_PAYLOAD_LENGTH + 8];
        const int n = tang_fpga_wait(FPGA_EXT_COMMAND, reply, sizeof(reply),
                                     remaining);
        if (n < 0) {
            count(counters.timeouts);
            return false;
        }
        if (accept(reply, n, opcode, sequence, address, result)) {
            count(counters.responses);
            return true;
        }
    }
}

bool request_locked(uint8_t opcode, uint32_t address, uint32_t data,
                    fpga_debug_result *result, uint32_t timeout_ms)
{
    uint8_t payload[REQUEST_PAYLOAD_LENGTH];
    payload[0] = FPGA_EXT_VERSION;
    payload[1] = opcode;
    fpga_ext_write_be32(&payload[4], address);
    fpga_ext_write_be32(&payload[8], data);
    return transaction_locked(FPGA_EXT_COMMAND, payload, REQUEST_PAYLOAD_LENGTH - 2,
                              result, timeout_ms);
}

} // namespace

bool fpga_debug_request(uint8_t opcode, uint32_t address, uint32_t data,
                        fpga_debug_result *result, uint32_t timeout_ms)
{
    if (tang_fpga_link_open() != 0) {
        return false;
    }
    tang_fpga_lock();
    const bool received = request_locked(opcode, address, data, result, timeout_ms);
    tang_fpga_unlock();
    // Let another sender waiting on the link run before this task can submit
    // its next transaction.
    taskYIELD();
    return received;
}

bool fpga_debug_write_block(uint32_t address, const uint32_t *words, size_t count,
                            fpga_debug_result *result, uint32_t timeout_ms)
{
    if (words == nullptr || count == 0 || count > FPGA_EXT_BLOCK_MAX_WORDS ||
        (address & 3u) != 0) {
        return false;
    }
    if (tang_fpga_link_open() != 0) {
        return false;
    }
    static uint8_t payload[FPGA_EXT_BLOCK_HEADER_LENGTH + 4 * FPGA_EXT_BLOCK_MAX_WORDS + 2];
    tang_fpga_lock();
    const size_t length = fpga_ext_block_payload(payload, FPGA_EXT_VERSION,
                                                 FPGA_EXT_WRITE_BLOCK, address,
                                                 words, count);
    const bool received = transaction_locked(FPGA_BLOCK_COMMAND, payload, length,
                                             result, timeout_ms);
    tang_fpga_unlock();
    taskYIELD();
    return received;
}

void fpga_debug_get_stats(fpga_debug_stats *stats)
{
    if (stats == nullptr) {
        return;
    }
    taskENTER_CRITICAL();
    *stats = counters;
    taskEXIT_CRITICAL();
}

bool fpga_debug_set_baud(uint32_t baudrate)
{
    if (baudrate != 2000000 && baudrate != 5000000) {
        return false;
    }
    if (tang_fpga_link_open() != 0) {
        return false;
    }
    if (tang_fpga_baud() == baudrate) {
        return true;
    }
    tang_fpga_lock();
    fpga_debug_result result;
    if (!request_locked(FPGA_EXT_SET_BAUD, 0, baudrate, &result, 1000) ||
        result.status != 0 || result.data != baudrate) {
        tang_fpga_unlock();
        return false;
    }
    // The FPGA changes rate only after its final response stop bit.
    vTaskDelay(pdMS_TO_TICKS(2));
    const bool changed = tang_fpga_set_baud(baudrate) == 0;
    tang_fpga_unlock();
    taskYIELD();
    return changed;
}
