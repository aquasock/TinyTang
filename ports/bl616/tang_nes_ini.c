// TinyNES — reading /nes.ini, the settings file at the card root.
//
// The same shape as /tang.ini: section headers, `key = value`, `#` comments.
// A missing file, or a key missing from it, is the default -- which is the
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

#include "ff.h"

/* The line buffer: the longest line the file may carry. */
#define LINE_MAX 192
#define SECTION_MAX 24

static const char *const s_pad_names[NES_CTL_COUNT] = {
    "b", "y", "up", "down", "left", "right", "select", "start",
};

static const char *const s_nes_names[TANG_NES_BUTTONS] = {
    "a", "b", "select", "start", "up", "down", "left", "right",
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
    s->volume = 75;
    s->mute = 0;
    s->channels = TANG_CHANNELS_STEREO;
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

/* Trim the whitespace off both ends, in place. */
static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }
    return text;
}

/* One key of one section.  Returns false for anything unrecognised, so the
 * caller's count reports how much of the file this firmware understood. */
static bool apply(tang_nes_settings_t *s, const char *section, const char *key,
                  const char *value)
{
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
            s->palette = 0;         /* FCEUmm's is the only one so far */
            return true;
        }
        if (strcmp(key, "overscan") == 0) {
            s->overscan = strcmp(value, "show") != 0;
            return true;
        }
        return false;
    }
    if (strcmp(section, "audio") == 0) {
        if (strcmp(key, "volume") == 0) {
            const int v = atoi(value);
            s->volume = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
            return true;
        }
        if (strcmp(key, "mute") == 0) {
            s->mute = strcmp(value, "yes") == 0;
            return true;
        }
        if (strcmp(key, "channels") == 0) {
            s->channels = strcmp(value, "mono") == 0 ? TANG_CHANNELS_MONO
                                                     : TANG_CHANNELS_STEREO;
            return true;
        }
        return false;
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

/* One line: one or more `key = value` pairs, because /nes.ini is laid out
 * with several to a line.  A value ends at the first whitespace, which is
 * what lets the next key follow on the same line; a value with a space in it
 * would need its own line and a different rule, and none has one. */
static void handle_line(tang_nes_settings_t *s, char *section, char *line)
{
    char *text = trim(line);

    if (*text == '\0' || *text == '#' || *text == ';') {
        return;
    }
    if (*text == '[') {
        char *end = strchr(text, ']');
        if (end == NULL) {
            return;
        }
        *end = '\0';
        snprintf(section, SECTION_MAX, "%s", trim(text + 1));
        return;
    }

    while (*text != '\0') {
        char *equals = strchr(text, '=');
        if (equals == NULL) {
            return;
        }
        char *key = text;
        char *key_end = equals;
        while (key_end > key && (key_end[-1] == ' ' || key_end[-1] == '\t')) {
            key_end--;
        }
        *key_end = '\0';

        char *value = trim(equals + 1);
        char *value_end = value;
        while (*value_end != '\0' && *value_end != ' ' && *value_end != '\t') {
            value_end++;
        }
        const char after = *value_end;
        *value_end = '\0';
        if (*key != '\0' && apply(s, section, key, value)) {
            s->keys++;
        }
        if (after == '\0') {
            return;
        }
        /* Past the whitespace, and stop at a comment to the end of the line. */
        text = value_end + 1;
        while (*text == ' ' || *text == '\t') {
            text++;
        }
        if (*text == '#' || *text == ';') {
            return;
        }
    }
}

bool tang_nes_load(tang_nes_settings_t *s, const char *path)
{
    char chunk[256];
    char line[LINE_MAX];
    char section[SECTION_MAX] = "";
    size_t len = 0;
    UINT got = 0;
    FIL file;

    tang_nes_defaults(s);
    if (f_open(&file, path, FA_READ) != FR_OK) {
        return false;
    }
    s->found = true;

    /* f_gets is not in this FatFS build (FF_USE_STRFUNC is off), so the
     * lines are split here.  A line longer than the buffer is truncated,
     * which for this file means the key is ignored rather than misread. */
    while (f_read(&file, chunk, sizeof chunk, &got) == FR_OK && got != 0) {
        for (UINT i = 0; i < got; i++) {
            if (chunk[i] == '\n') {
                line[len] = '\0';
                handle_line(s, section, line);
                len = 0;
            } else if (len + 1 < sizeof line) {
                line[len++] = chunk[i];
            }
        }
    }
    if (len != 0) {
        line[len] = '\0';
        handle_line(s, section, line);
    }
    (void)f_close(&file);
    return true;
}
