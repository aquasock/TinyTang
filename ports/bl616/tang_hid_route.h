// TinyTang — which parser a Bluetooth HID notification belongs to.
//
// SPDX-License-Identifier: MIT
//
// This is the decision `tang_ble.c`'s notify callback makes for every report a
// paired device sends, lifted out so it can be tested without a Bluetooth
// stack.  It is the one part of the Bluetooth path that fails *silently*: a
// report routed to the wrong parser is not an error anywhere, it simply does
// nothing.  A mouse whose movement is read as a keyboard report moves nothing,
// and the console still says the reports are arriving.
//
// Two of this project's hard-won facts are encoded here (BLE-019, and the
// Logitech case in the notify callback):
//
//   * Boot protocol defines exactly two report shapes -- 8 bytes for a
//     keyboard, 3 or 4 for a mouse -- and a boot device may send nothing else,
//     so a report of one of those lengths on a slot that carries that role is
//     routed by *format* rather than by handle.
//   * A device may put those reports on a report characteristic instead of
//     its boot input.  The Rii K06's combo does, and so does a Logitech mouse
//     in the mouse slot, which reports on its report characteristic and never
//     on the boot input; a strict handle match left its movement unread while
//     `watch` showed the reports arriving.
//
// So a report is routed when its handle is the configured one for that role,
// *or* when its length is that role's boot shape and the slot carries the
// role.  A combo slot carries both roles and relies on the lengths to keep the
// two apart.

#ifndef TANG_HID_ROUTE_H
#define TANG_HID_ROUTE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The three slots a paired device can occupy.  The values are part of the
 * contract: tang_ble.c indexes its per-slot arrays with them. */
enum {
    TANG_HID_SLOT_KBD = 0,
    TANG_HID_SLOT_MOUSE = 1,
    TANG_HID_SLOT_COMBO = 2,
};

/* Where a report goes.  NONE means no parser claims it. */
typedef enum {
    TANG_HID_ROUTE_NONE = 0,
    TANG_HID_ROUTE_KEYBOARD,
    TANG_HID_ROUTE_MOUSE,
} tang_hid_route_t;

#define TANG_HID_BOOT_KBD_LEN 8
#define TANG_HID_BOOT_MOUSE_LEN 3
#define TANG_HID_BOOT_MOUSE_LEN_WHEEL 4

/* Decide which parser takes a notification.
 *
 *   slot          TANG_HID_SLOT_*, the slot the device was paired into
 *   value_handle  the characteristic the report arrived on
 *   kbd_handle    the slot's configured keyboard input, 0 when it has none
 *   mouse_handle  the slot's configured mouse input, 0 when it has none
 *   length        the report's length in bytes
 *
 * A configured handle of 0 means "not known yet", which is the state during
 * service discovery; it must never match a real handle, because handle 0 is
 * not a valid ATT handle. */
tang_hid_route_t tang_hid_route(int slot, uint16_t value_handle,
                                uint16_t kbd_handle, uint16_t mouse_handle,
                                uint16_t length);

/* The name of a route, for a log line. */
const char *tang_hid_route_name(tang_hid_route_t route);

#ifdef __cplusplus
}
#endif

#endif /* TANG_HID_ROUTE_H */
