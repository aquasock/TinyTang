// TinyGBA — reading and writing /tinygba.ini, the settings file at the card
// root.  See tang_gba_ini.h for what the file carries and why the map runs
// from controller button to GBA key.
//
// SPDX-License-Identifier: MIT

#include "tang_gba_ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ff.h"
#include "tang_settings_ini.h"

/* The controller's buttons as [input]'s keys, and as the page shows them.
 * `plus` and `minus` are spelt out in the file because `+` and `-` are not
 * comfortable keys to type in an editor. */
static const char *const s_source_keys[TANG_GBA_SOURCES] = {
    "a", "b", "x", "y", "l", "r", "zl", "zr", "plus", "minus",
    "up", "down", "left", "right",
};
static const char *const s_source_names[TANG_GBA_SOURCES] = {
    "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "+", "-",
    "Up", "Down", "Left", "Right",
};

/* The GBA's keys as [input]'s values, and as the page shows them. */
static const char *const s_key_values[TANG_GBA_KEYS + 1] = {
    "a", "b", "l", "r", "start", "select", "up", "down", "left", "right", "none",
};
static const char *const s_key_names[TANG_GBA_KEYS + 1] = {
    "A", "B", "L", "R", "START", "SELECT", "UP", "DOWN", "LEFT", "RIGHT", "none",
};

/* The mapping fixed in fabric before the map existed (`joy_wii`, now in
 * tinygba_pads.sv), and the core's PAD_MAP_DEFAULT in iosys_bl616.v. */
static const uint8_t s_default_map[TANG_GBA_SOURCES] = {
    TANG_GBA_KEY_A, TANG_GBA_KEY_B,          /* A, B */
    TANG_GBA_KEY_A, TANG_GBA_KEY_B,          /* X, Y */
    TANG_GBA_KEY_L, TANG_GBA_KEY_R,          /* L, R */
    TANG_GBA_KEY_L, TANG_GBA_KEY_R,          /* ZL, ZR */
    TANG_GBA_KEY_START, TANG_GBA_KEY_SELECT, /* +, - */
    TANG_GBA_KEY_UP, TANG_GBA_KEY_DOWN, TANG_GBA_KEY_LEFT, TANG_GBA_KEY_RIGHT,
};

const char *tang_gba_source_name(int source)
{
    if (source < 0 || source >= TANG_GBA_SOURCES) {
        return "?";
    }
    return s_source_names[source];
}

const char *tang_gba_key_name(int key)
{
    if (key < 0 || key > TANG_GBA_KEY_NONE) {
        return "?";
    }
    return s_key_names[key];
}

void tang_gba_default_map(uint8_t map[TANG_GBA_SOURCES])
{
    memcpy(map, s_default_map, TANG_GBA_SOURCES);
}

void tang_gba_defaults(tang_gba_settings_t *s)
{
    memset(s, 0, sizeof *s);
    tang_gba_default_map(s->map);
    s->version = 1;
}

/* Exact matches: `l` must not match `left`, which a prefix compare would do. */
static int source_index(const char *key)
{
    for (int i = 0; i < TANG_GBA_SOURCES; i++) {
        if (strcmp(key, s_source_keys[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static int key_index(const char *value)
{
    for (int i = 0; i <= TANG_GBA_KEY_NONE; i++) {
        if (strcmp(value, s_key_values[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static bool apply(void *ctx, const char *section, const char *key,
                  const char *value)
{
    tang_gba_settings_t *s = ctx;

    if (strcmp(section, "input") == 0) {
        const int source = source_index(key);
        const int target = key_index(value);
        if (source < 0 || target < 0) {
            /* A name this build does not know keeps the default and is not
             * counted, so a misspelt binding shows in the key count rather
             * than as a button that quietly does nothing. */
            return false;
        }
        s->map[source] = (uint8_t)target;
        return true;
    }
    if (strcmp(section, "tinygba") == 0) {
        if (strcmp(key, "version") == 0) {
            s->version = (uint8_t)atoi(value);
            return true;
        }
        return false;
    }
    return false;
}

bool tang_gba_settings_load(tang_gba_settings_t *s, const char *path)
{
    tang_gba_defaults(s);
    s->found = tang_settings_walk(path, apply, s, &s->keys);
    return s->found;
}

uint64_t tang_gba_map_word(const uint8_t map[TANG_GBA_SOURCES])
{
    uint64_t word = 0;
    for (int i = 0; i < TANG_GBA_SOURCES; i++) {
        /* Anything that is not a key is sent as none, so a corrupt array can
         * only remove a button, never press a key nobody chose. */
        const unsigned sel = map[i] >= TANG_GBA_KEY_NONE ? 0xFu : map[i];
        word |= (uint64_t)sel << (4 * i);
    }
    return word;
}

void tang_gba_map_bytes(const uint8_t map[TANG_GBA_SOURCES], uint8_t out[7])
{
    const uint64_t word = tang_gba_map_word(map);
    for (int i = 0; i < 7; i++) {
        out[i] = (uint8_t)(word >> (8 * (6 - i)));
    }
}

/* The file.  Every button gets a line rather than only the ones that differ,
 * so the file is the whole mapping and can be read without knowing the
 * defaults. */
int tang_gba_settings_save(const tang_gba_settings_t *s, const char *path,
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

    char text[512];
    int n = snprintf(text, sizeof text,
                     "# TinyGBA settings, written by the desktop window.\n"
                     "# Each line names the GBA key a controller button presses.\n"
                     "[tinygba]\nversion = %u\n\n[input]\n",
                     (unsigned)s->version);
    for (int i = 0; i < TANG_GBA_SOURCES && n > 0 && (size_t)n < sizeof text; i++) {
        const unsigned k = s->map[i] > TANG_GBA_KEY_NONE ? TANG_GBA_KEY_NONE : s->map[i];
        n += snprintf(text + n, sizeof text - (size_t)n, "%s = %s\n",
                      s_source_keys[i], s_key_values[k]);
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
