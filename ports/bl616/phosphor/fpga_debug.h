// TinyTang: Tang-Control's register transport (utils/fpga_debug.h, Apache-2.0),
// ported.  The protocol, the frame layout and the checks on a response are
// Tang-Control's; what changed is the plumbing underneath (see fpga_debug.cpp).
#pragma once

#include <stddef.h>
#include <stdint.h>

// Version 1 extended control packets use legacy TangCore frame type 0x10.
// Every packet carries a sequence number and a CRC-16/CCITT-FALSE checksum.
enum : uint8_t {
    FPGA_EXT_VERSION = 1,
    FPGA_EXT_COMMAND = 0x10,
    FPGA_EXT_CAPABILITIES = 0x00,
    FPGA_EXT_READ32 = 0x01,
    FPGA_EXT_WRITE32 = 0x02,
    FPGA_EXT_SET_BAUD = 0x03,
    FPGA_EXT_WRITE_BLOCK = 0x04,
    // Validated multi-word writes use their own frame type and are answered
    // with a normal 0x10 response.
    FPGA_BLOCK_COMMAND = 0x12,
};

constexpr size_t FPGA_EXT_BLOCK_MAX_WORDS = 64;

enum : uint32_t {
    FPGA_EXT_CAP_READ32 = 1u << 0,
    FPGA_EXT_CAP_WRITE32 = 1u << 1,
    FPGA_EXT_CAP_STREAM = 1u << 2,
    FPGA_EXT_CAP_BAUD_SWITCH = 1u << 3,
    FPGA_EXT_CAP_WRITE_BLOCK = 1u << 4,
};

struct fpga_debug_result {
    uint8_t status;
    uint8_t opcode;
    uint16_t sequence;
    uint32_t address;
    uint32_t data;
};

struct fpga_debug_stats {
    uint32_t requests;
    uint32_t responses;
    uint32_t timeouts;
    uint32_t crc_errors;
    uint32_t malformed;
    uint32_t unexpected;
};

// One request and its matching 0x10 response, with the link held across both
// (EXTCTL-005).  Returns false on a timeout or if the link is down.
bool fpga_debug_request(uint8_t opcode, uint32_t address, uint32_t data,
                        fpga_debug_result *result, uint32_t timeout_ms = 250);
// Write 1-64 consecutive 32-bit registers starting at a word-aligned address.
// The FPGA applies none of them unless the complete frame validates.
bool fpga_debug_write_block(uint32_t address, const uint32_t *words, size_t count,
                            fpga_debug_result *result, uint32_t timeout_ms = 250);
void fpga_debug_get_stats(fpga_debug_stats *stats);
// Negotiate 2 or 5 Mbaud with the core and switch this end to match.
bool fpga_debug_set_baud(uint32_t baudrate);
