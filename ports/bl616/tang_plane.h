/* TinyTang — the desktop core's pixel plane, from the BL616 side.
 *
 * Desktop ABI 1.8 gives the desktop core a 640x360 RGB565 framebuffer that
 * HDMI shows at 2x instead of the text layer while it is on (pixplane.sv).  An
 * app that owns the whole screen draws into it, the way TinyDesk's LCD ports
 * let an app blit into their panel: the Reader first.
 *
 * Frames (desktop_link.v): 0x18 x y moves the write cursor, 0x19 streams
 * RGB565 pixels from it, high byte first, the address running on across rows;
 * 0x1A shows or hides the plane.  One frame holds at most 1021 pixels, so a
 * 640-pixel row is one frame.  At UART1's 2 Mbaud a whole screen is about
 * 2.3 s.
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

#define TANG_PLANE_WIDTH  640
#define TANG_PLANE_HEIGHT 360

/* The loaded core has the plane: the desktop core at ABI 1.8 or later. */
bool tang_plane_available(void);

/* Show the plane instead of the text layer, or hand the screen back.
 * 0 on success; -1 when the link is busy, held by a host, or the core has no
 * plane. */
int tang_plane_show(bool on);

/* Write n RGB565 pixels starting at (x, y), running on across rows.  n is at
 * most TANG_PLANE_WIDTH.  0 on success, -1 as above.  Not reentrant: one task
 * draws at a time, which is the app that owns the screen. */
int tang_plane_write(int x, int y, const uint16_t *px, int n);

#ifdef __cplusplus
}
#endif

#endif /* TANG_PLANE_H */
