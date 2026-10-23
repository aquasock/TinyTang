// TinyTang — reading the cartridge players' settings files.
//
// A missing file, or a key missing from it, is the default, so dropping one on
// the card is safe and an old card keeps working.  Unknown keys and sections
// are skipped rather than refused, and simply not counted, so a newer file on
// an older firmware still starts.
//
// SPDX-License-Identifier: MIT

#include "tang_settings_ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ff.h"

/* The line buffer: the longest line the file may carry. */
#define LINE_MAX 192
#define SECTION_MAX 24

typedef struct {
    tang_settings_apply_fn apply;
    void *ctx;
    uint16_t *keys;
    char section[SECTION_MAX];
} walk_t;

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

/* One line: one or more `key = value` pairs, because /tinynes.ini is laid out
 * with several to a line.  A value ends at the first whitespace, which is
 * what lets the next key follow on the same line; a value with a space in it
 * would need its own line and a different rule, and none has one. */
static void handle_line(walk_t *walk, char *line)
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
        snprintf(walk->section, SECTION_MAX, "%s", trim(text + 1));
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
        if (*key != '\0' && walk->apply(walk->ctx, walk->section, key, value)) {
            (*walk->keys)++;
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

bool tang_settings_walk(const char *path, tang_settings_apply_fn apply,
                        void *ctx, uint16_t *keys)
{
    char chunk[256];
    char line[LINE_MAX];
    size_t len = 0;
    UINT got = 0;
    FIL file;
    walk_t walk = { apply, ctx, keys, "" };

    if (f_open(&file, path, FA_READ) != FR_OK) {
        return false;
    }

    /* f_gets is not in this FatFS build (FF_USE_STRFUNC is off), so the
     * lines are split here.  A line longer than the buffer is truncated,
     * which for this file means the key is ignored rather than misread. */
    while (f_read(&file, chunk, sizeof chunk, &got) == FR_OK && got != 0) {
        for (UINT i = 0; i < got; i++) {
            if (chunk[i] == '\n') {
                line[len] = '\0';
                handle_line(&walk, line);
                len = 0;
            } else if (len + 1 < sizeof line) {
                line[len++] = chunk[i];
            }
        }
    }
    if (len != 0) {
        line[len] = '\0';
        handle_line(&walk, line);
    }
    (void)f_close(&file);
    return true;
}

void tang_audio_defaults(tang_audio_t *a)
{
    a->volume = TANG_VOLUME_MAX;
    a->mute = 0;
}

bool tang_audio_apply(tang_audio_t *a, const char *key, const char *value)
{
    if (strcmp(key, "volume") == 0) {
        const int v = atoi(value);
        a->volume = (uint8_t)(v < 0 ? 0 : v > TANG_VOLUME_MAX ? TANG_VOLUME_MAX : v);
        return true;
    }
    if (strcmp(key, "mute") == 0) {
        a->mute = strcmp(value, "yes") == 0;
        return true;
    }
    return false;
}
