// TinySNES — reading /tinysnes.ini, the settings file at the card root.
//
// The same shape as /tinynes.ini, read by ports/bl616/tang_settings_ini.c.  See
// tang_snes_ini.h for what the file carries and why only [input] exists so far.
//
// SPDX-License-Identifier: MIT

#include "tang_snes_ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ff.h"
#include "tang_settings_ini.h"

/* The pad's buttons, in the core's order.  These are the keys of [input] as
 * well: `p1_a = b` binds output button A to the source called "b".
 *
 * Three names are shortened because a settings key is written by hand:
 * `rt`/`lt` for the Classic's ZR/ZL shoulders, and `dn` for down so that `up`
 * and `dn` cannot be confused by a stray character in an editor. */
static const char *const s_button_names[TANG_SNES_BUTTONS] = {
    "r", "l", "x", "a", "rt", "lt", "dn", "up", "start", "select", "y", "b",
};

/* What a button may answer to.  The same names again -- a source is a button
 * of the pad -- plus "none", which is the deliberate absence of a binding. */
static const char *const s_source_names[TANG_SNES_SOURCES] = {
    "r", "l", "x", "a", "rt", "lt", "dn", "up", "start", "select", "y", "b",
    "none",
};

const char *tang_snes_button_name(int button)
{
    if (button < 0 || button >= TANG_SNES_BUTTONS) {
        return "?";
    }
    return s_button_names[button];
}

const char *tang_snes_source_name(int source)
{
    if (source < 0 || source >= TANG_SNES_SOURCES) {
        return "?";
    }
    return s_source_names[source];
}

void tang_snes_defaults(tang_snes_settings_t *s)
{
    memset(s, 0, sizeof *s);
    for (int i = 0; i < TANG_SNES_BUTTONS; i++) {
        /* Identity: output i takes source i, which is what the fabric did
         * before this file existed. */
        s->p1[i] = (uint8_t)i;
        s->p2[i] = (uint8_t)i;
    }
    s->version = 1;
}

/* A button name to its index, or -1.  Exact match: `r` must not match `rt`,
 * which a prefix compare would do. */
static int button_index(const char *name)
{
    for (int i = 0; i < TANG_SNES_BUTTONS; i++) {
        if (strcmp(name, s_button_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static int source_index(const char *name)
{
    for (int i = 0; i < TANG_SNES_SOURCES; i++) {
        if (strcmp(name, s_source_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static bool apply(void *ctx, const char *section, const char *key,
                  const char *value)
{
    tang_snes_settings_t *s = ctx;

    if (strcmp(section, "input") == 0) {
        uint8_t *map;
        int button, source;

        /* `p1_<button>`, `p2_<button>`. */
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
        button = button_index(key + 3);
        source = source_index(value);
        if (button < 0 || source < 0) {
            /* A name this build does not know keeps the default and is not
             * counted, so a misspelt binding shows in the key count rather
             * than as a button that quietly does nothing. */
            return false;
        }
        map[button] = (uint8_t)source;
        return true;
    }
    if (strcmp(section, "tinysnes") == 0) {
        if (strcmp(key, "version") == 0) {
            s->version = (uint8_t)atoi(value);
            return true;
        }
        return false;
    }
    return false;
}

bool tang_snes_settings_load(tang_snes_settings_t *s, const char *path)
{
    tang_snes_defaults(s);
    s->found = tang_settings_walk(path, apply, s, &s->keys);
    return s->found;
}

uint64_t tang_snes_map_word(const uint8_t map[TANG_SNES_BUTTONS])
{
    uint64_t word = 0;
    for (int i = 0; i < TANG_SNES_BUTTONS; i++) {
        const unsigned sel = map[i] > TANG_SNES_SRC_NONE ? TANG_SNES_SRC_NONE
                                                        : map[i];
        word |= (uint64_t)(sel & 0xFu) << (4 * i);
    }
    return word;
}

/* The file, written from one player's arrays.  Every button gets a line rather
 * than only the ones that differ, so the file is the whole mapping and can be
 * read without knowing the defaults. */
int tang_snes_settings_save(const tang_snes_settings_t *s, const char *path,
                            char *why, size_t why_len)
{
    char temp[96], bak[96];
    snprintf(temp, sizeof temp, "%s.tmp", path);
    snprintf(bak, sizeof bak, "%s.bak", path);

    FIL file;
    if (f_open(&file, temp, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        snprintf(why, why_len, "cannot write %s", temp);
        return -1;
    }

    char text[768];
    int n = snprintf(text, sizeof text,
                     "# TinySNES settings, written by the desktop window.\n"
                     "# Each line binds a SNES button to a controller button.\n"
                     "[tinysnes]\nversion = %u\n\n[input]\n",
                     (unsigned)s->version);
    for (int p = 0; p < 2 && n > 0 && (size_t)n < sizeof text; p++) {
        const uint8_t *map = p == 0 ? s->p1 : s->p2;
        for (int i = 0; i < TANG_SNES_BUTTONS && (size_t)n < sizeof text; i++) {
            n += snprintf(text + n, sizeof text - (size_t)n, "p%d_%s = %s\n",
                          p + 1, s_button_names[i],
                          tang_snes_source_name(map[i]));
        }
    }
    if (n <= 0 || (size_t)n >= sizeof text) {
        (void)f_close(&file);
        (void)f_unlink(temp);
        snprintf(why, why_len, "the settings do not fit");
        return -1;
    }

    UINT wrote = 0;
    const FRESULT w = f_write(&file, text, (UINT)n, &wrote);
    (void)f_close(&file);
    if (w != FR_OK || wrote != (UINT)n) {
        (void)f_unlink(temp);
        snprintf(why, why_len, "the card stopped taking the file");
        return -1;
    }

    /* FatFS will not rename onto a name that exists, so the old file moves
     * aside first and is removed only once the new one is in place.  A failure
     * here puts it back rather than leaving the card with neither. */
    (void)f_unlink(bak);
    const bool had_old = f_rename(path, bak) == FR_OK;
    if (f_rename(temp, path) != FR_OK) {
        if (had_old) {
            (void)f_rename(bak, path);
        }
        (void)f_unlink(temp);
        snprintf(why, why_len, "the card will not put the file in place");
        return -1;
    }
    (void)f_unlink(bak);
    return 0;
}
