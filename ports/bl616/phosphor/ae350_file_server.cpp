// TinyTang: AE350 file requests; see ae350_file_server.h.
//
// The loop is Tang-Control's disc_task (core/tangpsx.cpp, Apache-2.0) cut
// down to one file and one caller: poll the sequence, and when it moves,
// accept the request only if the sequence reads the same before and after its
// fields, since the mailbox crosses clock domains word by word.  It runs in
// the calling task rather than a task of its own, and stops when the program
// does, because the loader leaving RUN means nothing is left to ask.

#include "ae350_file_server.h"

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}

#include "ae350_play.h"

namespace {

constexpr uint32_t STATE_WAIT = 0x01u;
constexpr uint32_t STATE_RECEIVE = 0x02u;
constexpr uint32_t STATE_RUN = 0x03u;
constexpr uint32_t START_TIMEOUT_MS = 2000;

} // namespace

bool ae350_request_baseline(uint32_t *sequence)
{
    return ae350_read32(AE350_REG_REQUEST_SEQUENCE, sequence);
}

ae350_file_serve_result ae350_serve_file(const char *path, uint32_t baseline,
                                         fpga_file_stream_cancel cancel,
                                         void *cancel_context,
                                         ae350_file_request_report report,
                                         void *report_context)
{
    ae350_file_serve_result served = {};
    uint32_t last = baseline;
    bool running = false;
    const TickType_t started = xTaskGetTickCount();

    for (;;) {
        if (cancel != nullptr && cancel(cancel_context)) {
            served.cancelled = true;
            return served;
        }
        uint32_t state = 0;
        if (!ae350_read32(AE350_REG_STATE, &state)) {
            served.link_failed = true;
            return served;
        }
        served.state = state;
        const uint32_t phase = state & 0xffu;
        if (phase != STATE_RUN) {
            const bool starting = !running && (phase == STATE_WAIT || phase == STATE_RECEIVE);
            if (!starting ||
                xTaskGetTickCount() - started > pdMS_TO_TICKS(START_TIMEOUT_MS)) {
                return served;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        running = true;

        uint32_t sequence = 0;
        if (!ae350_read32(AE350_REG_REQUEST_SEQUENCE, &sequence)) {
            served.link_failed = true;
            return served;
        }
        if (sequence == last) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        ae350_file_request request = {};
        uint32_t check = 0;
        if (!ae350_read32(AE350_REG_REQUEST_OFFSET, &request.offset) ||
            !ae350_read32(AE350_REG_REQUEST_LENGTH, &request.length) ||
            !ae350_read32(AE350_REG_REQUEST_SEQUENCE, &check)) {
            served.link_failed = true;
            return served;
        }
        if (check != sequence) {
            continue;
        }
        last = sequence;
        request.sequence = sequence;

        // A length of 0 means "to the end of the file" to fpga_file_stream,
        // which the mailbox does not offer, so it is refused with the rest,
        // as is every request when there is no file to serve.
        if (path == nullptr || request.length == 0 || request.length > AE350_REQUEST_MAX) {
            request.result.status = fpga_file_stream_status::INVALID_ARGUMENT;
            ++served.refused;
        } else {
            fpga_file_stream_options options;
            options.offset = request.offset;
            options.length = request.length;
            request.result = fpga_file_stream(path, cancel, cancel_context, options);
            ++served.requests;
            served.bytes += request.result.bytes;
            if (request.result.status != fpga_file_stream_status::OK) {
                ++served.failed;
            }
        }
        if (report != nullptr) {
            report(request, report_context);
        }
        // `cancel` may consume what it detects (the shell's Ctrl-C does), so
        // a cancelled send ends the service here rather than at the next poll.
        if (request.result.status == fpga_file_stream_status::CANCELLED) {
            served.cancelled = true;
            return served;
        }
    }
}
