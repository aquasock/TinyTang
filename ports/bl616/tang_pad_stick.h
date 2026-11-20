/* TinyTang — the desktop's standard handling of a gamepad's sticks.
 *
 * Every pad that drives the desktop moves the pointer the same way, the way
 * the 8BitDo Bluetooth pad was tuned on the board (BLE-023): on the desktop
 * the left stick is the pointer, linear past a small dead zone and at most
 * TANG_STICK_POINTER_MAX counts a poll, and the right stick's vertical axis is
 * the wheel; at the shell prompt, where there is no pointer, the left stick's
 * vertical axis is the wheel, which walks the shell's history.  The 8BitDo
 * (tang_btpad_codec.c), the Wii Classic Controller and the nunchuck
 * (tang_wiipad.c) each turn their own encoding into a deflection on one scale
 * and hand it here, so the three cannot drift apart.
 *
 * A deflection runs from -TANG_STICK_FULL to TANG_STICK_FULL, positive to the
 * right and downward, as the mouse counts (tang_pad.h).  The scale is the
 * 8BitDo's own: its 16-bit axis less 0x8000.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef TANG_PAD_STICK_H
#define TANG_PAD_STICK_H

#include <stdbool.h>
#include <stdint.h>

#include "tang_pad.h"   /* tang_mouse_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Full deflection.  An 8BitDo axis reaches 0xfff0 one way and 0x0000 the
 * other, so 0x7ff0 is the shorter side and anything past it is clamped. */
#define TANG_STICK_FULL 0x7FF0

/* The pointer moves only past this deflection, then in proportion. */
#define TANG_STICK_POINTER_DZ 0x0C00

/* The pointer's speed at full deflection: three cells a 40 ms poll, which the
 * operator chose on the 8BitDo after twice that was too fast. */
#define TANG_STICK_POINTER_MAX 48

/* A wheel detent needs a lean of a little over a third of the travel, clear
 * of drift and of a resting thumb, and comes at most once per
 * TANG_STICK_WHEEL_MS; the gate re-arms when the stick returns. */
#define TANG_STICK_WHEEL_DZ 0x3000
#define TANG_STICK_WHEEL_MS 140

/* One axis as signed pointer counts: 0 inside TANG_STICK_POINTER_DZ, then in
 * proportion out to max_counts at TANG_STICK_FULL. */
int tang_stick_counts(int d, int max_counts);

/* One wheel detent while `d` leans past TANG_STICK_WHEEL_DZ, at most one per
 * TANG_STICK_WHEEL_MS kept in *next_ms: up (negative) is +1, scrolling back,
 * as wheel-up does. */
void tang_stick_wheel(int d, uint32_t now_ms, uint32_t *next_ms, tang_mouse_t *m);

/* The standard: with `console_mode` the left stick's vertical axis is the
 * wheel; otherwise the left stick moves the pointer and, when the pad has
 * one, the right stick's vertical axis is the wheel.  The axes are
 * deflections on the scale above. */
void tang_stick_apply(int lx, int ly, int ry, bool has_right, bool console_mode,
                      uint32_t now_ms, uint32_t *next_wheel_ms, tang_mouse_t *m);

/* An 8-bit axis as a deflection: (v - center) scaled so that `full` counts
 * from the center is TANG_STICK_FULL.  Not clamped: tang_stick_counts()
 * clamps, and the wheel only compares. */
int tang_stick_from_8bit(int v, int center, int full);

#ifdef __cplusplus
}
#endif

#endif /* TANG_PAD_STICK_H */
