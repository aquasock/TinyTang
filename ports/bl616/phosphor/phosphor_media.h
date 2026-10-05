// TinyTang: what the Phosphor app needs to know about a music file without the
// core: whether Phosphor plays it, how long it lasts, and how to show times.
//
// Pure logic over a file's name and its first bytes, so the host can test it
// (tools/tests/phosphor_media_test.cpp).  The core reports no duration (0x90
// reads 0), so it is read from the header where the header states it exactly:
// a WAV's data size and byte rate, a FLAC's STREAMINFO.  Every other format
// would need its frames walked, and is shown with its elapsed time only.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace phosphor_media {

// How much of the file the duration readers want.  A WAV's data chunk comes
// after its fmt chunk and whatever LIST or other chunks precede it; a FLAC's
// STREAMINFO is its first block, after any ID3v2 tag.  Four kilobytes covers
// ordinary files; a larger tag or chunk just means no duration.
constexpr size_t HEADER_BYTES = 4096;

inline char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

// Whether `name` ends in `.ext` (ext in lower case), ignoring case.
inline bool has_extension(const char *name, const char *ext)
{
    size_t n = 0;
    while (name[n] != '\0') {
        ++n;
    }
    size_t e = 0;
    while (ext[e] != '\0') {
        ++e;
    }
    if (n < e + 2 || name[n - e - 1] != '.') {
        return false;
    }
    for (size_t i = 0; i < e; ++i) {
        if (lower(name[n - e + i]) != ext[i]) {
            return false;
        }
    }
    return true;
}

// The twelve formats the resident player decodes, by their usual extensions
// (tools/make_codec_corpus.sh makes one of each).
inline bool supported(const char *name)
{
    static const char *const extensions[] = {
        "wav", "wave", "flac", "mp2", "mp3", "ogg", "oga", "opus", "aac",
        "m4a", "mp4",  "wv",   "tta", "wma", "ac3",
    };
    for (const char *ext : extensions) {
        if (has_extension(name, ext)) {
            return true;
        }
    }
    return false;
}

inline uint32_t le32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

inline bool tag(const uint8_t *p, const char *t)
{
    return p[0] == static_cast<uint8_t>(t[0]) && p[1] == static_cast<uint8_t>(t[1]) &&
           p[2] == static_cast<uint8_t>(t[2]) && p[3] == static_cast<uint8_t>(t[3]);
}

// A WAV's length: its data chunk's size over the fmt chunk's byte rate.  A
// data size of 0 or 0xffffffff (a stream that never went back to fill it in)
// means "to the end of the file".
inline bool wav_duration_ms(const uint8_t *buf, size_t len, uint64_t file_size,
                            uint32_t *ms_out)
{
    if (len < 12 || !tag(buf, "RIFF") || !tag(buf + 8, "WAVE")) {
        return false;
    }
    uint32_t byte_rate = 0;
    size_t at = 12;
    while (at + 8 <= len) {
        const uint32_t size = le32(buf + at + 4);
        const size_t body = at + 8;
        if (tag(buf + at, "fmt ")) {
            if (size < 16 || body + 16 > len) {
                return false;
            }
            byte_rate = le32(buf + body + 8);
        } else if (tag(buf + at, "data")) {
            if (byte_rate == 0) {
                return false;
            }
            uint64_t bytes = size;
            if ((size == 0 || size == 0xffffffffu) && file_size > body) {
                bytes = file_size - body;
            }
            *ms_out = static_cast<uint32_t>(bytes * 1000 / byte_rate);
            return true;
        }
        at = body + size + (size & 1u);
    }
    return false;
}

// A FLAC's length: STREAMINFO's total samples over its sample rate.  Total
// samples of 0 means the encoder did not know it.
inline bool flac_duration_ms(const uint8_t *buf, size_t len, uint32_t *ms_out)
{
    size_t at = 0;
    if (len >= 10 && buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') {
        const uint32_t size = (static_cast<uint32_t>(buf[6] & 0x7f) << 21) |
                              (static_cast<uint32_t>(buf[7] & 0x7f) << 14) |
                              (static_cast<uint32_t>(buf[8] & 0x7f) << 7) |
                              static_cast<uint32_t>(buf[9] & 0x7f);
        at = 10 + size + ((buf[5] & 0x10) ? 10 : 0);
    }
    // "fLaC", then the STREAMINFO block's header (type 0) and its 34 bytes.
    if (at + 8 + 18 > len || !tag(buf + at, "fLaC") || (buf[at + 4] & 0x7f) != 0) {
        return false;
    }
    const uint8_t *info = buf + at + 8;
    const uint32_t rate = (static_cast<uint32_t>(info[10]) << 12) |
                          (static_cast<uint32_t>(info[11]) << 4) |
                          (static_cast<uint32_t>(info[12]) >> 4);
    const uint64_t total = (static_cast<uint64_t>(info[13] & 0x0f) << 32) |
                           (static_cast<uint64_t>(info[14]) << 24) |
                           (static_cast<uint64_t>(info[15]) << 16) |
                           (static_cast<uint64_t>(info[16]) << 8) |
                           static_cast<uint64_t>(info[17]);
    if (rate == 0 || total == 0) {
        return false;
    }
    *ms_out = static_cast<uint32_t>(total * 1000 / rate);
    return true;
}

// The duration of `name` from its first bytes, for the formats that state it.
inline bool duration_ms(const char *name, const uint8_t *buf, size_t len, uint64_t file_size,
                        uint32_t *ms_out)
{
    if (has_extension(name, "wav") || has_extension(name, "wave")) {
        return wav_duration_ms(buf, len, file_size, ms_out);
    }
    if (has_extension(name, "flac")) {
        return flac_duration_ms(buf, len, ms_out);
    }
    return false;
}

// Elapsed playback from the sink's sample count and the HDMI audio rate.
inline uint32_t elapsed_ms(uint32_t samples, uint32_t rate)
{
    return rate == 0 ? 0 : static_cast<uint32_t>(static_cast<uint64_t>(samples) * 1000 / rate);
}

// "m:ss", or "h:mm:ss" from an hour up.
inline void format_time(uint32_t ms, char *out, size_t size)
{
    const uint32_t s = ms / 1000;
    if (s >= 3600) {
        snprintf(out, size, "%lu:%02lu:%02lu", static_cast<unsigned long>(s / 3600),
                 static_cast<unsigned long>(s / 60 % 60), static_cast<unsigned long>(s % 60));
    } else {
        snprintf(out, size, "%lu:%02lu", static_cast<unsigned long>(s / 60),
                 static_cast<unsigned long>(s % 60));
    }
}

// The progress bar's percentage, held at 100 once elapsed passes the duration.
inline int percent(uint32_t elapsed, uint32_t duration)
{
    if (duration == 0) {
        return 0;
    }
    if (elapsed >= duration) {
        return 100;
    }
    return static_cast<int>(static_cast<uint64_t>(elapsed) * 100 / duration);
}

// The track after (step 1) or before (step -1) `current` in a list of `count`,
// or -1 past either end.  With no current track, the first.
inline int neighbour(int current, int count, int step)
{
    if (count <= 0) {
        return -1;
    }
    if (current < 0) {
        return 0;
    }
    const int next = current + step;
    return (next >= 0 && next < count) ? next : -1;
}

} // namespace phosphor_media
