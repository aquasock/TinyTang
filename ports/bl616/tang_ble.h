// TinyTang — the Bluetooth LE keyboard as an input source.  See tang_ble.c.
//
// SPDX-License-Identifier: MIT

#ifndef TANG_BLE_H
#define TANG_BLE_H

#include <stdbool.h>
#include <stdint.h>

/* The connected keyboard's current HID boot report: byte 0 the modifiers,
 * byte 1 reserved, bytes 2..7 the usages -- the layout the wired link's 0x08
 * report has, so tang_key.c takes either.
 *
 * Returns true while a keyboard is connected and set up, and the report is
 * then live.  A BLE keyboard sends only when its state changes, so liveness
 * is the connection rather than a heartbeat: the link's supervision timeout
 * drops a keyboard that has gone, and from then on this returns false with an
 * all-zero report, so a key held at the moment the link died is released
 * rather than repeated. */
bool tang_ble_keyboard(uint8_t out[8]);

#endif /* TANG_BLE_H */
