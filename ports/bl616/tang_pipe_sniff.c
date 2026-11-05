// TinyTang — the overlay state a host sets through `fpgapipe`.
// See tang_pipe_sniff.h for why, and why it gives up rather than resyncs.
//
// SPDX-License-Identifier: MIT

#include "tang_pipe_sniff.h"

#define FRAME_MAGIC   0xAAu
#define FRAME_OVERLAY 0x08u

enum {
    SNIFF_MAGIC,
    SNIFF_LEN_HI,
    SNIFF_LEN_LO,
    SNIFF_TYPE,
    SNIFF_BODY,
    SNIFF_LOST,
};

void tang_pipe_sniff_reset(tang_pipe_sniff_t *s)
{
    s->state = SNIFF_MAGIC;
    s->type = 0;
    s->first = 0;
    s->left = 0;
}

int tang_pipe_sniff_lost(const tang_pipe_sniff_t *s)
{
    return s->state == SNIFF_LOST;
}

int tang_pipe_sniff(tang_pipe_sniff_t *s, const uint8_t *bytes, size_t n)
{
    int seen = -1;

    for (size_t i = 0; i < n && s->state != SNIFF_LOST; i++) {
        const uint8_t b = bytes[i];
        switch (s->state) {
        case SNIFF_MAGIC:
            s->state = b == FRAME_MAGIC ? SNIFF_LEN_HI : SNIFF_LOST;
            break;
        case SNIFF_LEN_HI:
            s->left = (uint16_t)((uint16_t)b << 8);
            s->state = SNIFF_LEN_LO;
            break;
        case SNIFF_LEN_LO:
            s->left |= b;
            /* The length counts the type byte, so 0 is no frame at all. */
            s->state = s->left == 0 ? SNIFF_LOST : SNIFF_TYPE;
            break;
        case SNIFF_TYPE:
            s->type = b;
            s->first = 1;
            s->left--;
            s->state = s->left == 0 ? SNIFF_MAGIC : SNIFF_BODY;
            break;
        case SNIFF_BODY:
            /* The first payload byte is the one the core acts on. */
            if (s->first && s->type == FRAME_OVERLAY) {
                seen = b & 1u;
            }
            s->first = 0;
            s->left--;
            if (s->left == 0) {
                s->state = SNIFF_MAGIC;
            }
            break;
        default:
            break;
        }
    }
    return seen;
}
