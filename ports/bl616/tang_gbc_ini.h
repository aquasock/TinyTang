// TinyGBC — the settings file, /tinygbc.ini at the card root.
//
// SPDX-License-Identifier: MIT

#ifndef TANG_GBC_INI_H
#define TANG_GBC_INI_H

#include <stdbool.h>
#include <stdint.h>

#include "tang_settings_ini.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The FatFS path, and the one the user sees. */
#define TANG_GBC_INI_REAL   "/sd/tinygbc.ini"
#define TANG_GBC_INI_SHOWN  "/tinygbc.ini"

#define TANG_GBC_BUTTONS 8

/* The Game Boy's buttons in the order the player's mapping array holds them,
 * which is the core's own joypad order (TinyGBC
 * firmware/include/tinygbc_pad.h, pad_joypad[]). */
enum {
    TANG_GBC_B, TANG_GBC_A, TANG_GBC_SELECT, TANG_GBC_START,
    TANG_GBC_UP, TANG_GBC_DOWN, TANG_GBC_LEFT, TANG_GBC_RIGHT,
};

/* The controller buttons a Game Boy button may answer to, by index, in the
 * order the player's pad_source[] holds them.  It is the whole measured
 * Classic word bar HOME, which is absent on purpose: it swaps between the game
 * and TinyDesk, so a game button bound to it would bring the desktop up with
 * every press.  The two lists change together. */
enum {
    TANG_GBC_CTL_A, TANG_GBC_CTL_B, TANG_GBC_CTL_START, TANG_GBC_CTL_SELECT,
    TANG_GBC_CTL_UP, TANG_GBC_CTL_DOWN, TANG_GBC_CTL_LEFT, TANG_GBC_CTL_RIGHT,
    TANG_GBC_CTL_X, TANG_GBC_CTL_Y, TANG_GBC_CTL_L, TANG_GBC_CTL_R,
    TANG_GBC_CTL_ZL, TANG_GBC_CTL_ZR,
    TANG_GBC_CTL_COUNT,
};

typedef struct {
    uint8_t  pad_map[TANG_GBC_BUTTONS]; /* controller button per Game Boy button */
    tang_audio_t audio;              /* [audio]: the master volume and mute */
    bool     found;                  /* the file was there to be read */
    uint16_t keys;                   /* how many of its keys were understood */
} tang_gbc_settings_t;

/* The defaults, which are what the player did before this file existed, and
 * the mapping alone of them, which is what the Input page's Restore wants:
 * resetting the buttons must not move the volume. */
void tang_gbc_defaults(tang_gbc_settings_t *s);
void tang_gbc_default_map(uint8_t *map);

/* Read a FatFS path.  Returns false when it could not be opened, which leaves
 * the defaults: a card with no /tinygbc.ini is the usual case, not an error. */
bool tang_gbc_load(tang_gbc_settings_t *s, const char *path);

const char *tang_gbc_pad_name(int button);
const char *tang_gbc_button_name(int button);

#ifdef __cplusplus
}
#endif

#endif /* TANG_GBC_INI_H */
