// TinyTang — the on-screen text page on a loaded core's video output.
//
// A loaded Tang core carries a text page over its picture: 32 columns by 28
// rows of 8x8 cells, drawn from a full ASCII bitmap font.  The page lives in
// the core, not here — this module owns the frames that write to it, and the
// `osd` command that exposes them to the shell.
//
// Two things about the page are the core's, not ours, and are worth knowing
// before drawing on it:
//
//   * It is opaque when shown.  The core's video mixer selects it wholesale
//     (`nes2hdmi.sv`: `if (overlay) rgb <= overlay_color`), so the page covers
//     the game rather than floating over it.  That is why the cartridge loader
//     hides it before starting a game.
//
//   * Column 0 of every row renders in the core's cursor colour rather than
//     the text colour.  It cannot be turned off.  A menu therefore uses
//     column 0 for its selection marker, which is the one highlight the page
//     offers, rather than fighting it.
//
// Unlike the raw transport in tang_fpga_link.h, these functions take the
// transmit lock themselves, so a caller cannot forget to and cannot deadlock
// by nesting.

#ifndef TANG_OSD_H
#define TANG_OSD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TANG_OSD_COLS 32
#define TANG_OSD_ROWS 28

/* The shell command. */
int tang_osd_register(void);

/* Show or hide the page.  Writing text also shows it, so this is only needed
 * to hide it again.  Returns 0, or -1 if the link is down. */
int tang_osd_set(bool on);

/* Whether the OSD is shown, as this module last told the core.  The cartridge
 * loader hides it through tang_osd_set(), so this stays true to what the core
 * was actually told rather than to what someone assumed. */
bool tang_osd_shown(void);

/* Send the overlay state this module last recorded, again -- for when the core
 * has been reprogrammed and its own copy is unknown.  The state is read under
 * the transmit lock, in the same critical stretch as the send.
 *
 * That is the whole reason this exists rather than tang_osd_set(
 * tang_osd_shown()): that reads the state first and takes the lock second, so
 * a caller that has to wait for the lock -- while nesload holds it to stream a
 * ROM -- sends a value that was true before nesload hid the overlay, and puts
 * TinyDesk back over a game that is running.  Returns 0, or -1 if the link is
 * down. */
int tang_osd_reassert(void);

/* Flip the overlay, reading its state under the lock for the same reason.
 * This is the F12 / L session switch.  Returns 0, or -1 if the link is down. */
int tang_osd_toggle(void);

/* Blank every cell. */
int tang_osd_clear(void);

/* Write `length` characters at cell (x, y).  The text is clipped to the end of
 * the row, because the core silently drops what does not fit.  Returns 0, or
 * -1 if the link is down or the frame was refused. */
int tang_osd_put(uint8_t x, uint8_t y, const char *text, size_t length);

#endif /* TANG_OSD_H */
