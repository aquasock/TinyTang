// TinyTang: Tang-Control's file streamer (utils/fpga_file_stream.h,
// Apache-2.0), ported.  See fpga_file_stream.cpp for what changed.
#pragma once

#include <stdint.h>

extern "C" {
#include "ff.h"
}

enum class fpga_file_stream_status : uint8_t {
    OK,
    INVALID_ARGUMENT,
    BUSY,
    CORE_UNAVAILABLE,
    OPEN_FAILED,
    FILE_TOO_LARGE,
    BAUD_FAILED,
    READ_FAILED,
    TRANSPORT_FAILED,
    CANCELLED,
    CLOSE_FAILED,
    RESTORE_BAUD_FAILED,
};

struct fpga_file_stream_result {
    fpga_file_stream_status status;
    FRESULT filesystem_status;
    uint8_t transport_status;
    uint32_t bytes;
    uint32_t crc32;
    uint64_t elapsed_ms;
};

using fpga_file_stream_cancel = bool (*)(void *context);

struct fpga_file_stream_options {
    // Zero allocates the next session ID.  A caller that must recognise the
    // session in core status (Phosphor's audible-stream register) reserves
    // one first with fpga_file_stream_reserve_id().
    uint16_t stream_id = 0;
    // Send a native FLAC file as `fLaC`, STREAMINFO, and its audio frames,
    // omitting metadata blocks the core would skip anyway.  The summary byte
    // count and CRC then describe the transmitted stream, not the file.
    bool reduce_flac_metadata = false;
    // Stream only `length` bytes starting at file byte `offset` (length 0 =
    // to the end of the file).  Stream offsets still count from zero.
    uint32_t offset = 0;
    uint32_t length = 0;
};

uint16_t fpga_file_stream_reserve_id(void);
// `path` is a FatFS path (this port's real path, e.g. from tdsh_path_to_real).
fpga_file_stream_result fpga_file_stream(
    const char *path, fpga_file_stream_cancel cancel = nullptr,
    void *cancel_context = nullptr,
    const fpga_file_stream_options &options = {});
const char *fpga_file_stream_status_text(fpga_file_stream_status status);
