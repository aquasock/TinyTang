// TinyTang — what the cartridge players' settings files have in common.
//
// /nes.ini and /gbc.ini have the shape of /tang.ini: section headers,
// `key = value`, several pairs to a line, `#` and `;` comments.  This is the
// reader both are walked with, and the [audio] section both carry.
//
// SPDX-License-Identifier: MIT

#ifndef TANG_SETTINGS_INI_H
#define TANG_SETTINGS_INI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One key of one section.  True when it was understood, which is what the
 * walk counts. */
typedef bool (*tang_settings_apply_fn)(void *ctx, const char *section,
                                       const char *key, const char *value);

/* Read a FatFS path and hand every pair to `apply`, adding the understood ones
 * to *keys.  False when the file could not be opened; a read that fails part
 * way keeps what was applied. */
bool tang_settings_walk(const char *path, tang_settings_apply_fn apply,
                        void *ctx, uint16_t *keys);

/* [audio]: one master volume for HDMI and the I2S2's Line Out together, since
 * both play the one stream, and a mute that keeps the volume where it is.
 * The player scales its samples by it (TinyNES core-log entry 50). */
#define TANG_VOLUME_MAX 100
#define TANG_VOLUME_STEP 5

typedef struct {
    uint8_t volume;                  /* 0 to 100 percent */
    uint8_t mute;                    /* 1 = silent, whatever the volume */
} tang_audio_t;

/* 100 percent and not muted, which is the level the players had before. */
void tang_audio_defaults(tang_audio_t *a);

/* `volume` and `mute` of an [audio] section. */
bool tang_audio_apply(tang_audio_t *a, const char *key, const char *value);

#ifdef __cplusplus
}
#endif

#endif /* TANG_SETTINGS_INI_H */
