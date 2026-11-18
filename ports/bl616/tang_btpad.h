// TinyTang — Classic Bluetooth gamepads, as the rest of the firmware sees them.
//
// tang_btpad.c is the Classic HID host (BLE-021, BLE-022): it pairs a pad,
// keeps its link key on the card, takes it back when the pad reconnects, and
// decodes its reports.  This header carries no Bluetooth types, so the desk
// layer and its host test can use it; tang_btpad_hooks.h is the side that
// tang_ble.c talks to.  In a build without Classic Bluetooth (the LE-only
// controller library) every pad reads as idle.
//
// SPDX-License-Identifier: MIT
#ifndef TANG_BTPAD_H
#define TANG_BTPAD_H

#include <stdbool.h>
#include <stdint.h>

#include "tang_pad.h"       /* tang_mouse_t */
#include "tang_wiipad.h"    /* tang_wii_keys_t */

/* The pad word (tang_pad.h) pad `pad` (0 or 1) is holding now: directions,
 * A, B and LB (home).  0 when the slot is empty, disconnected or idle.  The
 * desk layer takes LB from it, to show and hide the desktop. */
uint16_t tang_btpad_pad_word(int pad);

/* The connected pads' controls as the desktop's input, beside the wired Wii
 * pad's (tang_wiipad_poll): the same key map as the Wii Classic Controller
 * (tang_btpad_codec.h), movement, buttons and wheel merged into `m`, keys
 * appended to `k` (which may be NULL).  `console_mode` as for the Wii pad:
 * the stick walks the shell's history instead of moving a pointer.  Called
 * from the desk layer's task only. */
void tang_btpad_poll(tang_mouse_t *m, bool console_mode, tang_wii_keys_t *k);

#endif /* TANG_BTPAD_H */
