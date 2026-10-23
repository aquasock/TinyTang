// TinyTang — the /tang.ini parser.  See tang_ini.h.
//
// SPDX-License-Identifier: MIT

#include "tang_ini.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *const s_names[] = {
    [TANG_PMOD_NONE]    = "none",
    [TANG_PMOD_OLEDRGB] = "oledrgb",
    [TANG_PMOD_VGA_J1]  = "vga_j1",
    [TANG_PMOD_VGA_J2]  = "vga_j2",
    [TANG_PMOD_ENCODER] = "encoder",
    [TANG_PMOD_I2S2]    = "i2s2",
    [TANG_PMOD_I2C]     = "i2c",
    [TANG_PMOD_I2C_UART] = "i2c_uart",
};
#define NAMES (sizeof(s_names) / sizeof(s_names[0]))

static const char *const s_layouts[] = {
    [TANG_LAYOUT_NONE]      = "none",
    [TANG_LAYOUT_I2S2_OLED] = "i2s2_oled",
    [TANG_LAYOUT_VGA]       = "vga",
};
#define LAYOUTS (sizeof(s_layouts) / sizeof(s_layouts[0]))

const char *tang_ini_module_name(uint8_t module)
{
    return module < NAMES ? s_names[module] : "?";
}

const char *tang_ini_layout_name(uint8_t layout)
{
    return layout < LAYOUTS ? s_layouts[layout] : "?";
}

static char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static bool same(const char *a, const char *b)
{
    while (*a && *b) {
        if (lower(*a++) != lower(*b++)) {
            return false;
        }
    }
    return *a == *b;
}

static bool space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

static char *trim(char *s)
{
    while (space(*s)) {
        s++;
    }
    size_t n = strlen(s);
    while (n > 0 && space(s[n - 1])) {
        s[--n] = '\0';
    }
    return s;
}

static void note(tang_ini_t *ini, unsigned line, bool error, const char *fmt, ...)
{
    if (ini->nnotes >= TANG_INI_NOTES) {
        ini->more_notes++;
        return;
    }
    tang_ini_note_t *n = &ini->notes[ini->nnotes++];
    n->line = (uint16_t)line;
    n->error = error;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(n->text, sizeof(n->text), fmt, ap);
    va_end(ap);
}

void tang_ini_begin(tang_ini_t *ini)
{
    memset(ini, 0, sizeof(*ini));
    ini->section = -1;
}

/* The keys of the old per-socket contract, refused by name. */
static bool old_socket_key(const char *key)
{
    return same(key, "pmod0") || same(key, "pmod1") || same(key, "pmod0_flip") ||
           same(key, "pmod1_flip");
}

static void line_done(tang_ini_t *ini)
{
    const unsigned no = ++ini->lineno;
    if (ini->overlong) {
        note(ini, no, false, "longer than %d characters; ignored", TANG_INI_LINE_MAX - 1);
        ini->overlong = false;
        ini->len = 0;
        return;
    }
    ini->buf[ini->len] = '\0';
    ini->len = 0;
    char *s = trim(ini->buf);
    if (*s == '\0' || *s == ';' || *s == '#') {
        return;
    }

    if (*s == '[') {
        char *end = strchr(s, ']');
        if (end == NULL) {
            note(ini, no, false, "section without ']'; its keys are ignored");
            ini->section = 1;
            return;
        }
        *end = '\0';
        const char *name = trim(s + 1);
        ini->section = same(name, "tang") ? 0 : 1;
        if (ini->section != 0) {
            note(ini, no, false, "section [%.24s] is not used; its keys are ignored", name);
        }
        return;
    }

    char *eq = strchr(s, '=');
    if (eq == NULL) {
        note(ini, no, false, "not a key = value line; ignored");
        return;
    }
    *eq = '\0';
    const char *key = trim(s);
    char *value = eq + 1;
    /* A comment after the value: no layout name contains these. */
    value[strcspn(value, ";#")] = '\0';
    value = trim(value);

    if (ini->section != 0) {
        if (ini->section < 0) {
            note(ini, no, false, "%.24s is outside [tang]; ignored", key);
        }
        return;                            /* another section: warned once */
    }

    if (old_socket_key(key)) {
        ini->old_key = true;
        note(ini, no, true, "%.12s is no longer read; write layout = i2s2_oled or vga", key);
        return;
    }
    if (!same(key, "layout")) {
        note(ini, no, false, "unknown key %.24s; ignored", key);
        return;
    }

    if (ini->layout_line != 0) {
        note(ini, no, false, "layout given again; this line wins");
    }
    ini->layout_line = (uint16_t)no;
    ini->layout_bad = false;
    for (unsigned l = TANG_LAYOUT_NONE + 1; l < LAYOUTS; l++) {
        if (same(value, s_layouts[l])) {
            ini->layout = (uint8_t)l;
            return;
        }
    }
    ini->layout = TANG_LAYOUT_NONE;
    ini->layout_bad = true;
    note(ini, no, true, "unknown layout '%.20s'; use i2s2_oled or vga", value);
}

void tang_ini_feed(tang_ini_t *ini, const char *bytes, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        const char c = bytes[i];
        if (c == '\n') {
            line_done(ini);
        } else if (ini->len < TANG_INI_LINE_MAX - 1) {
            ini->buf[ini->len++] = c == '\0' ? ' ' : c;
        } else {
            ini->overlong = true;
        }
    }
}

void tang_ini_end(tang_ini_t *ini)
{
    if (ini->len > 0 || ini->overlong) {
        line_done(ini);                    /* a last line without '\n' */
    }

    if (ini->old_key || ini->layout_bad) {
        ini->layout = TANG_LAYOUT_NONE;
        note(ini, 0, true, "both sockets released");
    } else if (ini->layout_line == 0) {
        note(ini, 0, false, "no layout line; both sockets released");
    }
}

uint32_t tang_ini_word(uint8_t layout, uint8_t core_id)
{
    uint8_t pmod0 = TANG_PMOD_NONE, pmod1 = TANG_PMOD_NONE;
    if (core_id != TANG_INI_CORE_PHOSPHOR && core_id != TANG_INI_CORE_DESKTOP) {
        return 0;
    }
    switch (layout) {
    case TANG_LAYOUT_I2S2_OLED:
        pmod0 = TANG_PMOD_OLEDRGB;
        /* The desktop core refuses a word that names the I2S2, and would
         * release the OLED with it (PMOD-006); its PMOD1 stays released. */
        pmod1 = core_id == TANG_INI_CORE_DESKTOP ? TANG_PMOD_NONE : TANG_PMOD_I2S2;
        break;
    case TANG_LAYOUT_VGA:
        /* Right side up, J1 (red, blue) is in PMOD1 and J2 (green, sync) in
         * PMOD0. */
        pmod0 = TANG_PMOD_VGA_J2;
        pmod1 = TANG_PMOD_VGA_J1;
        break;
    default:
        break;
    }
    return ((uint32_t)pmod0 << 4) | ((uint32_t)pmod1 << 8);
}
