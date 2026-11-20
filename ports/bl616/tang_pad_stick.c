/* TinyTang — the desktop's standard handling of a gamepad's sticks
 * (tang_pad_stick.h).
 *
 * SPDX-License-Identifier: MIT
 */
#include "tang_pad_stick.h"

static int clamp_full(int d)
{
    if (d > TANG_STICK_FULL) {
        return TANG_STICK_FULL;
    }
    if (d < -TANG_STICK_FULL) {
        return -TANG_STICK_FULL;
    }
    return d;
}

int tang_stick_counts(int d, int max_counts)
{
    d = clamp_full(d);
    const int sign = d < 0 ? -1 : 1;
    const int mag = d < 0 ? -d : d;
    if (mag <= TANG_STICK_POINTER_DZ) {
        return 0;
    }
    return sign * (int)(((long)(mag - TANG_STICK_POINTER_DZ) * max_counts) /
                        (TANG_STICK_FULL - TANG_STICK_POINTER_DZ));
}

void tang_stick_wheel(int d, uint32_t now_ms, uint32_t *next_ms, tang_mouse_t *m)
{
    if (d <= -TANG_STICK_WHEEL_DZ || d >= TANG_STICK_WHEEL_DZ) {
        if ((int32_t)(now_ms - *next_ms) >= 0) {
            m->wheel += d < 0 ? 1 : -1;
            *next_ms = now_ms + TANG_STICK_WHEEL_MS;
        }
    } else {
        *next_ms = now_ms;
    }
}

void tang_stick_apply(int lx, int ly, int ry, bool has_right, bool console_mode,
                      uint32_t now_ms, uint32_t *next_wheel_ms, tang_mouse_t *m)
{
    if (console_mode) {
        tang_stick_wheel(ly, now_ms, next_wheel_ms, m);
        return;
    }
    m->dx += tang_stick_counts(lx, TANG_STICK_POINTER_MAX);
    m->dy += tang_stick_counts(ly, TANG_STICK_POINTER_MAX);
    if (has_right) {
        tang_stick_wheel(ry, now_ms, next_wheel_ms, m);
    }
}

int tang_stick_from_8bit(int v, int center, int full)
{
    if (full <= 0) {
        return 0;
    }
    return (int)(((long)(v - center) * TANG_STICK_FULL) / full);
}
