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

#include "tang_snes_ini.h"

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

/* ---- the cartridge's battery save ----
 *
 * The core exposes cartridge SRAM through two frames, 0x18 to write a block
 * and 0x19 to read one back, and refuses both unless it is held in reset.  So
 * restoring a save happens inside a load, between the ROM drain and the reset
 * release, and saving one puts the load hold back on first.  The two halves
 * are named separately because a load restores and an exit saves. */

/* The largest save the core's 17-bit window holds. */
#define TANG_SNES_SAVE_MAX    (128u * 1024u)
/* Bytes per 0x19 read: the largest reply that fits the core's frame budget. */
#define TANG_SNES_SAVE_BLOCK  250u

/* HOME with SELECT, as the core reports it in the pad word: store the save
 * and go back to the desktop. */
#define TANG_SNES_PAD_EXIT    0x8000u

/* How big the cartridge's battery save is, from its internal header, or 0
 * when the cartridge has no battery.  `expansion_ram` is the byte at 0x7FBD
 * or 0xFFBD, which carries the real SRAM size for a SuperFX cartridge whose
 * own byte 0x18 reads zero. */
uint32_t tang_snes_save_size(const uint8_t header[TANG_SNES_HEADER_LEN],
                             uint8_t expansion_ram);

/* Where a cartridge's save lives: its name with .sav, under the snes folder
 * of the card's saves, as TinyNES and TinyGBC name theirs.  0 on success. */
int tang_snes_save_path(const char *cart_real, char *out, size_t size);

/* Store `size` bytes of cartridge SRAM at `path`, verified.  Holds the core in
 * reset for the readout and leaves it held, so the caller can load another
 * core under it; tang_snes_resume() puts the game back if that fails.  The
 * file is built as .part and renamed only once it has been read back and its
 * CRC-32 matches, so a doomed write never costs the previous save. */
int tang_snes_save_write(const char *path, uint32_t size,
                         char *why, size_t why_len);

/* Send one player's pad map to the core, frame 0x1A.  The pad is decoded in
 * fabric -- the Wii Classic Controller never reaches this chip -- so a
 * remapping can only be honoured by the core, and this is the only way to get
 * one there.  Takes the link itself; 0 on success. */
int tang_snes_apply_pad_map(const uint8_t map[TANG_SNES_BUTTONS]);

/* Send the picture's shape to the core, frame 0x1B.  Takes the link itself;
 * 0 on success. */
int tang_snes_apply_video(uint8_t aspect, uint8_t overscan);

/* Send the master volume to the core, frame 0x1C, as the gain tang_audio_gain
 * turns this player's volume and mute into.  One gain for both outputs: the
 * core applies it between the S-DSP and HDMI and the Pmod together.  Takes the
 * link itself; 0 on success. */
int tang_snes_apply_audio(const tang_audio_t *audio);

/* Release the load hold that tang_snes_save_write took. */
void tang_snes_resume(void);

/* Put `path` back into the cartridge SRAM.  Only meaningful while the core is
 * held in reset; tang_snes_load calls it itself, so this is for a caller that
 * holds its own.  Returns 0 when the bytes went in, TANG_SNES_RESTORE_NONE
 * when there was no file (a first run -- the freshly configured core starts
 * with an empty array, so this is not a failure), or 1 with the reason in
 * `why`. */
#define TANG_SNES_RESTORE_NONE 2
int tang_snes_save_restore(const char *path, uint32_t size,
                           char *why, size_t why_len);

/* The save a cartridge keeps, worked out from its own file: its size in
 * `*size` and the file it lives in through `path`.  Returns 0 when the
 * cartridge has a battery, 1 when it does not or the file will not open, so a
 * caller can skip the whole exit path for a cartridge that has nowhere to put
 * a save.  Reads the card, so it is called on the way out rather than held. */
int tang_snes_save_for(const char *cart_real, uint32_t *size,
                       char *path, size_t path_size);

#ifdef __cplusplus
}
#endif

#endif /* TANG_SNES_LOAD_H */
