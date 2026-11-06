// TinySNES — the settings file, /tinysnes.ini at the card root.
//
// The same shape as /tinynes.ini and /tinygbc.ini, read by
// ports/bl616/tang_settings_ini.c: section headers, `key = value`, several
// pairs to a line, `#` and `;` comments.  A missing file, or a key missing from
// it, is the default, so dropping one on the card is safe and an old card keeps
// working.
//
// Only [input] exists so far.  The Video and Audio pages are still placeholders
// in the desktop window, and a settings file that carried keys nothing reads
// would be worse than one that does not.
//
// SPDX-License-Identifier: MIT

#ifndef TANG_SNES_INI_H
#define TANG_SNES_INI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The FatFS path, and the one the user sees. */
#define TANG_SNES_INI_REAL   "/sd/tinysnes.ini"
#define TANG_SNES_INI_SHOWN  "/tinysnes.ini"

/* The twelve buttons of the SNES pad word, in the order the core carries them:
 * bit 11 first.  This ordering is the core's, not a choice of this module --
 * pad_remap.v indexes the same way, and the frame that carries the map packs
 * them in this order too. */
#define TANG_SNES_BUTTONS 12

enum {
    TANG_SNES_R, TANG_SNES_L, TANG_SNES_X, TANG_SNES_A,
    TANG_SNES_RT, TANG_SNES_LT, TANG_SNES_DN, TANG_SNES_UP,
    TANG_SNES_START, TANG_SNES_SELECT, TANG_SNES_Y, TANG_SNES_B,
};

/* What a button may answer to.  The Classic Controller's own buttons, named
 * the way a player would say them.  A mapping stores the index of one of
 * these; TANG_SNES_SRC_NONE means the button produces nothing, which is how a
 * player removes a binding rather than moving it.
 *
 * The names are the twelve the pad word already carries, so a controller
 * button is a perfectly general source: offering only these keeps a mapping to
 * things the fabric's decoder can actually produce. */
#define TANG_SNES_SRC_NONE 12
#define TANG_SNES_SOURCES 13

typedef struct {
    /* Controller source per SNES button, in TANG_SNES_BUTTONS order. */
    uint8_t p1[TANG_SNES_BUTTONS];
    uint8_t p2[TANG_SNES_BUTTONS];
    uint8_t version;
    bool    found;          /* the file was there to be read */
    uint16_t keys;          /* how many of its keys were understood */
} tang_snes_settings_t;

/* The defaults: every button on its own source, which is the wiring every
 * build before this file had.  An absent file therefore changes nothing. */
void tang_snes_defaults(tang_snes_settings_t *s);

/* Read a FatFS path.  False when it could not be read at all; an absent file
 * leaves the defaults in place.  Unknown keys and sections are ignored rather
 * than refused, so a newer file on an older firmware still starts. */
bool tang_snes_settings_load(tang_snes_settings_t *s, const char *path);

/* The name a player would use for a button of the pad, and for a source.  Both
 * return "?" out of range rather than a plausible wrong answer. */
const char *tang_snes_button_name(int button);
const char *tang_snes_source_name(int source);

/* The map as the core's frame 0x1A carries it: twelve nibbles, button 11 in
 * the most significant.  Built from one player's array. */
uint64_t tang_snes_map_word(const uint8_t map[TANG_SNES_BUTTONS]);

/* Write the file.  Returns 0, or -1 with the card's reason left in `why`.
 *
 * The write goes through a temporary and a rename because FatFS has no atomic
 * replace: f_rename refuses a name already in use, so the old file is moved
 * aside first and only removed once the new one is in place.  A failure at any
 * step puts the old file back, so a card that fills up cannot cost a player
 * their bindings. */
int tang_snes_settings_save(const tang_snes_settings_t *s, const char *path,
                            char *why, size_t why_len);

#ifdef __cplusplus
}
#endif

#endif /* TANG_SNES_INI_H */
