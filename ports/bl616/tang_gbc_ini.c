// TinyGBC — reading /tinygbc.ini, the settings file at the card root.
//
// The same shape as /tinynes.ini and read the same way
// (ports/bl616/tang_settings_ini.c).  It holds the button mapping and [audio],
// the two things the TinyGBC player takes from the host.  Read once per
// launch; the TinyGBC window on the desktop is what writes it.
//
// SPDX-License-Identifier: MIT

#include "tang_gbc_ini.h"

#include <string.h>

/* The Game Boy's buttons as the file names them, in pad_map[] order. */
static const char *const s_button_names[TANG_GBC_BUTTONS] = {
    "b", "a", "select", "start", "up", "down", "left", "right",
};

/* The Classic Controller's buttons as the file names them, in the player's
 * pad_source[] order.  HOME is not among them (see tang_gbc_ini.h). */
static const char *const s_pad_names[TANG_GBC_CTL_COUNT] = {
    "a", "b", "plus", "minus", "up", "down", "left", "right",
    "x", "y", "l", "r", "zl", "zr",
};

/* The picture's two settings as the file names them, in the player's order. */
static const char *const s_palette_names[TANG_GBC_PAL_COUNT] = {
    "off", "auto", "dmg", "pocket", "light", "gbc",
};

static const char *const s_colour_names[TANG_GBC_COLOUR_COUNT] = {
    "off", "accurate", "fast",
};

const char *tang_gbc_palette_name(int palette)
{
    if (palette < 0 || palette >= TANG_GBC_PAL_COUNT) {
        return "?";
    }
    return s_palette_names[palette];
}

const char *tang_gbc_colour_name(int colour)
{
    if (colour < 0 || colour >= TANG_GBC_COLOUR_COUNT) {
        return "?";
    }
    return s_colour_names[colour];
}

static int palette_index(const char *name)
{
    for (int i = 0; i < TANG_GBC_PAL_COUNT; i++) {
        if (strcmp(name, s_palette_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static int colour_index(const char *name)
{
    for (int i = 0; i < TANG_GBC_COLOUR_COUNT; i++) {
        if (strcmp(name, s_colour_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

const char *tang_gbc_pad_name(int button)
{
    if (button < 0 || button >= TANG_GBC_CTL_COUNT) {
        return "?";
    }
    return s_pad_names[button];
}

const char *tang_gbc_button_name(int button)
{
    if (button < 0 || button >= TANG_GBC_BUTTONS) {
        return "?";
    }
    return s_button_names[button];
}

static int pad_index(const char *name)
{
    for (int i = 0; i < TANG_GBC_CTL_COUNT; i++) {
        if (strcmp(name, s_pad_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static int button_index(const char *name)
{
    for (int i = 0; i < TANG_GBC_BUTTONS; i++) {
        if (strcmp(name, s_button_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

/* What the player did before the mapping existed: the D-pad as itself, A and
 * B as the Classic's A and B, Start and Select as Plus and Minus. */
void tang_gbc_default_map(uint8_t *map)
{
    static const uint8_t pad[TANG_GBC_BUTTONS] = {
        TANG_GBC_CTL_B, TANG_GBC_CTL_A, TANG_GBC_CTL_SELECT, TANG_GBC_CTL_START,
        TANG_GBC_CTL_UP, TANG_GBC_CTL_DOWN, TANG_GBC_CTL_LEFT, TANG_GBC_CTL_RIGHT,
    };

    memcpy(map, pad, sizeof pad);
}

void tang_gbc_defaults(tang_gbc_settings_t *s)
{
    memset(s, 0, sizeof *s);
    tang_gbc_default_map(s->pad_map);
    tang_audio_defaults(&s->audio);
}

/* One key of one section.  Returns false for anything unrecognised, so the
 * caller's count reports how much of the file this firmware understood. */
static bool apply(void *ctx, const char *section, const char *key,
                  const char *value)
{
    tang_gbc_settings_t *s = ctx;

    /* [input] names a Game Boy button and the controller button that answers
     * to it, e.g. `a = x`: the mapping the player is sent, and the same one
     * the Input page edits. */
    if (strcmp(section, "input") == 0) {
        const int button = button_index(key);
        const int pad = pad_index(value);

        if (button < 0 || pad < 0) {
            return false;
        }
        s->pad_map[button] = (uint8_t)pad;
        return true;
    }
    if (strcmp(section, "video") == 0) {
        /* [video] is the picture: the palette a monochrome cartridge is drawn
         * in, and how a colour one's colours are corrected. */
        if (strcmp(key, "palette") == 0) {
            const int palette = palette_index(value);

            if (palette < 0) {
                return false;
            }
            s->palette = (uint8_t)palette;
            return true;
        }
        if (strcmp(key, "colour") == 0) {
            const int colour = colour_index(value);

            if (colour < 0) {
                return false;
            }
            s->colour = (uint8_t)colour;
            return true;
        }
        return false;
    }
    if (strcmp(section, "audio") == 0) {
        return tang_audio_apply(&s->audio, key, value);
    }
    return false;
}

bool tang_gbc_load(tang_gbc_settings_t *s, const char *path)
{
    tang_gbc_defaults(s);
    s->found = tang_settings_walk(path, apply, s, &s->keys);
    return s->found;
}
