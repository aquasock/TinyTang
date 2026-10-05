// TinyTang — the desktop layer on the core's extended text page.
//
// TinyDesk's screen, 80 columns by 45 rows, each cell in its own colour, is
// drawn by the core as a layer over the whole HDMI output.  This module is what
// puts it there.
//
// It does not render the desktop, and it does not parse ANSI by hand.  The
// console tap already sees every byte TinyDesk emits, and TinyDesk ships a
// terminal emulator (`td_vterm_t`) that already understands exactly what it
// emits -- 256-colour SGR, absolute cursor addressing, UTF-8, the alternate
// screen.  So the pipeline is
//
//     console tap -> td_vterm_write -> diff -> frames
//
// and nothing in tinydesk changes.
//
// Two grids are kept: the emulator's, which is live, and a shadow of what the
// core has actually been told.  Only the difference is sent, which matters
// because a full repaint is 80x45 cells over a 2 Mbaud link.
//
// The desktop learns its size by moving the cursor far past the corner and
// asking where it landed (`ESC [ 6 n`).  A terminal clamps that move to its own
// size, so the answer *is* the size.  The emulator already implements that
// query against its own grid, so pointing it at 80x45 is the whole mechanism --
// there is nothing to synthesise, and without it the desktop would fall back to
// 80x25 and draw a third of a screen.
//
// Nothing here writes to the console: a diagnostic that printed would re-enter
// the tap that feeds it, so failures are counted and the command reports them.

#ifndef TANG_OSD_DESK_H
#define TANG_OSD_DESK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The layer's grid.  It is the desktop's grid, not a viewport into something
 * larger: the page is the screen. */
#define TANG_DESK_COLS 80
#define TANG_DESK_ROWS 45

/* The desktop's size query, exactly as td_render_query_size() emits it: save
 * the cursor, jump far past the corner (a terminal clamps that), ask where it
 * ended up, restore.
 *
 * While the layer owns the display it is the layer that must answer -- and the
 * console must not, or a terminal on the other end answers too and the desktop
 * flips between the two sizes once a second, a full repaint each time.  The
 * query reaches the layer through the console tap and is kept off the wire by
 * the console writer, so these two definitions are the whole contract between
 * them. */
#define TANG_DESK_SIZE_QUERY     "\x1b" "7\x1b[999;999H\x1b[6n\x1b" "8"
#define TANG_DESK_SIZE_QUERY_LEN 18

/* Start or stop drawing the desktop on the layer.  Starting clears the layer
 * and repaints it from the next output; stopping hides it, so the game comes
 * back. */
int  tang_osd_desk_set(bool on);
bool tang_osd_desk_enabled(void);

/* Mirror these bytes into the layer.  Cheap and silent when off; called from
 * the console's one write path. */
void tang_osd_desk_feed(const void *data, size_t length);

/* One byte of the layer's answer to the desktop, or -1.  The HAL drains this
 * before the console, so the desktop's size query is answered by the emulator
 * that is drawing the layer rather than by whatever is on the other end of the
 * USB cable. */
int  tang_osd_desk_read_byte(void);

/* `osd desk on | off | status`, dispatched from the osd command. */
int  tang_osd_desk_command(int argc, char **argv);

/* The core was reprogrammed underneath us.  `tangload` reconfigures the FPGA,
 * which wipes everything the layer put in it -- the enable bit goes back to 0
 * and the cell store goes blank -- while the shadow still matches the
 * emulator, so nothing would ever resend it.  Without this call the desktop
 * would vanish the first time a cartridge was launched from it and never come
 * back.  Called by the loader, not by the user. */
void tang_osd_desk_core_reloaded(void);

/* Whether the pointer exists.  Only the desktop wants one: it turns on mouse
 * reporting and draws windows to point at.  At the bare console there is
 * nothing to point at, and the pointer's mouse reports would reach the shell
 * as text, so there it is off -- not drawn, not moved by the pad or by
 * right-alt with the arrows.  The desktop command turns it on for the length
 * of desktop_run(); it starts off. */
void tang_osd_desk_set_pointer(bool on);

#endif /* TANG_OSD_DESK_H */
