// TinyTang — the shell session, mirrored onto the core's text page.
//
// The OSD is a terminal, not a menu driven by a knob.  Whatever the shell
// prints to the USB console is also drawn on the core's page, with a cursor
// blinking where the shell's line editor has it.  Input stays where it has
// always been: the console.  Nothing here reads a switch or an encoder, and
// nothing here needs to.
//
// The emulator handles exactly what the shell emits, which is a small and
// closed set (see src/core/tdsh_terminal.c):
//
//     \r         back to the start of the current line
//     \n         next row, scrolling if needed
//     \033[2K    erase the rest of the current line
//     \033[2J    clear the page
//     \033[H     home
//     \033[<n>C  \033[<n>D   cursor right / left
//     \033[...m  colour -- dropped, the page has one text colour
//
// Everything else is dropped rather than passed through.  Nothing here writes
// to the console: a diagnostic that printed would re-enter the tap that feeds
// it, so failures are counted and reported by `osd term` instead.

#ifndef TANG_OSD_TERM_H
#define TANG_OSD_TERM_H

#include <stdbool.h>
#include <stddef.h>

/* --------------------------------------------------------------- geometry */

/* The page is 32x28, but the core draws its own logo into rows 25 and 26 and
 * that is not ours to move or clear.  The terminal stops short of it. */
#define TANG_TERM_COLS 32
#define TANG_TERM_ROWS 25

/* --------------------------------------------------------------- the API */

/* Start or stop mirroring.  Starting clears the page and puts the cursor
 * home; stopping also hides the page, so the game comes back — that is the
 * mode switch, and `osd on`/`osd off` stay available for the page alone. */
int tang_osd_term_set(bool on);

/* Mirror these bytes.  Cheap and silent when off; call it from the console's
 * one write path. */
void tang_osd_term_feed(const void *data, size_t length);

/* State, for `osd term` with no argument. */
bool     tang_osd_term_enabled(void);
unsigned tang_osd_term_dropped(void);

#endif /* TANG_OSD_TERM_H */
