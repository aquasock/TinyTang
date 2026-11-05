// TinyTang — stream a SNES cartridge from the card into the TinySNES core.
//
// TinySNES has no CPU of its own to read the card, so the BL616 does what
// TinySNES's tools/load_snes_rom.py does over fpgapipe: hold the core in
// reset, send the 64-byte internal header the core maps the cartridge by,
// send the image from its first byte, wait for the tail to reach SDRAM, then
// release reset.  The overlay is hidden before any of that, so the core's
// colour bars show while the cartridge goes in.  `snesload` and the TinySNES desk app
// both call tang_snes_load(), so the two cannot drift apart.
//
// SPDX-License-Identifier: MIT
#ifndef TANG_SNES_LOAD_H
#define TANG_SNES_LOAD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The core's loader address is 23 bits wide, so 8 MiB is the most it maps. */
#define TANG_SNES_MAX_IMAGE  (8u * 1024u * 1024u)
#define TANG_SNES_HEADER_LEN 64u
#define TANG_SNES_LOROM_HDR  0x7FC0u
#define TANG_SNES_HIROM_HDR  0xFFC0u

/* The bytes a 512-byte copier header adds in front of the image, or 0. */
uint32_t tang_snes_copier_skip(uint32_t file_size);

/* How much the 64 bytes at a candidate offset look like an internal header:
 * checksum and complement agree (3), a known map mode (2), a plausible ROM
 * size (1), a printable start of the title (1).  0 means nothing matched. */
int tang_snes_header_score(const uint8_t header[TANG_SNES_HEADER_LEN]);

/* Which header the core gets, from the two candidates' scores: HiROM only if
 * it scores higher, LoROM otherwise, and 0 when neither scores at all.  A
 * score of -1 marks a candidate the image is too short to hold. */
uint32_t tang_snes_pick_header(int lorom_score, int hirom_score);

/* Load the cartridge at `real` (a FatFS path, /sd/...) into the running
 * TinySNES core and start it.  0 on success; otherwise non-zero with the
 * reason in `why`.  On success `why` says what went in. */
int tang_snes_load(const char *real, char *why, size_t why_len);

#ifdef __cplusplus
}
#endif

#endif /* TANG_SNES_LOAD_H */
