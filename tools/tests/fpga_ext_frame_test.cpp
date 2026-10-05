// TinyTang: copied unchanged from Tang-Control tests/fpga_ext_frame_test.cpp (Apache-2.0).

#include "fpga_ext_frame.h"

#include <assert.h>

#include <iostream>
#include <vector>

namespace {

// Expected bytes were computed independently with Python's binascii.crc_hqx
// initialized to 0xffff (CRC-16/CCITT-FALSE).

void test_write32_request()
{
    uint8_t payload[14] = {0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
                           0x12, 0x34, 0x56, 0x78};
    assert(fpga_ext_seal_request(0x10, payload, 12, 0x1002) == 14);
    const std::vector<uint8_t> expected = {0x01, 0x02, 0x10, 0x02, 0x00, 0x00, 0x00,
                                           0x20, 0x12, 0x34, 0x56, 0x78, 0x73, 0x17};
    assert(std::vector<uint8_t>(payload, payload + 14) == expected);
}

void test_block_request()
{
    const uint32_t words[] = {0x31000031u, 0xdeadbeefu, 0x00ff00ffu};
    uint8_t payload[FPGA_EXT_BLOCK_HEADER_LENGTH + 4 * 3 + 2];
    const size_t length = fpga_ext_block_payload(payload, 0x01, 0x04, 0x100, words, 3);
    assert(length == 21);
    assert(fpga_ext_seal_request(0x12, payload, length, 0x2001) == 23);
    const std::vector<uint8_t> expected = {
        0x01, 0x04, 0x20, 0x01, 0x00, 0x00, 0x01, 0x00, 0x03,
        0x31, 0x00, 0x00, 0x31, 0xde, 0xad, 0xbe, 0xef, 0x00, 0xff, 0x00, 0xff,
        0x3d, 0x96};
    assert(std::vector<uint8_t>(payload, payload + 23) == expected);
}

void test_maximum_block_request()
{
    uint32_t words[64];
    for (uint32_t i = 0; i < 64; ++i) {
        words[i] = (i * 0x01010101u) ^ 0xa5a5a5a5u;
    }
    uint8_t payload[FPGA_EXT_BLOCK_HEADER_LENGTH + 4 * 64 + 2];
    const size_t length = fpga_ext_block_payload(payload, 0x01, 0x04, 0x1000, words, 64);
    const size_t sealed = fpga_ext_seal_request(0x12, payload, length, 0x0007);
    // The FPGA accepts a frame length of 12 + 4 * count: command plus payload.
    assert(sealed + 1 == 12 + 4 * 64);
    assert(payload[8] == 64);
    assert(payload[sealed - 2] == 0x35 && payload[sealed - 1] == 0x01);
}

} // namespace

int main()
{
    test_write32_request();
    test_block_request();
    test_maximum_block_request();
    std::cout << "PASS extended request and validated block-write frame encoding\n";
    return 0;
}
