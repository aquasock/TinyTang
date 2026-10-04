// TinyTang — decoding the core's reply frames.
//
// See fpga_frames.h for why this is a module of its own.  This file is the
// mechanism: a resynchronising state machine in, a small queue out.

#include "fpga_frames.h"

#include <string.h>

typedef struct {
    uint8_t  type;
    uint16_t len;                       /* the payload's real length */
    uint8_t  payload[FPGA_FRAMES_PAYLOAD_MAX];   /* as much of it as is kept */
} frame_t;

static frame_t  s_q[FPGA_FRAMES_MAX];
static unsigned s_q_count;

/* Where the decoder is in the byte stream.  Kept here rather than passed in,
 * because a frame can straddle any number of pushes. */
enum { RS_MAGIC, RS_LEN_HI, RS_LEN_LO, RS_TYPE, RS_PAYLOAD };

static int      s_state = RS_MAGIC;
static uint8_t  s_body[FPGA_FRAMES_PAYLOAD_MAX];
static unsigned s_len, s_got;
static uint8_t  s_type;

static uint16_t s_joy1, s_joy2;
static bool     s_joy_seen;

void fpga_frames_reset(void)
{
    s_state = RS_MAGIC;
    s_len = 0;
    s_got = 0;
    s_q_count = 0;
}

static void push_frame(uint8_t type, const uint8_t *payload, unsigned len)
{
    if (s_q_count >= FPGA_FRAMES_MAX) {
        /* Full: lose the oldest, so a burst cannot hide the newest reply. */
        memmove(&s_q[0], &s_q[1], sizeof(s_q[0]) * (FPGA_FRAMES_MAX - 1));
        s_q_count--;
    }
    frame_t *f = &s_q[s_q_count];
    f->type = type;
    f->len = (uint16_t)len;
    const unsigned keep = len > FPGA_FRAMES_PAYLOAD_MAX ? FPGA_FRAMES_PAYLOAD_MAX : len;
    if (keep != 0) {
        memcpy(f->payload, payload, keep);
    }
    s_q_count++;
}

void fpga_frames_push(uint8_t byte)
{
    switch (s_state) {
    case RS_MAGIC:
        if (byte == FPGA_FRAME_MAGIC) {
            s_state = RS_LEN_HI;
        }
        break;

    case RS_LEN_HI:
        s_len = (unsigned)byte << 8;
        /* A length this protocol cannot carry is noise, not a header: go back
         * to hunting rather than believe it and swallow the next 2048 bytes. */
        s_state = (byte < FPGA_LEN_HI_MAX) ? RS_LEN_LO : RS_MAGIC;
        break;

    case RS_LEN_LO:
        s_len |= byte;
        s_got = 0;
        s_state = (s_len >= 1) ? RS_TYPE : RS_MAGIC;
        break;

    case RS_TYPE:
        s_type = byte;
        if (s_len == 1) {               /* header-only frame */
            push_frame(s_type, NULL, 0);
            s_state = RS_MAGIC;
        } else {
            s_state = RS_PAYLOAD;
        }
        break;

    default: /* RS_PAYLOAD */
        if (s_got < FPGA_FRAMES_PAYLOAD_MAX) {
            s_body[s_got] = byte;
        }
        s_got++;
        if (s_got == s_len - 1) {
            /* Cache the pad before queueing, so a pointer reading it can never
             * see a frame the queue still holds. */
            if (s_type == FPGA_RESP_JOYPAD && s_got >= 4) {
                s_joy1 = (uint16_t)((s_body[0] << 8) | s_body[1]);
                s_joy2 = (uint16_t)((s_body[2] << 8) | s_body[3]);
                s_joy_seen = true;
            }
            push_frame(s_type, s_body, s_got);
            s_state = RS_MAGIC;
        }
        break;
    }
}

int fpga_frames_take(uint8_t type, uint8_t *out, size_t cap)
{
    unsigned found = FPGA_FRAMES_MAX;
    for (unsigned i = 0; i < s_q_count; i++) {
        if (s_q[i].type == type) {
            found = i;
            break;
        }
    }
    if (found == FPGA_FRAMES_MAX) {
        return FPGA_FRAMES_NONE;
    }

    /* A frame that does not fit is dropped, not truncated, and one longer than
     * can be held is refused outright rather than copied out of a short
     * buffer.  Half a reply is worse than none: it parses. */
    int rc = FPGA_FRAMES_REFUSED;
    const uint16_t len = s_q[found].len;
    if ((size_t)len <= cap && len <= FPGA_FRAMES_PAYLOAD_MAX) {
        if (len != 0) {
            memcpy(out, s_q[found].payload, len);
        }
        rc = (int)len;
    }

    for (unsigned i = found; i + 1 < s_q_count; i++) {
        s_q[i] = s_q[i + 1];
    }
    s_q_count--;
    return rc;
}

void fpga_frames_joypad(uint16_t *joy1, uint16_t *joy2)
{
    /* Cached joypad frames are redundant next to the state, and they would
     * otherwise crowd out a reply someone is waiting for. */
    unsigned w = 0;
    for (unsigned i = 0; i < s_q_count; i++) {
        if (s_q[i].type == FPGA_RESP_JOYPAD) {
            continue;
        }
        s_q[w++] = s_q[i];
    }
    s_q_count = w;

    *joy1 = s_joy1;
    *joy2 = s_joy2;
}

bool fpga_frames_joypad_seen(void)
{
    return s_joy_seen;
}
