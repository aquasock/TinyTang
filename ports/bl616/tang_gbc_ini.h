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

typedef struct {
    tang_audio_t audio;              /* [audio]: the master volume and mute */
    bool     found;                  /* the file was there to be read */
    uint16_t keys;                   /* how many of its keys were understood */
} tang_gbc_settings_t;

/* The defaults, which are what the player did before this file existed. */
void tang_gbc_defaults(tang_gbc_settings_t *s);

/* Read a FatFS path.  Returns false when it could not be opened, which leaves
 * the defaults: a card with no /tinygbc.ini is the usual case, not an error. */
bool tang_gbc_load(tang_gbc_settings_t *s, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* TANG_GBC_INI_H */
