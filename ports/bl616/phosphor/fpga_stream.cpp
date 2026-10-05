// TinyTang: Tang-Control's stop-and-credit stream frame (utils/fpga_stream.cpp,
// Apache-2.0), ported.
//
// The frame and the acknowledgement checks are Tang-Control's.  Two things
// changed.  The frame is assembled in one buffer and sent with tang_fpga_frame,
// because this firmware's link writes a payload rather than byte by byte.  And
// the acknowledgement is waited for synchronously under the link lock, as in
// fpga_debug.cpp, in place of Tang-Control's receive task and semaphore.

#include "fpga_stream.h"
#include "fpga_ext_frame.h"

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "bflb_mtimer.h"
#include "tang_fpga_link.h"
}

namespace {

constexpr size_t HEADER_LENGTH = 10;            // version .. length
constexpr size_t RESPONSE_PAYLOAD_LENGTH = 13;

// Version, flags, id, offset and length, the data, then the CRC.
uint8_t frame[HEADER_LENGTH + FPGA_STREAM_MAX_DATA + 2];

fpga_stream_timing timing;

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

// Tang-Control's fpga_stream_handle_response, as a test on one candidate.
bool accept(const uint8_t *payload, int length, uint8_t flags, uint16_t stream_id,
            fpga_stream_result *result)
{
    if (length != static_cast<int>(RESPONSE_PAYLOAD_LENGTH) ||
        payload[0] != FPGA_STREAM_VERSION) {
        return false;
    }
    if (fpga_ext_packet_crc(FPGA_STREAM_COMMAND, payload, 11) != read_be16(&payload[11])) {
        return false;
    }
    if (payload[2] != flags || read_be16(&payload[3]) != stream_id) {
        return false;
    }
    result->status = payload[1];
    result->flags = payload[2];
    result->stream_id = stream_id;
    result->next_offset = read_be32(&payload[5]);
    result->credit = read_be16(&payload[9]);
    return true;
}

} // namespace

bool fpga_stream_send(uint8_t flags, uint16_t stream_id, uint32_t offset,
                      const uint8_t *data, uint16_t length,
                      fpga_stream_result *result, uint32_t timeout_ms)
{
    if (result == nullptr || length > FPGA_STREAM_MAX_DATA ||
        (length != 0 && data == nullptr)) {
        return false;
    }
    if (tang_fpga_link_open() != 0) {
        return false;
    }

    tang_fpga_lock();
    frame[0] = FPGA_STREAM_VERSION;
    frame[1] = flags;
    fpga_ext_write_be16(&frame[2], stream_id);
    fpga_ext_write_be32(&frame[4], offset);
    fpga_ext_write_be16(&frame[8], length);
    for (uint16_t i = 0; i < length; ++i) {
        frame[HEADER_LENGTH + i] = data[i];
    }
    const size_t body = HEADER_LENGTH + length;
    fpga_ext_write_be16(&frame[body],
                        fpga_ext_packet_crc(FPGA_STREAM_COMMAND, frame, body));

    bool received = false;
    const uint64_t send_start = bflb_mtimer_get_time_us();
    const int sent = tang_fpga_frame(FPGA_STREAM_COMMAND, frame, body + 2);
    const uint64_t ack_start = bflb_mtimer_get_time_us();
    if (sent == 0) {
        const uint64_t deadline = bflb_mtimer_get_time_ms() + timeout_ms;
        for (;;) {
            const uint64_t now = bflb_mtimer_get_time_ms();
            if (now >= deadline) {
                break;
            }
            uint8_t reply[RESPONSE_PAYLOAD_LENGTH + 8];
            const int n = tang_fpga_wait(FPGA_STREAM_COMMAND, reply, sizeof(reply),
                                         static_cast<uint32_t>(deadline - now));
            if (n < 0) {
                break;
            }
            if (accept(reply, n, flags, stream_id, result)) {
                received = true;
                break;
            }
        }
    }
    timing.frames++;
    timing.baud = tang_fpga_baud();
    timing.send_us += ack_start - send_start;
    timing.ack_us += bflb_mtimer_get_time_us() - ack_start;
    // Keep the shared link locked through the matching response: the FPGA
    // transport has one response channel and cannot accept an unrelated
    // request while this stream frame is outstanding.
    tang_fpga_unlock();
    // A stream can otherwise reacquire the link immediately and starve every
    // other sender for the length of a track.
    taskYIELD();
    return received;
}

void fpga_stream_timing_reset(void)
{
    timing = {};
}

void fpga_stream_timing_get(fpga_stream_timing *out)
{
    if (out != nullptr) {
        *out = timing;
    }
}
