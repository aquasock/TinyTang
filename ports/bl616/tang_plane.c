/* TinyTang — the desktop core's pixel plane, from the BL616 side (tang_plane.h).
 *
 * SPDX-License-Identifier: MIT
 */
#include "tang_plane.h"

#include "tang_core_load.h"
#include "tang_fpga_link.h"

#include <stddef.h>

#define PLANE_CMD_CURSOR 0x18u
#define PLANE_CMD_PIXELS 0x19u
#define PLANE_CMD_SHOW   0x1Au

#define ABI_PLANE        0x00010008u   /* 640x360 in block RAM            */
#define ABI_PLANE_SDRAM  0x00010009u   /* 1280x720 in SDRAM, shown 1:1    */
#define REG_PLANE_SIZE   0x0Cu         /* width << 16 | height            */

/* Pixels in one 0x19 frame.  A frame carries at most 2047 bytes after its
 * header, so 1021 pixels; a 1280-pixel row goes as two halves, the core's
 * cursor running on from the first into the second. */
#define PIXELS_PER_FRAME 640

/* How long a write waits for the link.  The desk layer's mirror shares it and
 * holds it for one row of cells at a time, so a short bound is enough; a
 * longer wait would only hide a link that is not coming back. */
#define PLANE_LOCK_MS 200u

bool tang_plane_available(void)
{
    return tang_core_desktop_abi() >= ABI_PLANE;
}

/* The size, read from the core once per loaded core: the ABI word is cached
 * at load (tang_core_desktop_abi), so a change in it is a different core. */
static uint32_t s_size_abi;
static int s_w, s_h;

bool tang_plane_size(int *w, int *h)
{
    const uint32_t abi = tang_core_desktop_abi();
    if (abi < ABI_PLANE) {
        return false;
    }
    if (abi != s_size_abi || s_w == 0) {
        uint32_t v = 0;
        int rw = 0, rh = 0;
        if (tang_fpga_link_open() == 0 && !tang_fpga_pipe_open() &&
            tang_fpga_read32(REG_PLANE_SIZE, &v)) {
            rw = (int)(v >> 16);
            rh = (int)(v & 0xFFFFu);
        }
        if (rw < 1 || rw > TANG_PLANE_MAX_WIDTH || rh < 1 || rh > TANG_PLANE_MAX_HEIGHT) {
            /* No usable answer: what each ABI is known to have. */
            rw = abi >= ABI_PLANE_SDRAM ? 1280 : 640;
            rh = abi >= ABI_PLANE_SDRAM ? 720 : 360;
        }
        s_w = rw;
        s_h = rh;
        s_size_abi = abi;
    }
    *w = s_w;
    *h = s_h;
    return true;
}

static bool plane_lock(void)
{
    if (!tang_plane_available()) {
        return false;
    }
    if (tang_fpga_link_open() != 0 || tang_fpga_pipe_open()) {
        return false;
    }
    return tang_fpga_lock_within(PLANE_LOCK_MS);
}

int tang_plane_show(bool on)
{
    if (!plane_lock()) {
        return -1;
    }
    const uint8_t b = on ? 1u : 0u;
    const int rc = tang_fpga_frame(PLANE_CMD_SHOW, &b, 1);
    tang_fpga_unlock();
    return rc == 0 ? 0 : -1;
}

/* One row's bytes, big-endian.  Static: 2560 bytes do not belong on a task's
 * stack, and only the app that owns the screen draws. */
static uint8_t s_row[TANG_PLANE_MAX_WIDTH * 2];

int tang_plane_write(int x, int y, const uint16_t *px, int n)
{
    int w = 0, h = 0;
    if (px == NULL || n <= 0 || x < 0 || y < 0 || !tang_plane_size(&w, &h) ||
        x >= w || y >= h || n > w - x) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        s_row[2 * i]     = (uint8_t)(px[i] >> 8);
        s_row[2 * i + 1] = (uint8_t)px[i];
    }
    const uint8_t cursor[4] = {
        (uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(y >> 8), (uint8_t)y,
    };
    if (!plane_lock()) {
        return -1;
    }
    int rc = tang_fpga_frame(PLANE_CMD_CURSOR, cursor, sizeof cursor);
    for (int at = 0; rc == 0 && at < n; at += PIXELS_PER_FRAME) {
        const int k = n - at < PIXELS_PER_FRAME ? n - at : PIXELS_PER_FRAME;
        rc = tang_fpga_frame(PLANE_CMD_PIXELS, s_row + 2 * at, (size_t)k * 2u);
    }
    tang_fpga_unlock();
    return rc == 0 ? 0 : -1;
}
