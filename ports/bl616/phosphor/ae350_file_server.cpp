// TinyTang: AE350 file requests; see ae350_file_server.h.
//
// The step is Tang-Control's disc_task (core/tangpsx.cpp, Apache-2.0) cut
// down to one file: read the sequence, and when it moves, accept the request
// only if the sequence reads the same before and after its fields, since the
// mailbox crosses clock domains word by word.  It runs in the caller's task
// rather than one of its own, so the playback task can serve a track's
// requests between its looks at the track, and `phosphor run` can serve a
// test program until it returns.

#include "ae350_file_server.h"

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}

#include "ae350_play.h"
#include "fpga_stream.h"

namespace {

constexpr uint32_t STATE_WAIT = 0x01u;
constexpr uint32_t STATE_RECEIVE = 0x02u;
constexpr uint32_t STATE_RUN = 0x03u;
constexpr uint32_t START_TIMEOUT_MS = 2000;

// Answer a size query: one session carrying the size as four bytes.
fpga_file_stream_result send_size(const char *path)
{
    fpga_file_stream_result sent = {};
    sent.status = fpga_file_stream_status::TRANSPORT_FAILED;
    FILINFO info;
    sent.filesystem_status = f_stat(path, &info);
    if (sent.filesystem_status != FR_OK || info.fsize > UINT32_MAX) {
        sent.status = fpga_file_stream_status::OPEN_FAILED;
        return sent;
    }
    const uint32_t size = static_cast<uint32_t>(info.fsize);
    const uint8_t bytes[4] = {static_cast<uint8_t>(size), static_cast<uint8_t>(size >> 8),
                              static_cast<uint8_t>(size >> 16), static_cast<uint8_t>(size >> 24)};
    const uint16_t id = fpga_file_stream_reserve_id();
    const TickType_t started = xTaskGetTickCount();
    fpga_stream_result r;
    if (fpga_stream_send(FPGA_STREAM_START, id, 0, nullptr, 0, &r) && r.status == 0 &&
        fpga_stream_send(FPGA_STREAM_DATA, id, 0, bytes, sizeof(bytes), &r) && r.status == 0 &&
        r.next_offset == sizeof(bytes) &&
        fpga_stream_send(FPGA_STREAM_END, id, sizeof(bytes), nullptr, 0, &r) && r.status == 0) {
        sent.status = fpga_file_stream_status::OK;
        sent.bytes = sizeof(bytes);
    }
    sent.transport_status = r.status;
    sent.elapsed_ms = (xTaskGetTickCount() - started) * portTICK_PERIOD_MS;
    return sent;
}

} // namespace

bool ae350_request_baseline(uint32_t *sequence)
{
    return ae350_read32(AE350_REG_REQUEST_SEQUENCE, sequence);
}

void ae350_file_server_begin(ae350_file_server &server, const char *path, uint32_t baseline)
{
    server = {};
    server.path = path;
    server.last = baseline;
}

bool ae350_file_server_step(ae350_file_server &server, fpga_file_stream_cancel cancel,
                            void *cancel_context, ae350_file_request_report report,
                            void *report_context, bool *served)
{
    *served = false;
    uint32_t sequence = 0;
    if (!ae350_read32(AE350_REG_REQUEST_SEQUENCE, &sequence)) {
        return false;
    }
    if (sequence == server.last) {
        return true;
    }
    ae350_file_request request = {};
    uint32_t check = 0;
    if (!ae350_read32(AE350_REG_REQUEST_OFFSET, &request.offset) ||
        !ae350_read32(AE350_REG_REQUEST_LENGTH, &request.length) ||
        !ae350_read32(AE350_REG_REQUEST_SEQUENCE, &check)) {
        return false;
    }
    if (check != sequence) {
        return true;   // caught mid-write; read it again next step
    }
    // A trap writes mcause, mepc and mtval over the mailbox, which reads as a
    // new request; only a program in RUN can have made one.
    uint32_t state = 0;
    if (!ae350_read32(AE350_REG_STATE, &state)) {
        return false;
    }
    server.last = sequence;
    if ((state & 0xffu) != STATE_RUN) {
        return true;
    }
    request.sequence = sequence;
    *served = true;

    if (server.path == nullptr || request.length > AE350_REQUEST_MAX) {
        request.result.status = fpga_file_stream_status::INVALID_ARGUMENT;
        ++server.refused;
    } else {
        if (request.length == 0) {
            request.result = send_size(server.path);
        } else {
            fpga_file_stream_options options;
            options.offset = request.offset;
            options.length = request.length;
            request.result = fpga_file_stream(server.path, cancel, cancel_context, options);
        }
        ++server.requests;
        server.bytes += request.result.bytes;
        if (request.result.status != fpga_file_stream_status::OK) {
            ++server.failed;
        }
    }
    server.last_status = request.result.status;
    if (report != nullptr) {
        report(request, report_context);
    }
    return true;
}

ae350_file_serve_result ae350_serve_file(ae350_file_server &server,
                                         fpga_file_stream_cancel cancel,
                                         void *cancel_context,
                                         ae350_file_request_report report,
                                         void *report_context)
{
    ae350_file_serve_result result = {};
    bool running = false;
    const TickType_t started = xTaskGetTickCount();

    for (;;) {
        if (cancel != nullptr && cancel(cancel_context)) {
            result.cancelled = true;
            return result;
        }
        uint32_t state = 0;
        if (!ae350_read32(AE350_REG_STATE, &state)) {
            result.link_failed = true;
            return result;
        }
        result.state = state;
        const uint32_t phase = state & 0xffu;
        if (phase != STATE_RUN) {
            const bool starting = !running && (phase == STATE_WAIT || phase == STATE_RECEIVE);
            if (!starting ||
                xTaskGetTickCount() - started > pdMS_TO_TICKS(START_TIMEOUT_MS)) {
                return result;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        running = true;

        bool served = false;
        if (!ae350_file_server_step(server, cancel, cancel_context, report, report_context,
                                    &served)) {
            result.link_failed = true;
            return result;
        }
        // `cancel` may consume what it detects (the shell's Ctrl-C does), so
        // a send it cancelled ends the service here rather than at the next
        // poll.
        if (served && server.last_status == fpga_file_stream_status::CANCELLED) {
            result.cancelled = true;
            return result;
        }
        if (!served) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
