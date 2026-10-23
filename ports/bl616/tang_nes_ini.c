// TinyNES — reading /tinynes.ini, the settings file at the card root.
//
// The same shape as /tinytang.ini, read by ports/bl616/tang_settings_ini.c.  A
// missing file, or a key missing from it, is the default -- which is the
// behaviour the firmware had before this file existed, so dropping one on the
// card is safe and an old card keeps working.
//
// Read once per launch.  The app on the desktop is what writes it.
//
// SPDX-License-Identifier: MIT

#include "tang_nes_ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tang_settings_ini.h"

static const char *const s_pad_names[NES_CTL_COUNT] = {
    "b", "y", "up", "down", "left", "right", "select", "start",
};

static const char *const s_nes_names[TANG_NES_BUTTONS] = {
    "a", "b", "select", "start", "up", "down", "left", "right",
};

static const char *const s_palette_names[TANG_NES_PALETTES] = {
    "fceumm",
    "asqrealc", "restored-wii-vc", "wii-vc", "rgb", "yuv-v3",
    "unsaturated-final", "sony-cxa2025as-us", "pal", "bmf-final2",
    "bmf-final3", "smooth-fbx", "composite-direct-fbx", "pvm-style-d93-fbx",
    "ntsc-hardware-fbx", "nes-classic-fbx-fs", "nescap", "wavebeam",
    "digital-prime-fbx", "magnum-fbx", "smooth-v2-fbx", "nes-classic-fbx",
    "royaltea", "mugicha",
};

const char *tang_nes_pad_name(int button)
{
    if (button < 0 || button >= NES_CTL_COUNT) {
        return "?";
    }
    return s_pad_names[button];
}

const char *tang_nes_button_name(int button)
{
    if (button < 0 || button >= TANG_NES_BUTTONS) {
        return "?";
    }
    return s_nes_names[button];
}

const char *tang_nes_aspect_name(int aspect)
{
    switch (aspect) {
    case TANG_ASPECT_1_1: return "1:1";
    case TANG_ASPECT_4_3: return "4:3";
    default:              return "8:7";
    }
}

const char *tang_nes_palette_name(int palette)
{
    if (palette < 0 || palette >= TANG_NES_PALETTES) {
        return s_palette_names[0];
    }
    return s_palette_names[palette];
}

static int palette_index(const char *name)
{
    for (int i = 0; i < TANG_NES_PALETTES; i++) {
        if (strcmp(name, s_palette_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

void tang_nes_defaults(tang_nes_settings_t *s)
{
    static const uint8_t pad[TANG_NES_BUTTONS] = {
        NES_CTL_B, NES_CTL_Y, NES_CTL_SELECT, NES_CTL_START,
        NES_CTL_UP, NES_CTL_DOWN, NES_CTL_LEFT, NES_CTL_RIGHT,
    };

    memset(s, 0, sizeof *s);
    memcpy(s->p1, pad, sizeof pad);
    memcpy(s->p2, pad, sizeof pad);
    s->aspect = TANG_ASPECT_8_7;
    s->scale = 3;
    s->scanlines = 0;
    s->palette = 0;
    s->overscan = 1;
    tang_audio_defaults(&s->audio);
    s->version = 1;
}

static int pad_index(const char *name)
{
    for (int i = 0; i < NES_CTL_COUNT; i++) {
        if (strcmp(name, s_pad_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static int nes_index(const char *name)
{
    for (int i = 0; i < TANG_NES_BUTTONS; i++) {
        if (strcmp(name, s_nes_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

/* One key of one section.  Returns false for anything unrecognised, so the
 * caller's count reports how much of the file this firmware understood. */
static bool apply(void *ctx, const char *section, const char *key,
                  const char *value)
{
    tang_nes_settings_t *s = ctx;

    if (strcmp(section, "input") == 0) {
        uint8_t *map;
        int nes, pad;

        if (key[0] != 'p' || key[2] != '_') {
            return false;
        }
        if (key[1] == '1') {
            map = s->p1;
        } else if (key[1] == '2') {
            map = s->p2;
        } else {
            return false;
        }
        nes = nes_index(key + 3);
        pad = pad_index(value);
        if (nes < 0 || pad < 0) {
            return false;
        }
        map[nes] = (uint8_t)pad;
        return true;
    }
    if (strcmp(section, "video") == 0) {
        if (strcmp(key, "aspect") == 0) {
            s->aspect = strcmp(value, "1:1") == 0 ? TANG_ASPECT_1_1
                      : strcmp(value, "4:3") == 0 ? TANG_ASPECT_4_3
                                                  : TANG_ASPECT_8_7;
            return true;
        }
        if (strcmp(key, "scale") == 0) {
            const int v = atoi(value);
            s->scale = (uint8_t)(v < 1 ? 1 : v > 3 ? 3 : v);
            return true;
        }
        if (strcmp(key, "scanlines") == 0) {
            s->scanlines = strcmp(value, "on") == 0;
            return true;
        }
        if (strcmp(key, "palette") == 0) {
            /* A name this build does not know keeps the default and is not
             * counted, so a misspelt palette shows in the key count rather
             * than as a game that silently looks the same. */
            const int palette = palette_index(value);
            if (palette < 0) {
                return false;
            }
            s->palette = (uint8_t)palette;
            return true;
        }
        if (strcmp(key, "overscan") == 0) {
            s->overscan = strcmp(value, "show") != 0;
            return true;
        }
        return false;
    }
    /* [audio] came back as one master volume and a mute (core-log entry 20);
     * an old file's `channels` is not one of them and is not counted. */
    if (strcmp(section, "audio") == 0) {
        return tang_audio_apply(&s->audio, key, value);
    }
    if (strcmp(section, "tinynes") == 0) {
        if (strcmp(key, "version") == 0) {
            s->version = (uint8_t)atoi(value);
            return true;
        }
        return false;
    }
    return false;
}

bool tang_nes_load(tang_nes_settings_t *s, const char *path)
{
    tang_nes_defaults(s);
    s->found = tang_settings_walk(path, apply, s, &s->keys);
    return s->found;
}
