// TinyTang — /tang.ini, the declaration of what is seated in the PMOD sockets.
//
// PMOD modules carry no identification pins, so the core can only be told
// what is attached (PMOD-002).  The file is Tang-Phosphor's contract:
//
//     [tang]
//     pmod0 = oledrgb
//     pmod0_flip = no
//     pmod1 = encoder
//     pmod1_flip = yes
//
// A missing file, or a socket with no entry, leaves that socket released,
// which is the safe state.  A module name this firmware does not know, a
// flip that is not yes or no, and a vga_j1 without vga_j2 in the other socket
// (or the reverse) release that socket and say why.  Keys and sections it
// does not know are warned about and ignored, so the file can carry settings
// for a later firmware.  Comments start with ';' or '#', at the start of a
// line or after a value.  Names and yes/no are not case-sensitive.
//
// This file is only the parser: bytes in, the socket control word and the
// problems out, with nothing from the card or the core in it, so it can be
// checked on the host.  ports/bl616/phosphor/pmod_sockets.cpp reads the file
// and writes the word to a core that has the sockets.
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

/* Socket personalities: Tang-Phosphor's socket control numbering (PMOD-005). */
enum {
    TANG_PMOD_NONE    = 0,
    TANG_PMOD_OLEDRGB = 1,
    TANG_PMOD_VGA_J1  = 2,
    TANG_PMOD_VGA_J2  = 3,
    TANG_PMOD_ENCODER = 4,
    TANG_PMOD_I2S2    = 5,
};

typedef struct {
    uint16_t line;               /* 0 for a problem of the whole file */
    bool     error;              /* a socket was released because of it */
    char     text[80];
} tang_ini_note_t;

typedef struct {
    /* The result, after tang_ini_end(). */
    uint8_t  module[TANG_INI_SOCKETS];   /* TANG_PMOD_*, NONE when released */
    bool     flip[TANG_INI_SOCKETS];     /* seated upside down */
    uint32_t word;                       /* socket control: [7:4] [11:8] [12] [13] */
    tang_ini_note_t notes[TANG_INI_NOTES];
    unsigned nnotes;
    unsigned more_notes;                 /* problems past the table */

    /* Parsing state. */
    char     buf[TANG_INI_LINE_MAX];
    unsigned len;
    bool     overlong;
    unsigned lineno;
    int      section;                    /* -1 none yet, 0 [tang], 1 another */
    uint16_t module_line[TANG_INI_SOCKETS];
    uint16_t flip_line[TANG_INI_SOCKETS];
    bool     module_bad[TANG_INI_SOCKETS];   /* the latest pmodN line was refused */
    bool     flip_bad[TANG_INI_SOCKETS];     /* the latest pmodN_flip line was */
    bool     refused[TANG_INI_SOCKETS];      /* released for any reason, after end */
} tang_ini_t;

void tang_ini_begin(tang_ini_t *ini);
/* Any number of bytes at a time; a line may span calls. */
void tang_ini_feed(tang_ini_t *ini, const char *bytes, size_t n);
/* Finish the last line, check the VGA pairing and compute the word. */
void tang_ini_end(tang_ini_t *ini);

/* "none", "oledrgb", ...; "?" for a number with no name. */
const char *tang_ini_module_name(uint8_t module);

#ifdef __cplusplus
}
#endif

#endif /* TANG_INI_H */
