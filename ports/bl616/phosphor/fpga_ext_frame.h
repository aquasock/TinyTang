// TinyTang: copied unchanged from Tang-Control utils/fpga_ext_frame.h (Apache-2.0).
// Pure encoders with no RTOS dependency, so tools/tests exercises the exact
// bytes the firmware sends.

#pragma once

#include <stddef.h>
#include <stdint.h>

// Pure encoders for extended FPGA request payloads.  They carry no RTOS
// dependency so host tests exercise the exact bytes the firmware transmits.

inline uint16_t fpga_ext_crc16_byte(uint16_t crc, uint8_t byte)
{
    crc ^= static_cast<uint16_t>(byte) << 8;
    for (unsigned bit = 0; bit < 8; ++bit) {
        crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
                              : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

// CRC-16/CCITT-FALSE over the frame's command byte and payload.
inline uint16_t fpga_ext_packet_crc(uint8_t command, const uint8_t *payload,
                                    size_t length)
{
    uint16_t crc = 0xffffu;
    crc = fpga_ext_crc16_byte(crc, command);
    for (size_t i = 0; i < length; ++i) {
        crc = fpga_ext_crc16_byte(crc, payload[i]);
    }
    return crc;
}

inline void fpga_ext_write_be16(uint8_t *data, uint16_t value)
{
    data[0] = static_cast<uint8_t>(value >> 8);
    data[1] = static_cast<uint8_t>(value);
}

inline void fpga_ext_write_be32(uint8_t *data, uint32_t value)
{
    data[0] = static_cast<uint8_t>(value >> 24);
    data[1] = static_cast<uint8_t>(value >> 16);
    data[2] = static_cast<uint8_t>(value >> 8);
    data[3] = static_cast<uint8_t>(value);
}

// Version, opcode, sequence, address, and word count precede block data.
constexpr size_t FPGA_EXT_BLOCK_HEADER_LENGTH = 9;

// Fill a block-write payload (without sequence or CRC) and return its length.
inline size_t fpga_ext_block_payload(uint8_t *payload, uint8_t version, uint8_t opcode,
                                     uint32_t address, const uint32_t *words,
                                     size_t count)
{
    payload[0] = version;
    payload[1] = opcode;
    fpga_ext_write_be32(&payload[4], address);
    payload[8] = static_cast<uint8_t>(count);
    for (size_t i = 0; i < count; ++i) {
        fpga_ext_write_be32(&payload[FPGA_EXT_BLOCK_HEADER_LENGTH + 4 * i], words[i]);
    }
    return FPGA_EXT_BLOCK_HEADER_LENGTH + 4 * count;
}

// Store the sequence at payload offset 2 and append the CRC after `length`
// payload bytes.  Returns the number of payload bytes to transmit.
inline size_t fpga_ext_seal_request(uint8_t command, uint8_t *payload, size_t length,
                                    uint16_t sequence)
{
    fpga_ext_write_be16(&payload[2], sequence);
    fpga_ext_write_be16(&payload[length], fpga_ext_packet_crc(command, payload, length));
    return length + 2;
}
