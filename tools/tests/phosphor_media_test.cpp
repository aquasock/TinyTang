// TinyTang: host test for phosphor_media.h, which the Phosphor app uses to list
// playable files and to show a track's length and progress.

#include "phosphor_media.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

int failures;

void check(bool ok, const char *what)
{
    if (!ok) {
        printf("FAIL %s\n", what);
        ++failures;
    }
}

size_t put32(uint8_t *p, uint32_t v)
{
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
    return 4;
}

size_t put(uint8_t *p, const char *t)
{
    memcpy(p, t, 4);
    return 4;
}

// A canonical 16-bit stereo 44.1 kHz WAV header with `data_size` bytes of
// audio, optionally with a LIST chunk (odd-sized, so padded) before the data.
size_t make_wav(uint8_t *buf, uint32_t data_size, bool list)
{
    size_t n = 0;
    n += put(buf + n, "RIFF");
    n += put32(buf + n, 0);
    n += put(buf + n, "WAVE");
    n += put(buf + n, "fmt ");
    n += put32(buf + n, 16);
    const uint8_t fmt[16] = {1, 0, 2, 0, 0x44, 0xac, 0, 0, 0x10, 0xb1, 2, 0, 4, 0, 16, 0};
    memcpy(buf + n, fmt, sizeof(fmt));
    n += sizeof(fmt);
    if (list) {
        n += put(buf + n, "LIST");
        n += put32(buf + n, 5);
        memcpy(buf + n, "INFOx\0", 6);
        n += 6;
    }
    n += put(buf + n, "data");
    n += put32(buf + n, data_size);
    return n;
}

void test_wav()
{
    uint8_t buf[256];
    uint32_t ms = 0;
    size_t n = make_wav(buf, 176400 * 10, false);
    check(phosphor_media::wav_duration_ms(buf, n, 44 + 176400 * 10, &ms) && ms == 10000,
          "WAV: ten seconds");
    n = make_wav(buf, 176400 * 3 + 88200, true);
    check(phosphor_media::wav_duration_ms(buf, n, 0, &ms) && ms == 3500,
          "WAV: a padded LIST chunk before the data");
    n = make_wav(buf, 0xffffffffu, false);
    check(phosphor_media::wav_duration_ms(buf, n, 44 + 176400 * 2, &ms) && ms == 2000,
          "WAV: an unfilled data size runs to the end of the file");
    n = make_wav(buf, 1000, false);
    check(!phosphor_media::wav_duration_ms(buf, n - 6, 0, &ms),
          "WAV: no data chunk in the bytes read");
    memcpy(buf + 8, "AVI ", 4);
    check(!phosphor_media::wav_duration_ms(buf, n, 0, &ms), "WAV: not a WAVE");
    check(phosphor_media::duration_ms("Song.WAV", buf, 0, 0, &ms) == false,
          "WAV: no bytes, no duration");
}

// "fLaC" and a STREAMINFO block for `total` samples at `rate`.
size_t make_flac(uint8_t *buf, uint32_t rate, uint64_t total)
{
    size_t n = 0;
    n += put(buf + n, "fLaC");
    buf[n++] = 0x80;  // last block, type 0 (STREAMINFO)
    buf[n++] = 0;
    buf[n++] = 0;
    buf[n++] = 34;
    uint8_t *info = buf + n;
    memset(info, 0, 34);
    info[10] = static_cast<uint8_t>(rate >> 12);
    info[11] = static_cast<uint8_t>(rate >> 4);
    info[12] = static_cast<uint8_t>(((rate & 0x0f) << 4) | (1 << 1));  // 2 channels
    info[13] = static_cast<uint8_t>((15 << 4) | ((total >> 32) & 0x0f));  // 16 bits
    info[14] = static_cast<uint8_t>(total >> 24);
    info[15] = static_cast<uint8_t>(total >> 16);
    info[16] = static_cast<uint8_t>(total >> 8);
    info[17] = static_cast<uint8_t>(total);
    return n + 34;
}

void test_flac()
{
    uint8_t buf[512];
    uint32_t ms = 0;
    size_t n = make_flac(buf, 44100, 441000);
    check(phosphor_media::flac_duration_ms(buf, n, &ms) && ms == 10000, "FLAC: ten seconds");
    n = make_flac(buf, 96000, 96000ull * 3600 * 2);
    check(phosphor_media::flac_duration_ms(buf, n, &ms) && ms == 7200000,
          "FLAC: two hours at 96 kHz");
    n = make_flac(buf, 48000, 0x123456789ull);
    check(phosphor_media::flac_duration_ms(buf, n, &ms) &&
              ms == static_cast<uint32_t>(0x123456789ull * 1000 / 48000),
          "FLAC: all 36 bits of the sample count");
    n = make_flac(buf, 44100, 0);
    check(!phosphor_media::flac_duration_ms(buf, n, &ms), "FLAC: unknown length");

    // An ID3v2 tag of 100 bytes (syncsafe size) ahead of the stream.
    uint8_t tagged[512] = {'I', 'D', '3', 4, 0, 0, 0, 0, 0, 100};
    n = make_flac(tagged + 110, 44100, 88200);
    check(phosphor_media::flac_duration_ms(tagged, 110 + n, &ms) && ms == 2000,
          "FLAC: after an ID3v2 tag");
    check(!phosphor_media::flac_duration_ms(tagged, 110 + 20, &ms),
          "FLAC: STREAMINFO cut off");
    check(phosphor_media::duration_ms("a.Flac", tagged, 110 + n, 0, &ms) && ms == 2000,
          "duration_ms picks FLAC by extension");
    check(!phosphor_media::duration_ms("a.mp3", tagged, 110 + n, 0, &ms),
          "no duration for other formats");
}

void test_names()
{
    check(phosphor_media::supported("track.flac"), "flac");
    check(phosphor_media::supported("TRACK.MP3"), "upper case");
    check(phosphor_media::supported("a.b.opus"), "last extension counts");
    check(phosphor_media::supported("x.wv") && phosphor_media::supported("x.m4a") &&
              phosphor_media::supported("x.mp4") && phosphor_media::supported("x.ac3") &&
              phosphor_media::supported("x.tta") && phosphor_media::supported("x.wma") &&
              phosphor_media::supported("x.mp2") && phosphor_media::supported("x.ogg") &&
              phosphor_media::supported("x.wav"),
          "the corpus formats");
    check(!phosphor_media::supported("notes.txt"), "text");
    check(!phosphor_media::supported("flac"), "no dot");
    check(!phosphor_media::supported(".mp3"), "a bare extension is not a track");
    check(!phosphor_media::supported("x.mp33"), "longer extension");
}

void test_times()
{
    char text[16];
    phosphor_media::format_time(0, text, sizeof(text));
    check(strcmp(text, "0:00") == 0, "0:00");
    phosphor_media::format_time(59999, text, sizeof(text));
    check(strcmp(text, "0:59") == 0, "rounds down");
    phosphor_media::format_time(754000, text, sizeof(text));
    check(strcmp(text, "12:34") == 0, "12:34");
    phosphor_media::format_time(3723000, text, sizeof(text));
    check(strcmp(text, "1:02:03") == 0, "hours");

    check(phosphor_media::elapsed_ms(48000 * 90, 48000) == 90000, "elapsed");
    check(phosphor_media::elapsed_ms(4000000000u, 44100) == 90702947u, "elapsed, no overflow");
    check(phosphor_media::elapsed_ms(100, 0) == 0, "elapsed before the rate is known");

    check(phosphor_media::percent(0, 0) == 0, "no duration");
    check(phosphor_media::percent(5000, 10000) == 50, "half");
    check(phosphor_media::percent(12000, 10000) == 100, "held at 100");
    check(phosphor_media::percent(4000000000u, 4000000001u) == 99, "large, no overflow");
}

void test_neighbour()
{
    check(phosphor_media::neighbour(-1, 5, 1) == 0, "nothing playing: first");
    check(phosphor_media::neighbour(2, 5, 1) == 3, "next");
    check(phosphor_media::neighbour(4, 5, 1) == -1, "past the end");
    check(phosphor_media::neighbour(2, 5, -1) == 1, "previous");
    check(phosphor_media::neighbour(0, 5, -1) == -1, "before the start");
    check(phosphor_media::neighbour(0, 0, 1) == -1, "empty list");
}

} // namespace

int main()
{
    test_wav();
    test_flac();
    test_names();
    test_times();
    test_neighbour();
    if (failures != 0) {
        printf("phosphor_media: %d failures\n", failures);
        return EXIT_FAILURE;
    }
    printf("PASS phosphor_media names, durations and times\n");
    return 0;
}
