// TinyTang — stream a GBA cartridge from the card into the TinyGBA core.
//
// TinyGBA has no CPU of its own to read the card, so the BL616 does what
// `gbaload` does over the console: hold the core in reset, stream the image,
// work out the cartridge's save hardware from its own marker string, configure
// it, put any stored save back, then release reset and let the cartridge run.
//
// This exists as an entry point for the same reason `tang_snes_load()` does:
// `cmd_gbaload` resolves its argument and then calls this, so the shell command
// and the desk app cannot drift apart.  For GBA the implementation deliberately
// stays in `tang_fpga_uart.c` beside the save helpers it shares with `gbasave`,
// rather than moving to its own file, because this load path is the one the
// project has proved on hardware most often and relocating it would be risk
// without benefit.
//
// SPDX-License-Identifier: MIT
#ifndef TANG_GBA_LOAD_H
#define TANG_GBA_LOAD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Save hardware, as the core's `config_backup_type` numbers them
 * (fpga/gba/src/memory/gba_memory.sv).  These are what a cartridge's marker
 * string resolves to, and the numbers are a wire protocol: the loader sends the
 * value as the first byte of its configuration stage, so they must match the
 * core's own constants rather than be chosen here. */
#define TANG_GBA_BACKUP_NONE      0u
#define TANG_GBA_BACKUP_FLASH512K 1u
#define TANG_GBA_BACKUP_FLASH1M   2u
#define TANG_GBA_BACKUP_SRAM      3u
#define TANG_GBA_BACKUP_EEPROM    4u

/* The largest image the core's 25-bit cartridge window maps. */
#define TANG_GBA_MAX_IMAGE (32u * 1024u * 1024u)

/* A GBA header is 0xC0 bytes and carries a fixed 0x96 at 0xB2.  One read
 * settles whether a file is a cartridge at all; the BIOS checks the Nintendo
 * logo itself, so validating that here would only repeat the machine's work. */
#define TANG_GBA_HEADER_MIN   0xC0u
#define TANG_GBA_HEADER_FIXED 0xB2u
#define TANG_GBA_HEADER_VALUE 0x96u

/* Load the cartridge at `real` (a FatFS path such as
 * "/sd/root/media/gba/game.gba") into the running TinyGBA core and start it.
 * `label` is the name the caller used, for messages.  0 on success; otherwise
 * non-zero with the reason in `why`, which is also set to the last progress
 * line on success so a window can show it.
 *
 * Blocks for the whole stream -- an 8 MiB image takes a few seconds -- so it is
 * called from a task, never from the desktop's own loop.  The overlay is hidden
 * before the core is released, so the game is what appears, not the desktop. */
int tang_gba_load(const char *label, const char *real, char *why, size_t why_len);

#ifdef __cplusplus
}
#endif

#endif /* TANG_GBA_LOAD_H */
