/*
 * Host test for the core's reply-frame decoder.
 *
 * This is the change I would least like to discover was wrong on hardware.  It
 * used to live inside tang_fpga_uart.c, which needs a UART, an interrupt and a
 * card to run at all, so it shipped unexercised.  Extracted, it runs here.
 *
 * The case that matters most is #5: a reader waiting for the core id must not
 * lose it to a joypad report that happened to arrive first.  That is the bug
 * the extraction exists to prevent, and it is written down as a test rather
 * than trusted to a comment.
 *
 *   tools/tests/test_frames.sh
 */
#include <stdio.h>
#include <string.h>

#include "fpga_frames.h"

static int fails;
static int checks;

static void check_int(const char *what, long got, long want)
{
    checks++;
    if (got != want) {
        fails++;
        printf("  FAIL: %s (got %ld, want %ld)\n", what, got, want);
    }
}

static void clear(void)
{
    fpga_frames_reset();
}

/* Push one frame in the wire format: 0xAA len_hi len_lo type payload... */
static void put_frame(uint8_t type, const uint8_t *payload, unsigned len)
{
    const unsigned wire = len + 1;          /* the type byte is counted */
    fpga_frames_push(FPGA_FRAME_MAGIC);
    fpga_frames_push((uint8_t)(wire >> 8));
    fpga_frames_push((uint8_t)(wire & 0xFFu));
    fpga_frames_push(type);
    for (unsigned i = 0; i < len; i++) {
        fpga_frames_push(payload[i]);
    }
}

int main(void)
{
    uint8_t buf[512];
    uint8_t id;

    /* 1. A frame comes back exactly as it went in. */
    clear();
    {
        const uint8_t body[3] = { 0x11, 0x22, 0x33 };
        put_frame(0x10, body, 3);
        check_int("length", fpga_frames_take(0x10, buf, sizeof(buf)), 3);
        check_int("byte 0", buf[0], 0x11);
        check_int("byte 2", buf[2], 0x33);
    }

    /* 2. A header-only frame is a frame of length zero, not an absence. */
    clear();
    put_frame(0x01, NULL, 0);
    check_int("header-only", fpga_frames_take(0x01, buf, sizeof(buf)), 0);

    /* 3. Bytes that cannot start a frame are ignored. */
    clear();
    fpga_frames_push(0x00);
    fpga_frames_push(0x55);
    {
        const uint8_t body[2] = { 0xDE, 0xAD };
        put_frame(0x02, body, 2);
        check_int("decoded after noise", fpga_frames_take(0x02, buf, sizeof(buf)), 2);
        check_int("noise did not corrupt it", buf[0], 0xDE);
    }

    /* 3b. A limitation written down instead of discovered later.
     *
     * 0xAA in the stream *is* a header start, and if the bytes after it happen
     * to look like a plausible length the reader will believe them and swallow
     * whatever follows.  Without a checksum that is not recoverable in-stream,
     * and it is inherited from the protocol, not introduced here.  It is why
     * the link drains before every send: an exchange starts from a known place
     * rather than from wherever the last one ended.  What a drain buys is
     * tested in #10. */

    /* 4. A length the protocol cannot carry is noise, and the reader recovers
     *    rather than swallowing everything after it. */
    clear();
    fpga_frames_push(FPGA_FRAME_MAGIC);
    fpga_frames_push(0x09);          /* >= 8: not a frame */
    fpga_frames_push(0xFF);
    {
        const uint8_t body[1] = { 0x42 };
        put_frame(0x03, body, 1);
        check_int("recovered from a bad length", fpga_frames_take(0x03, buf, sizeof(buf)), 1);
        check_int("and the body is right", buf[0], 0x42);
    }

    /* 5. THE POINT: two readers, two types, neither destroys the other.
     *
     * A joypad report arrives first; a waiter wants the core id.  Before the
     * extraction the id would have been thrown away on the way past. */
    clear();
    {
        const uint8_t joy[4] = { 0x00, 0x10, 0x00, 0x00 };
        const uint8_t core[1] = { 0x50 };
        put_frame(FPGA_RESP_JOYPAD, joy, 4);
        put_frame(FPGA_RESP_CORE_ID, core, 1);

        check_int("the id survives a joypad report", 
                  fpga_frames_take(FPGA_RESP_CORE_ID, &id, 1), 1);
        check_int("and it is the id", id, 0x50);
        check_int("the joypad frame is still there",
                  fpga_frames_take(FPGA_RESP_JOYPAD, buf, sizeof(buf)), 4);
    }

    /* 6. The joypad state is sticky, not consumed, and queued copies do not
     *    crowd the queue. */
    clear();
    {
        const uint8_t joy[4] = { 0x01, 0x00, 0x00, 0x02 };   /* A, and joy2 B */
        uint16_t j1 = 0, j2 = 0;
        put_frame(FPGA_RESP_JOYPAD, joy, 4);
        fpga_frames_joypad(&j1, &j2);
        check_int("joy1 cached", j1, 0x0100);
        check_int("joy2 cached", j2, 0x0002);
        check_int("joypad seen", fpga_frames_joypad_seen(), 1);

        /* Reading it does not consume the state: held is held. */
        fpga_frames_joypad(&j1, &j2);
        check_int("state is sticky", j1, 0x0100);

        /* And the queued copy was dropped, so a reply arriving now is not
         * pushed out by joypad traffic. */
        check_int("queued joypad frame was purged",
                  fpga_frames_take(FPGA_RESP_JOYPAD, buf, sizeof(buf)), FPGA_FRAMES_NONE);
        {
            const uint8_t core[1] = { 0x51 };
            put_frame(FPGA_RESP_CORE_ID, core, 1);
            check_int("a reply still gets through",
                      fpga_frames_take(FPGA_RESP_CORE_ID, &id, 1), 1);
        }
    }

    /* 7. Asking for what is not there is an absence, not an error. */
    clear();
    check_int("absent type", fpga_frames_take(0x77, buf, sizeof(buf)), FPGA_FRAMES_NONE);

    /* 8. A frame that will not fit is refused and dropped, not truncated. */
    clear();
    {
        uint8_t big[64];
        memset(big, 0xA5, sizeof(big));
        put_frame(0x10, big, sizeof(big));
        check_int("refused when it does not fit",
                  fpga_frames_take(0x10, buf, 8), FPGA_FRAMES_REFUSED);
        check_int("and it was dropped, not left to be found again",
                  fpga_frames_take(0x10, buf, sizeof(buf)), FPGA_FRAMES_NONE);
    }

    /* 9. When the queue overflows the newest survives: a burst must not hide
     *    the reply that just arrived. */
    clear();
    {
        const uint8_t core[1] = { 0x5A };
        for (int i = 0; i < (int)FPGA_FRAMES_MAX + 4; i++) {
            const uint8_t junk[1] = { (uint8_t)i };
            put_frame(0x20, junk, 1);
        }
        put_frame(FPGA_RESP_CORE_ID, core, 1);
        check_int("the newest frame survives an overflow",
                  fpga_frames_take(FPGA_RESP_CORE_ID, &id, 1), 1);
        check_int("and it is the newest", id, 0x5A);
    }

    /* 10. A reset discards a half-seen frame rather than completing it out of
     *     the next one's bytes. */
    clear();
    fpga_frames_push(FPGA_FRAME_MAGIC);
    fpga_frames_push(0x00);
    fpga_frames_push(0x05);          /* five bytes promised, none delivered */
    fpga_frames_push(0xAA);          /* the reset happens here */
    clear();
    {
        const uint8_t core[1] = { 0x50 };
        put_frame(FPGA_RESP_CORE_ID, core, 1);
        check_int("a clean frame after a reset", fpga_frames_take(FPGA_RESP_CORE_ID, &id, 1), 1);
        check_int("with the right body", id, 0x50);
    }

    printf("fpga_frames: %s (%d checks)\n", fails == 0 ? "PASS" : "FAIL", checks);
    return fails == 0 ? 0 : 1;
}
