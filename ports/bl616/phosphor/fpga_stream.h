// TinyTang: Tang-Control's stop-and-credit stream frame (utils/fpga_stream.h,
// Apache-2.0), ported.  See fpga_stream.cpp for what changed.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum : uint8_t {
    FPGA_STREAM_COMMAND = 0x11,
    FPGA_STREAM_VERSION = 1,
    FPGA_STREAM_START = 1u << 0,
    FPGA_STREAM_DATA = 1u << 1,
    FPGA_STREAM_END = 1u << 2,
    FPGA_STREAM_CANCEL = 1u << 3,
};

constexpr size_t FPGA_STREAM_MAX_DATA = 1024;

struct fpga_stream_result {
    uint8_t status;
    uint8_t flags;
    uint16_t stream_id;
    uint32_t next_offset;
    uint16_t credit;
};

// Send one stream frame and wait for the core's acknowledgement, holding the
// link across both.  The acknowledgement is the back-pressure: the core
// answers only when it has room, so a sender paces itself by waiting.
bool fpga_stream_send(uint8_t flags, uint16_t stream_id, uint32_t offset,
                      const uint8_t *data, uint16_t length,
                      fpga_stream_result *result, uint32_t timeout_ms = 1000);

// Where a stream's time goes, for `phosphor play`: frames sent, microseconds
// spent writing them to the UART, and microseconds spent waiting for the
// acknowledgement.  Reset by the caller.
struct fpga_stream_timing {
    uint32_t frames;
    uint32_t baud;          // the link rate the last frame went out at
    uint64_t send_us;
    uint64_t ack_us;
};
void fpga_stream_timing_reset(void);
void fpga_stream_timing_get(fpga_stream_timing *out);
