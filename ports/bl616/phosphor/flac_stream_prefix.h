// TinyTang: copied unchanged from Tang-Control utils/flac_stream_prefix.h (Apache-2.0).
// Pure encoders with no RTOS dependency, so tools/tests exercises the exact
// bytes the firmware sends.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Native FLAC stream reduction for FPGA playback (RFC 9639 section 8).  The
// decoder needs only the `fLaC` marker and STREAMINFO before the first frame;
// every other metadata block (PICTURE, VORBIS_COMMENT, PADDING, SEEKTABLE) is
// skipped by the core.  Sending those bytes over the FPGA UART delays the start
// of the track, so the stream is rebuilt as the marker, STREAMINFO marked as
// the last metadata block, and the unchanged audio frames.  The reader carries
// no RTOS or FatFs dependency so host tests exercise the exact bytes.

constexpr size_t FLAC_STREAMINFO_LENGTH = 34;
constexpr size_t FLAC_REDUCED_PREFIX_LENGTH = 4 + 4 + FLAC_STREAMINFO_LENGTH;
constexpr unsigned FLAC_MAX_METADATA_BLOCKS = 1024;

// `read(offset, buffer, length)` must fill exactly `length` bytes or return
// false.  On success, `prefix` holds the reduced header and `audio_offset` is
// the file offset of the first audio frame.  Returns false for anything that
// is not a well-formed native FLAC metadata chain; the caller then streams the
// file unchanged and lets the core report the error.
template <typename Reader>
bool flac_reduced_prefix(Reader &&read, uint64_t file_size,
                         uint8_t (&prefix)[FLAC_REDUCED_PREFIX_LENGTH],
                         uint32_t &audio_offset)
{
    if (file_size < FLAC_REDUCED_PREFIX_LENGTH ||
        !read(0, prefix, FLAC_REDUCED_PREFIX_LENGTH) ||
        memcmp(prefix, "fLaC", 4) != 0 || (prefix[4] & 0x7f) != 0 ||
        prefix[5] != 0 || prefix[6] != 0 || prefix[7] != FLAC_STREAMINFO_LENGTH) {
        return false;
    }

    bool last = (prefix[4] & 0x80) != 0;
    uint64_t offset = FLAC_REDUCED_PREFIX_LENGTH;
    for (unsigned blocks = 1; !last; ++blocks) {
        uint8_t header[4];
        if (blocks >= FLAC_MAX_METADATA_BLOCKS || offset + 4 > file_size ||
            !read(static_cast<uint32_t>(offset), header, 4) ||
            (header[0] & 0x7f) == 0 || (header[0] & 0x7f) == 127) {
            return false;
        }
        last = (header[0] & 0x80) != 0;
        offset += 4 + ((static_cast<uint32_t>(header[1]) << 16) |
                       (static_cast<uint32_t>(header[2]) << 8) | header[3]);
        if (offset > file_size) {
            return false;
        }
    }
    if (offset > UINT32_MAX) {
        return false;
    }
    prefix[4] = 0x80;
    audio_offset = static_cast<uint32_t>(offset);
    return true;
}
