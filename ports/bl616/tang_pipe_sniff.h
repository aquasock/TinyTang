// TinyTang — the overlay state a host sets through `fpgapipe`.
//
// SPDX-License-Identifier: MIT
//
// `fpgapipe` passes a host's bytes straight to the core.  A cartridge loader
// on the host (TinySNES's tools/load_snes_rom.py) ends its load with an overlay
// frame, `0x08 0`, to hand the screen to the game -- and the firmware never saw
// it, so tang_osd_shown() still said the overlay was up.  The first L / HOME
// tap afterwards then "hid" an overlay that was already hidden: the tap did
// nothing, and only the second one brought TinyDesk back.
//
// This watches the host's stream for that frame without changing a byte of it.
// The frame is the link's own (tang_fpga_link.h): 0xAA, a 16-bit big-endian
// length that counts the type byte, the type, then the payload.  The core takes
// bit 0 of an overlay frame's first payload byte, so that is what is reported.
//
// It is strict on purpose.  A host with its own protocol on the link --
// TinyNES's AE350 loader -- streams bytes that are not these frames, and a
// payload can hold any byte, so 0xAA 00 02 08 01 can turn up inside a ROM.  The
// stream is only believed while it has been well-formed frames from its first
// byte: a byte that is not 0xAA where a frame must start, or a length of 0,
// loses sync for the rest of the session and nothing more is reported.

#ifndef TANG_PIPE_SNIFF_H
#define TANG_PIPE_SNIFF_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t  state;
    uint8_t  type;
    uint8_t  first;     /* the next body byte is the frame's first payload byte */
    uint16_t left;      /* bytes of the current frame still to come */
} tang_pipe_sniff_t;

/* A new session: the next byte must start a frame. */
void tang_pipe_sniff_reset(tang_pipe_sniff_t *s);

/* Take `n` bytes the host sent.  Returns the overlay state set by the last
 * overlay frame among them -- 1 shown, 0 hidden -- or -1 if they set none. */
int tang_pipe_sniff(tang_pipe_sniff_t *s, const uint8_t *bytes, size_t n);

/* Whether sync has been lost for this session. */
int tang_pipe_sniff_lost(const tang_pipe_sniff_t *s);

#endif
