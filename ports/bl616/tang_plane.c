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

/* How long a write waits for the link.  The desk layer's mirror shares it and
 * holds it for one row of cells at a time, so a short bound is enough; a
 * longer wait would only hide a link that is not coming back. */
#define PLANE_LOCK_MS 200u

bool tang_plane_available(void)
{
    return tang_core_desktop_abi() >= 0x00010008u;
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

/* One row's bytes, big-endian.  Static: 1280 bytes do not belong on a task's
 * stack, and only the app that owns the screen draws. */
static uint8_t s_row[TANG_PLANE_WIDTH * 2];

int tang_plane_write(int x, int y, const uint16_t *px, int n)
{
    if (px == NULL || n <= 0 || n > TANG_PLANE_WIDTH || x < 0 || y < 0 ||
        x >= TANG_PLANE_WIDTH || y >= TANG_PLANE_HEIGHT) {
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
    if (rc == 0) {
        rc = tang_fpga_frame(PLANE_CMD_PIXELS, s_row, (size_t)n * 2u);
    }
    tang_fpga_unlock();
    return rc == 0 ? 0 : -1;
}
