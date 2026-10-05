// TinyTang: copied unchanged from Tang-Control tests/flac_stream_prefix_test.cpp (Apache-2.0).

#include "flac_stream_prefix.h"

#include <assert.h>

#include <iostream>
#include <vector>

namespace {

struct MemoryReader {
    const std::vector<uint8_t> &data;

    bool operator()(uint32_t offset, uint8_t *buffer, size_t length) const
    {
        if (offset > data.size() || length > data.size() - offset) {
            return false;
        }
        memcpy(buffer, data.data() + offset, length);
        return true;
    }
};

void append_block(std::vector<uint8_t> &file, uint8_t type, bool last,
                  const std::vector<uint8_t> &body)
{
    file.push_back(static_cast<uint8_t>((last ? 0x80 : 0x00) | type));
    file.push_back(static_cast<uint8_t>(body.size() >> 16));
    file.push_back(static_cast<uint8_t>(body.size() >> 8));
    file.push_back(static_cast<uint8_t>(body.size()));
    file.insert(file.end(), body.begin(), body.end());
}

std::vector<uint8_t> streaminfo_body()
{
    std::vector<uint8_t> body(FLAC_STREAMINFO_LENGTH);
    for (size_t i = 0; i < body.size(); ++i) {
        body[i] = static_cast<uint8_t>(0x10 + i);
    }
    return body;
}

std::vector<uint8_t> flac_file(bool with_picture)
{
    std::vector<uint8_t> file = {'f', 'L', 'a', 'C'};
    append_block(file, 0, !with_picture, streaminfo_body());
    if (with_picture) {
        append_block(file, 4, false, std::vector<uint8_t>(40, 0x55));
        append_block(file, 6, false, std::vector<uint8_t>(300000, 0xaa));
        append_block(file, 1, true, std::vector<uint8_t>(8192, 0x00));
    }
    file.push_back(0xff); // First frame sync byte.
    file.push_back(0xf8);
    return file;
}

void test_metadata_removed()
{
    const std::vector<uint8_t> file = flac_file(true);
    uint8_t prefix[FLAC_REDUCED_PREFIX_LENGTH];
    uint32_t audio_offset = 0;
    assert(flac_reduced_prefix(MemoryReader{file}, file.size(), prefix, audio_offset));
    assert(audio_offset == file.size() - 2);
    assert(file[audio_offset] == 0xff);
    // Marker, then STREAMINFO with only the last-block flag changed.
    assert(memcmp(prefix, "fLaC", 4) == 0);
    assert(prefix[4] == 0x80 && prefix[5] == 0 && prefix[6] == 0 && prefix[7] == 34);
    assert(memcmp(prefix + 8, file.data() + 8, FLAC_STREAMINFO_LENGTH) == 0);
}

void test_streaminfo_only()
{
    const std::vector<uint8_t> file = flac_file(false);
    uint8_t prefix[FLAC_REDUCED_PREFIX_LENGTH];
    uint32_t audio_offset = 0;
    assert(flac_reduced_prefix(MemoryReader{file}, file.size(), prefix, audio_offset));
    assert(audio_offset == FLAC_REDUCED_PREFIX_LENGTH);
    assert(std::vector<uint8_t>(prefix, prefix + FLAC_REDUCED_PREFIX_LENGTH) ==
           std::vector<uint8_t>(file.begin(), file.begin() + FLAC_REDUCED_PREFIX_LENGTH));
}

void test_rejected_inputs()
{
    uint8_t prefix[FLAC_REDUCED_PREFIX_LENGTH];
    uint32_t audio_offset = 0;

    std::vector<uint8_t> riff = flac_file(false);
    memcpy(riff.data(), "RIFF", 4);
    assert(!flac_reduced_prefix(MemoryReader{riff}, riff.size(), prefix, audio_offset));

    std::vector<uint8_t> short_info = flac_file(false);
    short_info[7] = 33;
    assert(!flac_reduced_prefix(MemoryReader{short_info}, short_info.size(), prefix,
                                audio_offset));

    // A block length running past the end of the file.
    std::vector<uint8_t> truncated = flac_file(true);
    truncated.resize(1000);
    assert(!flac_reduced_prefix(MemoryReader{truncated}, truncated.size(), prefix,
                                audio_offset));

    // A second STREAMINFO and the forbidden type 127 are invalid chains.
    std::vector<uint8_t> repeated = {'f', 'L', 'a', 'C'};
    append_block(repeated, 0, false, streaminfo_body());
    append_block(repeated, 0, true, streaminfo_body());
    assert(!flac_reduced_prefix(MemoryReader{repeated}, repeated.size(), prefix,
                                audio_offset));
    std::vector<uint8_t> forbidden = {'f', 'L', 'a', 'C'};
    append_block(forbidden, 0, false, streaminfo_body());
    append_block(forbidden, 127, true, {});
    assert(!flac_reduced_prefix(MemoryReader{forbidden}, forbidden.size(), prefix,
                                audio_offset));

    std::vector<uint8_t> tiny = {'f', 'L', 'a', 'C'};
    assert(!flac_reduced_prefix(MemoryReader{tiny}, tiny.size(), prefix, audio_offset));
}

} // namespace

int main()
{
    test_metadata_removed();
    test_streaminfo_only();
    test_rejected_inputs();
    std::cout << "PASS FLAC stream metadata reduction\n";
    return 0;
}
