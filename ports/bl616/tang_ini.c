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
};
#define NAMES (sizeof(s_names) / sizeof(s_names[0]))

const char *tang_ini_module_name(uint8_t module)
{
    return module < NAMES ? s_names[module] : "?";
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

/* "pmod0" -> 0, "pmod1_flip" -> 1 with *flip; -1 for any other key. */
static int socket_key(const char *key, bool *flip)
{
    static const char *const modules[TANG_INI_SOCKETS] = {"pmod0", "pmod1"};
    static const char *const flips[TANG_INI_SOCKETS] = {"pmod0_flip", "pmod1_flip"};
    for (int s = 0; s < TANG_INI_SOCKETS; s++) {
        if (same(key, modules[s]) || same(key, flips[s])) {
            *flip = same(key, flips[s]);
            return s;
        }
    }
    return -1;
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
    /* A comment after the value: no name or yes/no contains these. */
    value[strcspn(value, ";#")] = '\0';
    value = trim(value);

    if (ini->section != 0) {
        if (ini->section < 0) {
            note(ini, no, false, "%.24s is outside [tang]; ignored", key);
        }
        return;                            /* another section: warned once */
    }

    bool flip = false;
    const int sock = socket_key(key, &flip);
    if (sock < 0) {
        note(ini, no, false, "unknown key %.24s; ignored", key);
        return;
    }

    if (!flip) {
        if (ini->module_line[sock] != 0) {
            note(ini, no, false, "pmod%d given again; this line wins", sock);
        }
        ini->module_line[sock] = (uint16_t)no;
        ini->module_bad[sock] = false;
        for (unsigned m = 0; m < NAMES; m++) {
            if (same(value, s_names[m])) {
                ini->module[sock] = (uint8_t)m;
                return;
            }
        }
        ini->module[sock] = TANG_PMOD_NONE;
        ini->module_bad[sock] = true;
        note(ini, no, true, "pmod%d: unknown module '%.20s'; pmod%d released", sock, value, sock);
        return;
    }

    if (ini->flip_line[sock] != 0) {
        note(ini, no, false, "pmod%d_flip given again; this line wins", sock);
    }
    ini->flip_line[sock] = (uint16_t)no;
    ini->flip_bad[sock] = false;
    if (same(value, "yes")) {
        ini->flip[sock] = true;
    } else if (same(value, "no")) {
        ini->flip[sock] = false;
    } else {
        ini->flip[sock] = false;
        ini->flip_bad[sock] = true;
        note(ini, no, true, "pmod%d_flip: '%.20s' is not yes or no; pmod%d released",
             sock, value, sock);
    }
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

    for (int s = 0; s < TANG_INI_SOCKETS; s++) {
        ini->refused[s] = ini->module_bad[s] || ini->flip_bad[s];
        if (ini->refused[s]) {
            ini->module[s] = TANG_PMOD_NONE;
            ini->flip[s] = false;
        }
    }

    /* The PmodVGA is one module across both sockets: each half needs the
     * other.  Checked on what survived, so a refused partner counts as none. */
    uint8_t m[TANG_INI_SOCKETS];
    memcpy(m, ini->module, sizeof(m));
    for (int s = 0; s < TANG_INI_SOCKETS; s++) {
        const int o = 1 - s;
        const bool j1 = m[s] == TANG_PMOD_VGA_J1, j2 = m[s] == TANG_PMOD_VGA_J2;
        if ((j1 && m[o] != TANG_PMOD_VGA_J2) || (j2 && m[o] != TANG_PMOD_VGA_J1)) {
            note(ini, ini->module_line[s], true, "pmod%d: %s needs %s in pmod%d; pmod%d released",
                 s, j1 ? "vga_j1" : "vga_j2", j1 ? "vga_j2" : "vga_j1", o, s);
            ini->module[s] = TANG_PMOD_NONE;
            ini->flip[s] = false;
            ini->refused[s] = true;
        }
    }

    for (int s = 0; s < TANG_INI_SOCKETS; s++) {
        if (ini->flip[s] && ini->module[s] == TANG_PMOD_NONE && !ini->refused[s]) {
            note(ini, ini->flip_line[s], false, "pmod%d_flip has no module to apply to", s);
            ini->flip[s] = false;
        }
    }

    ini->word = ((uint32_t)ini->module[0] << 4) | ((uint32_t)ini->module[1] << 8) |
                (ini->flip[0] ? 1u << 12 : 0u) | (ini->flip[1] ? 1u << 13 : 0u);
}
