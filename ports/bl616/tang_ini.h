// TinyTang — /tinytang.ini, the declaration of what is seated in the PMOD sockets.
//
// PMOD modules carry no identification pins, so the core can only be told
// what is attached (PMOD-002).  Two layouts are supported, each with its
// modules in fixed sockets and seated right side up, so the file names one:
//
//     [tang]
//     layout = i2s2_oled    ; I2S2 in PMOD1 beside HDMI, OLEDrgb in PMOD0
//     layout = vga          ; the PmodVGA across both, J1 in PMOD1, J2 in PMOD0
//
// There is no flip and no choice of socket.  A missing file, or a file with
// no layout line, leaves both sockets released, which is the safe state; so
// do an unknown layout and the per-socket keys of the old contract (pmod0,
// pmod1, pmod0_flip, pmod1_flip), which are refused with a note saying what
// to write instead.  Other keys and sections are warned about and ignored.
// Comments start with ';' or '#', at the start of a line or after a value.
// Names are not case-sensitive.
//
// Each core gets the layout as the socket control word it can take:
// tang_ini_word() says which.  The desktop core has no I2S2, so for
// i2s2_oled it is sent the OLED alone and PMOD1 stays released.
//
// This file is only the parser: bytes in, the layout and the problems out,
// with nothing from the card or the core in it, so it can be checked on the
// host.  ports/bl616/phosphor/pmod_sockets.cpp reads the file and writes the
// word to a core that has the sockets.
//
// SPDX-License-Identifier: MIT

#ifndef TANG_INI_H
#define TANG_INI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TANG_INI_SOCKETS   2
#define TANG_INI_LINE_MAX  128   /* longer lines are ignored with a warning */
#define TANG_INI_NOTES     8     /* problems kept; more are counted */

/* Socket personalities: Tang-Phosphor's socket control numbering (PMOD-004).
 * Only the ones a layout uses are written.  6 and 7 were the desktop core's
 * I2C and I2C-plus-UART sockets until desktop ABI 1.5 moved its I2C bus to
 * the USB-A port; they are kept here only to name them. */
enum {
    TANG_PMOD_NONE    = 0,
    TANG_PMOD_OLEDRGB = 1,
    TANG_PMOD_VGA_J1  = 2,
    TANG_PMOD_VGA_J2  = 3,
    TANG_PMOD_ENCODER = 4,
    TANG_PMOD_I2S2    = 5,
    TANG_PMOD_I2C     = 6,
    TANG_PMOD_I2C_UART = 7,
};

/* The supported layouts; NONE is both sockets released. */
enum {
    TANG_LAYOUT_NONE      = 0,
    TANG_LAYOUT_I2S2_OLED = 1,
    TANG_LAYOUT_VGA       = 2,
};

/* Core IDs that have the sockets (pmod_sockets.cpp matches their ABI). */
#define TANG_INI_CORE_PHOSPHOR 0x50
#define TANG_INI_CORE_DESKTOP  0x54

typedef struct {
    uint16_t line;               /* 0 for a problem of the whole file */
    bool     error;              /* the sockets were released because of it */
    char     text[80];
} tang_ini_note_t;

typedef struct {
    /* The result, after tang_ini_end(). */
    uint8_t  layout;                     /* TANG_LAYOUT_*, NONE when released */
    tang_ini_note_t notes[TANG_INI_NOTES];
    unsigned nnotes;
    unsigned more_notes;                 /* problems past the table */

    /* Parsing state. */
    char     buf[TANG_INI_LINE_MAX];
    unsigned len;
    bool     overlong;
    unsigned lineno;
    int      section;                    /* -1 none yet, 0 [tang], 1 another */
    uint16_t layout_line;
    bool     layout_bad;                 /* the latest layout line was refused */
    bool     old_key;                    /* a pmodN or pmodN_flip line was seen */
} tang_ini_t;

void tang_ini_begin(tang_ini_t *ini);
/* Any number of bytes at a time; a line may span calls. */
void tang_ini_feed(tang_ini_t *ini, const char *bytes, size_t n);
/* Finish the last line and settle the layout. */
void tang_ini_end(tang_ini_t *ini);

/* The socket control word for `layout` on core `core_id`: personalities
 * [7:4] PMOD0 and [11:8] PMOD1, never a flip.  0 for a core without the
 * sockets. */
uint32_t tang_ini_word(uint8_t layout, uint8_t core_id);

/* "none", "i2s2_oled", "vga"; "?" for a number with no name. */
const char *tang_ini_layout_name(uint8_t layout);
/* "none", "oledrgb", ...; "?" for a number with no name. */
const char *tang_ini_module_name(uint8_t module);

#ifdef __cplusplus
}
#endif

#endif /* TANG_INI_H */
