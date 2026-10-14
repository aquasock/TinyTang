// TinyNES — the settings file, /nes.ini at the card root.
//
// SPDX-License-Identifier: MIT

#ifndef TANG_NES_INI_H
#define TANG_NES_INI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The FatFS path, and the one the user sees. */
#define TANG_NES_INI_REAL   "/sd/nes.ini"
#define TANG_NES_INI_SHOWN  "/nes.ini"

#define TANG_NES_BUTTONS 8

/* NES buttons, in the order the app lists them. */
enum {
    TANG_NES_A, TANG_NES_B, TANG_NES_SELECT, TANG_NES_START,
    TANG_NES_UP, TANG_NES_DOWN, TANG_NES_LEFT, TANG_NES_RIGHT,
};

/* Controller buttons a NES button may answer to, by index.  HOME is
 * deliberately absent: it swaps between the game and TinyDesk. */
enum {
    NES_CTL_A, NES_CTL_B, NES_CTL_X, NES_CTL_Y,
    NES_CTL_L, NES_CTL_R, NES_CTL_ZL, NES_CTL_ZR,
    NES_CTL_LSTICK, NES_CTL_RSTICK, NES_CTL_PLUS, NES_CTL_MINUS,
    NES_CTL_UP, NES_CTL_DOWN, NES_CTL_LEFT, NES_CTL_RIGHT,
    NES_CTL_COUNT,
};

enum { TANG_ASPECT_8_7, TANG_ASPECT_1_1, TANG_ASPECT_4_3 };
enum { TANG_CHANNELS_STEREO, TANG_CHANNELS_MONO };

typedef struct {
    uint8_t  p1[TANG_NES_BUTTONS];   /* controller button per NES button */
    uint8_t  p2[TANG_NES_BUTTONS];
    uint8_t  aspect;
    uint8_t  scale;
    uint8_t  scanlines;
    uint8_t  palette;                /* only FCEUmm's, for now */
    uint8_t  overscan;               /* 1 = hide the top and bottom lines */
    uint8_t  volume;                 /* 0 to 100 */
    uint8_t  mute;
    uint8_t  channels;
    uint8_t  version;
    bool     found;                  /* the file was there to be read */
    uint16_t keys;                   /* how many of its keys were understood */
} tang_nes_settings_t;

/* The defaults, which are what the firmware did before this file existed, so
 * an absent file changes nothing. */
void tang_nes_defaults(tang_nes_settings_t *s);

/* Read a FatFS path.  Returns false when it could not be read at all; an
 * absent file leaves the defaults in place.  Unknown keys and sections are
 * ignored rather than refused, so a newer file on an older firmware still
 * starts. */
bool tang_nes_load(tang_nes_settings_t *s, const char *path);

const char *tang_nes_pad_name(int button);
const char *tang_nes_button_name(int button);
const char *tang_nes_aspect_name(int aspect);

#ifdef __cplusplus
}
#endif

#endif /* TANG_NES_INI_H */
