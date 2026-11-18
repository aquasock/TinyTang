/* TinyTang — the desktop core's pixel plane, from the BL616 side.
 *
 * The desktop core has an RGB565 framebuffer that HDMI shows instead of the
 * text layer while it is on.  An app that owns the whole screen draws into it,
 * the way TinyDesk's LCD ports let an app blit into their panel: the Reader
 * first.  Desktop ABI 1.8 has it at 640x360 in block RAM, shown at 2x; ABI 1.9
 * at 1280x720 in the dock's SDRAM, shown 1:1 (fb_sdram.sv).  Its size is the
 * core's register 0x0c, so an app asks tang_plane_size rather than assuming.
 *
 * Frames (desktop_link.v): 0x18 x y moves the write cursor, 0x19 streams
 * RGB565 pixels from it, high byte first, the cursor running on across rows;
 * 0x1A shows or hides the plane.  One frame holds at most 1021 pixels, so a
 * 1280-pixel row is sent as two.  At UART1's 2 Mbaud a whole 1280x720 screen
 * is about 9.4 s, and a 640x360 one about 2.3 s.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef TANG_PLANE_H
#define TANG_PLANE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The largest plane any desktop core has: buffers sized for it fit all. */
#define TANG_PLANE_MAX_WIDTH  1280
#define TANG_PLANE_MAX_HEIGHT 720

/* The loaded core has the plane: the desktop core at ABI 1.8 or later. */
bool tang_plane_available(void);

/* The loaded core's plane size, from its register 0x0c (read once per loaded
 * core), or the size its ABI is known to have if that does not answer.  False
 * when there is no plane. */
bool tang_plane_size(int *w, int *h);

/* Show the plane instead of the text layer, or hand the screen back.
 * 0 on success; -1 when the link is busy, held by a host, or the core has no
 * plane. */
int tang_plane_show(bool on);

/* Write n RGB565 pixels starting at (x, y), along one row: x + n is at most
 * the plane's width.  0 on success, -1 as above.  Not reentrant: one task
 * draws at a time, which is the app that owns the screen. */
int tang_plane_write(int x, int y, const uint16_t *px, int n);

#ifdef __cplusplus
}
#endif

#endif /* TANG_PLANE_H */
