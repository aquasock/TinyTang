// TinyTang — host test for the fpgapipe overlay sniffer (tang_pipe_sniff.c).
//
// SPDX-License-Identifier: MIT

#include "tang_pipe_sniff.h"

#include <stdio.h>
#include <string.h>

static int fails;
static int checks;

static void check(int ok, const char *what)
{
    checks++;
    if (!ok) {
        fails++;
        printf("  FAIL: %s\n", what);
    }
}

/* One frame of the link: 0xAA, the length counting the type, the type, the
 * payload -- what tools/load_snes_rom.py's frame() writes. */
static size_t frame(uint8_t *out, uint8_t type, const uint8_t *payload, size_t n)
{
    const size_t len = n + 1;
    out[0] = 0xAA;
    out[1] = (uint8_t)(len >> 8);
    out[2] = (uint8_t)len;
    out[3] = type;
    memcpy(&out[4], payload, n);
    return n + 4;
}

/* Feed a stream in pieces of `step` bytes and return the last state reported,
 * so every split of a frame across reads is covered. */
static int feed(const uint8_t *bytes, size_t n, size_t step, tang_pipe_sniff_t *s)
{
    int last = -1;
    for (size_t i = 0; i < n; i += step) {
        const size_t k = n - i < step ? n - i : step;
        const int r = tang_pipe_sniff(s, &bytes[i], k);
        if (r >= 0) {
            last = r;
        }
    }
    return last;
}

static uint8_t stream[80000];

int main(void)
{
    tang_pipe_sniff_t s;
    uint8_t p[300];
    size_t n;

    /* A loader's whole session: reset on, a ROM chunk that is full of the
     * overlay frame's own bytes, reset off, then overlay off at the end. */
    n = 0;
    p[0] = 1;
    n += frame(&stream[n], 0x06, p, 1);
    for (size_t i = 0; i < 256; i++) {
        static const uint8_t fake[5] = { 0xAA, 0x00, 0x02, 0x08, 0x01 };
        p[i] = fake[i % 5];
    }
    n += frame(&stream[n], 0x07, p, 256);
    p[0] = 0;
    n += frame(&stream[n], 0x06, p, 1);
    p[0] = 0;
    n += frame(&stream[n], 0x08, p, 1);

    for (size_t step = 1; step <= 300; step++) {
        tang_pipe_sniff_reset(&s);
        const int r = feed(stream, n, step, &s);
        if (r != 0 || tang_pipe_sniff_lost(&s)) {
            printf("  at step %zu: %d\n", step, r);
            check(0, "a loader's closing overlay-off was not seen");
            break;
        }
    }
    checks++;

    /* Bit 0 of the first payload byte, as the core takes it. */
    tang_pipe_sniff_reset(&s);
    p[0] = 0x03;
    n = frame(stream, 0x08, p, 1);
    check(tang_pipe_sniff(&s, stream, n) == 1, "overlay on was not seen");
    p[0] = 0x02;
    n = frame(stream, 0x08, p, 1);
    check(tang_pipe_sniff(&s, stream, n) == 0, "bit 1 was taken as the overlay");

    /* A longer overlay frame: only its first payload byte counts. */
    tang_pipe_sniff_reset(&s);
    p[0] = 1; p[1] = 0; p[2] = 0;
    n = frame(stream, 0x08, p, 3);
    check(tang_pipe_sniff(&s, stream, n) == 1, "a later payload byte was taken");

    /* Two overlay frames in one read: the last one wins. */
    tang_pipe_sniff_reset(&s);
    p[0] = 1;
    n = frame(stream, 0x08, p, 1);
    p[0] = 0;
    n += frame(&stream[n], 0x08, p, 1);
    check(tang_pipe_sniff(&s, stream, n) == 0, "the earlier of two frames won");

    /* An overlay frame with no payload sets nothing, and keeps sync. */
    tang_pipe_sniff_reset(&s);
    n = frame(stream, 0x08, p, 0);
    p[0] = 1;
    n += frame(&stream[n], 0x08, p, 1);
    check(tang_pipe_sniff(&s, stream, n) == 1, "an empty overlay frame lost the next");

    /* A stream that does not start with a frame is someone else's protocol:
     * nothing in it is believed, including a real-looking frame later. */
    tang_pipe_sniff_reset(&s);
    stream[0] = 0x55;
    p[0] = 1;
    n = 1 + frame(&stream[1], 0x08, p, 1);
    check(tang_pipe_sniff(&s, stream, n) == -1, "a foreign stream was believed");
    check(tang_pipe_sniff_lost(&s), "a foreign stream kept sync");

    /* A zero length is no frame: sync is lost for the session. */
    tang_pipe_sniff_reset(&s);
    const uint8_t zero[3] = { 0xAA, 0x00, 0x00 };
    check(tang_pipe_sniff(&s, zero, 3) == -1, "a zero-length frame reported");
    check(tang_pipe_sniff_lost(&s), "a zero-length frame kept sync");
    n = frame(stream, 0x08, p, 1);
    check(tang_pipe_sniff(&s, stream, n) == -1, "sync came back after it was lost");

    /* A reset is a new session. */
    tang_pipe_sniff_reset(&s);
    check(tang_pipe_sniff(&s, stream, n) == 1, "a reset did not start a new session");

    /* Other frame types set nothing. */
    tang_pipe_sniff_reset(&s);
    p[0] = 1;
    n = frame(stream, 0x15, p, 1);
    check(tang_pipe_sniff(&s, stream, n) == -1, "another frame type was taken");

    printf("pipe sniff: %s (%d checks)\n", fails == 0 ? "PASS" : "FAIL", checks);
    return fails == 0 ? 0 : 1;
}
