// SPDX-License-Identifier: MIT
// Write the exact UART bytes the BL616 firmware sends for OLED block writes,
// using its own encoder (ports/bl616/phosphor/fpga_ext_frame.h) and the frame
// header tang_fpga_frame() puts in front, so tb_desktop_uart.sv replays what
// the board sends rather than its own idea of the protocol.
//
//   gen_desktop_block_frames <dir>
// writes <dir>/firmware-blocks.hex (one byte per line), firmware-blocks.count
// ("<frames> <bytes>") and firmware-cells.hex (the 384 cells expected after).
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "fpga_ext_frame.h"

namespace {
constexpr uint8_t FRAME_MAGIC = 0xaa;    // tang_fpga_link.h FPGA_FRAME_MAGIC
constexpr uint8_t BLOCK_COMMAND = 0x12;  // fpga_debug.h FPGA_BLOCK_COMMAND
constexpr uint8_t WRITE_BLOCK = 0x04;    // fpga_debug.h FPGA_EXT_WRITE_BLOCK

std::vector<uint8_t> wire;
uint16_t cells[384];
unsigned frames;

void block(unsigned first, unsigned count, uint16_t sequence, unsigned salt)
{
    uint32_t words[64];
    for (unsigned i = 0; i < count; i++) {
        const unsigned k = first + i + salt;
        words[i] = static_cast<uint32_t>((k * 5 % 16) << 12 | (k * 3 % 16) << 8 | (32 + k * 7 % 95));
        cells[first + i] = static_cast<uint16_t>(words[i]);
    }
    uint8_t payload[FPGA_EXT_BLOCK_HEADER_LENGTH + 4 * 64 + 2];
    const size_t length = fpga_ext_block_payload(payload, 1, WRITE_BLOCK, 0x200 + first * 4,
                                                 words, count);
    const size_t sealed = fpga_ext_seal_request(BLOCK_COMMAND, payload, length, sequence);
    const size_t len = sealed + 1;       // tang_fpga_frame counts the type byte
    wire.push_back(FRAME_MAGIC);
    wire.push_back(static_cast<uint8_t>(len >> 8));
    wire.push_back(static_cast<uint8_t>(len));
    wire.push_back(BLOCK_COMMAND);
    wire.insert(wire.end(), payload, payload + sealed);
    frames++;
}
}  // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: gen_desktop_block_frames <dir>\n");
        return 2;
    }
    // oled_link.cpp's full repaint: six 64-cell blocks.
    for (unsigned first = 0; first < 384; first += 64) block(first, 64, 0x4000 + frames, 0);
    // Then a short block in the middle and the last cell alone.
    block(100, 3, 0x4000 + frames, 11);
    block(383, 1, 0x4000 + frames, 29);

    const std::string dir = argv[1];
    FILE *bytes = std::fopen((dir + "/firmware-blocks.hex").c_str(), "w");
    FILE *count = std::fopen((dir + "/firmware-blocks.count").c_str(), "w");
    FILE *expect = std::fopen((dir + "/firmware-cells.hex").c_str(), "w");
    if (!bytes || !count || !expect) return 1;
    for (uint8_t b : wire) std::fprintf(bytes, "%02x\n", b);
    std::fprintf(count, "%u %zu\n", frames, wire.size());
    for (uint16_t c : cells) std::fprintf(expect, "%04x\n", c);
    std::fclose(bytes);
    std::fclose(count);
    std::fclose(expect);
    return 0;
}
