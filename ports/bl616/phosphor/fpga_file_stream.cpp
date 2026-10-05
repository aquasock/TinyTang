// TinyTang: Tang-Control's file streamer (utils/fpga_file_stream.cpp,
// Apache-2.0), ported.
//
// The session -- capabilities first, the 5 Mbaud switch when the core offers
// it, START, the optional FLAC reduction, DATA paced by the core's
// acknowledgements, END or CANCEL, and the rate restored on every path out --
// is Tang-Control's, line for line.  What changed: the FreeRTOS mutex that kept
// two streams apart is a busy flag, because this firmware has one caller (the
// shell) and the link lock already serialises every frame; and the buffer is
// ordinary static RAM, as nesload's is, since this port reads the card into
// plain memory.

#include "fpga_file_stream.h"

#include <limits.h>
#include <string.h>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "bflb_mtimer.h"
}

#include "flac_stream_prefix.h"
#include "fpga_debug.h"
#include "fpga_stream.h"

namespace {

volatile bool busy;
uint16_t next_stream_id = 1;
uint8_t stream_buffer[FPGA_STREAM_MAX_DATA];

bool take_busy(void)
{
    taskENTER_CRITICAL();
    const bool was = busy;
    busy = true;
    taskEXIT_CRITICAL();
    return !was;
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length)
{
    while (length-- != 0) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

bool send_frame(uint8_t flags, uint16_t stream_id, uint32_t offset,
                const uint8_t *data, uint16_t length, uint32_t expected_next,
                fpga_file_stream_result &summary)
{
    fpga_stream_result response;
    if (!fpga_stream_send(flags, stream_id, offset, data, length, &response)) {
        summary.transport_status = 0xff;
        return false;
    }
    summary.transport_status = response.status;
    return response.status == 0 && response.next_offset == expected_next;
}

bool cancellation_requested(fpga_file_stream_cancel cancel, void *context)
{
    return cancel != nullptr && cancel(context);
}

bool read_at(FIL &file, uint32_t offset, uint8_t *buffer, size_t length)
{
    UINT count = 0;
    return f_lseek(&file, offset) == FR_OK &&
           f_read(&file, buffer, static_cast<UINT>(length), &count) == FR_OK &&
           count == length;
}

} // namespace

uint16_t fpga_file_stream_reserve_id(void)
{
    taskENTER_CRITICAL();
    const uint16_t id = next_stream_id++;
    if (next_stream_id == 0) {
        next_stream_id = 1;
    }
    taskEXIT_CRITICAL();
    return id;
}

fpga_file_stream_result fpga_file_stream(const char *path,
                                         fpga_file_stream_cancel cancel,
                                         void *cancel_context,
                                         const fpga_file_stream_options &options)
{
    fpga_file_stream_result summary = {};
    summary.status = fpga_file_stream_status::INVALID_ARGUMENT;
    summary.filesystem_status = FR_OK;
    if (path == nullptr || path[0] == '\0') {
        return summary;
    }
    if (!take_busy()) {
        summary.status = fpga_file_stream_status::BUSY;
        return summary;
    }

    FIL file;
    bool file_open = false;
    bool using_fast_baud = false;
    bool session_started = false;
    uint16_t stream_id = 0;
    uint32_t crc = 0xffffffffu;
    uint32_t offset = 0;
    const uint64_t started = bflb_mtimer_get_time_ms();

    fpga_debug_result capabilities;
    if (!fpga_debug_request(FPGA_EXT_CAPABILITIES, 0, 0, &capabilities) ||
        capabilities.status != 0 ||
        (capabilities.data & FPGA_EXT_CAP_STREAM) == 0) {
        summary.status = fpga_file_stream_status::CORE_UNAVAILABLE;
        goto finish;
    }

    summary.filesystem_status = f_open(&file, path, FA_READ);
    if (summary.filesystem_status != FR_OK) {
        summary.status = fpga_file_stream_status::OPEN_FAILED;
        goto finish;
    }
    file_open = true;
    if (f_size(&file) > UINT32_MAX) {
        summary.status = fpga_file_stream_status::FILE_TOO_LARGE;
        goto finish;
    }
    if (options.offset != 0) {
        summary.filesystem_status = f_lseek(&file, options.offset);
        if (summary.filesystem_status != FR_OK) {
            summary.status = fpga_file_stream_status::READ_FAILED;
            goto finish;
        }
    }

    if ((capabilities.data & FPGA_EXT_CAP_BAUD_SWITCH) != 0) {
        if (!fpga_debug_set_baud(5000000)) {
            summary.status = fpga_file_stream_status::BAUD_FAILED;
            goto finish;
        }
        using_fast_baud = true;
    }

    stream_id = options.stream_id != 0 ? options.stream_id
                                       : fpga_file_stream_reserve_id();
    if (!send_frame(FPGA_STREAM_START, stream_id, 0, nullptr, 0, 0, summary)) {
        summary.status = fpga_file_stream_status::TRANSPORT_FAILED;
        goto finish;
    }
    session_started = true;

    if (options.reduce_flac_metadata) {
        uint8_t prefix[FLAC_REDUCED_PREFIX_LENGTH];
        uint32_t audio_offset = 0;
        const bool reduced = flac_reduced_prefix(
            [&file](uint32_t at, uint8_t *buffer, size_t length) {
                return read_at(file, at, buffer, length);
            },
            f_size(&file), prefix, audio_offset);
        summary.filesystem_status =
            f_lseek(&file, reduced ? audio_offset : 0);
        if (summary.filesystem_status != FR_OK) {
            summary.status = fpga_file_stream_status::READ_FAILED;
        } else if (reduced) {
            memcpy(stream_buffer, prefix, sizeof(prefix));
            crc = crc32_update(crc, stream_buffer, sizeof(prefix));
            if (send_frame(FPGA_STREAM_DATA, stream_id, 0, stream_buffer,
                           sizeof(prefix), sizeof(prefix), summary)) {
                offset = sizeof(prefix);
            } else {
                summary.status = fpga_file_stream_status::TRANSPORT_FAILED;
            }
        }
    }

    while (summary.status == fpga_file_stream_status::INVALID_ARGUMENT &&
           !cancellation_requested(cancel, cancel_context)) {
        UINT count = 0;
        UINT wanted = sizeof(stream_buffer);
        if (options.length != 0) {
            if (offset >= options.length) {
                summary.status = fpga_file_stream_status::OK;
                break;
            }
            if (options.length - offset < wanted) {
                wanted = options.length - offset;
            }
        }
        summary.filesystem_status =
            f_read(&file, stream_buffer, wanted, &count);
        if (summary.filesystem_status != FR_OK) {
            summary.status = fpga_file_stream_status::READ_FAILED;
            break;
        }
        if (count == 0) {
            summary.status = fpga_file_stream_status::OK;
            break;
        }
        crc = crc32_update(crc, stream_buffer, count);
        if (!send_frame(FPGA_STREAM_DATA, stream_id, offset, stream_buffer,
                        static_cast<uint16_t>(count), offset + count, summary)) {
            summary.status = fpga_file_stream_status::TRANSPORT_FAILED;
            break;
        }
        offset += count;
    }

    if (summary.status == fpga_file_stream_status::INVALID_ARGUMENT) {
        summary.status = fpga_file_stream_status::CANCELLED;
    }
    if (summary.status == fpga_file_stream_status::OK) {
        if (!send_frame(FPGA_STREAM_END, stream_id, offset, nullptr, 0, offset,
                        summary)) {
            summary.status = fpga_file_stream_status::TRANSPORT_FAILED;
        }
    } else if (session_started) {
        fpga_stream_result ignored;
        fpga_stream_send(FPGA_STREAM_CANCEL, stream_id, offset, nullptr, 0,
                         &ignored);
    }

finish:
    if (file_open) {
        const FRESULT close_status = f_close(&file);
        if (close_status != FR_OK &&
            summary.status == fpga_file_stream_status::OK) {
            summary.filesystem_status = close_status;
            summary.status = fpga_file_stream_status::CLOSE_FAILED;
        }
    }
    if (using_fast_baud && !fpga_debug_set_baud(2000000) &&
        summary.status == fpga_file_stream_status::OK) {
        summary.status = fpga_file_stream_status::RESTORE_BAUD_FAILED;
    }
    summary.bytes = offset;
    summary.crc32 = ~crc;
    summary.elapsed_ms = bflb_mtimer_get_time_ms() - started;
    busy = false;
    return summary;
}

const char *fpga_file_stream_status_text(fpga_file_stream_status status)
{
    switch (status) {
        case fpga_file_stream_status::OK: return "complete";
        case fpga_file_stream_status::INVALID_ARGUMENT: return "invalid path";
        case fpga_file_stream_status::BUSY: return "streamer busy";
        case fpga_file_stream_status::CORE_UNAVAILABLE: return "core unavailable";
        case fpga_file_stream_status::OPEN_FAILED: return "file open failed";
        case fpga_file_stream_status::FILE_TOO_LARGE: return "file exceeds 4 GiB";
        case fpga_file_stream_status::BAUD_FAILED: return "baud negotiation failed";
        case fpga_file_stream_status::READ_FAILED: return "file read failed";
        case fpga_file_stream_status::TRANSPORT_FAILED: return "FPGA stream failed";
        case fpga_file_stream_status::CANCELLED: return "cancelled";
        case fpga_file_stream_status::CLOSE_FAILED: return "file close failed";
        case fpga_file_stream_status::RESTORE_BAUD_FAILED: return "baud restore failed";
    }
    return "unknown error";
}
