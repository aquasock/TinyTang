// TinyGBA — the settings file, /tinygba.ini at the card root.
//
// The same shape as /tinysnes.ini, read by ports/bl616/tang_settings_ini.c:
// section headers, `key = value`, several pairs to a line, `#` and `;`
// comments.  A missing file, or a key missing from it, is the default, so
// dropping one on the card is safe and an old card keeps working.
//
// Only [input] exists: the pad map.  The Video and Audio pages are still
// placeholders because the core has no frame for either, and a section
// nothing reads is worse than an absent one.
//
// The map runs the other way round from TinySNES's.  There each console button
// names the one controller button that drives it; here each CONTROLLER button
// names the GBA key it presses.  The GBA has ten keys and the Classic fourteen
// buttons, and the mapping every build has had puts two buttons on one key --
// X with A, Y with B, ZL with L, ZR with R -- which TinySNES's direction
// cannot say.  This one can, and it is also what the core applies
// (tinygba_pads.sv in the TinyGBA repository).
//
// SPDX-License-Identifier: MIT

#ifndef TANG_GBA_INI_H
#define TANG_GBA_INI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The FatFS path, and the one the user sees. */
#define TANG_GBA_INI_REAL   "/sd/tinygba.ini"
#define TANG_GBA_INI_SHOWN  "/tinygba.ini"

/* The controller's buttons, in the core's source order: source i is the
 * selector in bits 4i+3..4i of frame 0x1A's map (tinygba_pads.sv).  HOME is
 * not here: it is the desk and the exit, never a GBA key.  The 8BitDo
 * Bluetooth pad has no ZL or ZR, so those two rows only move the wired
 * Classic's. */
#define TANG_GBA_SOURCES 14

enum {
    TANG_GBA_SRC_A, TANG_GBA_SRC_B, TANG_GBA_SRC_X, TANG_GBA_SRC_Y,
    TANG_GBA_SRC_L, TANG_GBA_SRC_R, TANG_GBA_SRC_ZL, TANG_GBA_SRC_ZR,
    TANG_GBA_SRC_PLUS, TANG_GBA_SRC_MINUS,
    TANG_GBA_SRC_UP, TANG_GBA_SRC_DOWN, TANG_GBA_SRC_LEFT, TANG_GBA_SRC_RIGHT,
};

/* The GBA's keys, the values a selector takes.  TANG_GBA_KEY_NONE presses
 * nothing, which is how a player removes a button rather than moving it; the
 * core treats every value from 10 up the same way. */
#define TANG_GBA_KEYS 10
#define TANG_GBA_KEY_NONE 10

enum {
    TANG_GBA_KEY_A, TANG_GBA_KEY_B, TANG_GBA_KEY_L, TANG_GBA_KEY_R,
    TANG_GBA_KEY_START, TANG_GBA_KEY_SELECT,
    TANG_GBA_KEY_UP, TANG_GBA_KEY_DOWN, TANG_GBA_KEY_LEFT, TANG_GBA_KEY_RIGHT,
};

typedef struct {
    /* The GBA key each controller button presses, in TANG_GBA_SOURCES order. */
    uint8_t map[TANG_GBA_SOURCES];
    uint8_t version;
    bool    found;          /* the file was there to be read */
    uint16_t keys;          /* how many of its keys were understood */
} tang_gba_settings_t;

/* The defaults: the mapping fixed in fabric before the map existed, which the
 * core also holds out of reset.  An absent file therefore changes nothing. */
void tang_gba_defaults(tang_gba_settings_t *s);
void tang_gba_default_map(uint8_t map[TANG_GBA_SOURCES]);

/* Read a FatFS path.  False when it could not be read at all; an absent file
 * leaves the defaults in place.  Unknown keys and sections are ignored rather
 * than refused, so a newer file on an older firmware still starts. */
bool tang_gba_settings_load(tang_gba_settings_t *s, const char *path);

/* The names a player sees: a controller button ("ZL", "+") and a GBA key
 * ("START", or "none").  Both return "?" out of range rather than a plausible
 * wrong answer. */
const char *tang_gba_source_name(int source);
const char *tang_gba_key_name(int key);

/* The map as frame 0x1A carries it: fourteen nibbles, source i in bits
 * 4i+3..4i, and anything past the last key sent as none. */
uint64_t tang_gba_map_word(const uint8_t map[TANG_GBA_SOURCES]);

/* The word as the frame's seven bytes, source 13's nibble at the top of the
 * first, the order iosys_bl616.v shifts them in. */
void tang_gba_map_bytes(const uint8_t map[TANG_GBA_SOURCES], uint8_t out[7]);

/* Write the file.  Returns 0, or -1 with the card's reason left in `why`.
 * Through a temporary and a rename, as tang_snes_settings_save() does, so a
 * card that fills up cannot cost a player their bindings. */
int tang_gba_settings_save(const tang_gba_settings_t *s, const char *path,
                           char *why, size_t why_len);

#ifdef __cplusplus
}
#endif

#endif /* TANG_GBA_INI_H */
