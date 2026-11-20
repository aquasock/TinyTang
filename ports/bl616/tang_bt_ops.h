// TinyTang — the port's Bluetooth surface, shaped so TinyDesk can adopt it.
//
// SPDX-License-Identifier: MIT
//
// tinydesk-project/tinydesk issue #7 asked where Bluetooth belongs.  The
// maintainer's answer: the port builds it for now, and if the port's interface
// follows the shape of td_net_ops_t in td_sysinfo.h -- scan, pair, list paired
// devices, forget, status -- it can become the upstream interface later without
// renaming anything.
//
// This is that interface.  It is deliberately a small, flat table of function
// pointers with the same asynchrony contract as td_net_ops_t: every call
// returns at once, slow work runs in the background, and its outcome appears in
// status().  A caller never blocks on the radio.
//
// Upstream adopted it.  TinyDesk 0.4 moved these types into its own
// td_sysinfo.h under the same names (upstream 624847f, "Bluetooth window and
// its test, from TinyTang"), with the limits renamed TD_BT_*.  From TinyDesk
// 0.5.2 this header takes the types from there rather than declaring a second,
// conflicting copy, and keeps the TANG_BT_* names as aliases so the port's own
// code reads as before.  The asynchrony contract above is unchanged.

#ifndef TANG_BT_OPS_H
#define TANG_BT_OPS_H

#include <stdbool.h>
#include <stdint.h>

#include "tinydesk/td_sysinfo.h"

/* Slots: 0 keyboard, 1 mouse, 2 combo (keyboard and touchpad in one device). */
#define TANG_BT_SLOTS       TD_BT_SLOTS
#define TANG_BT_NAME_LEN    TD_BT_NAME_LEN
#define TANG_BT_ADDR_LEN    TD_BT_ADDR_LEN
#define TANG_BT_MAX_DEVICES TD_BT_MAX_DEVICES
#define TANG_BT_MESSAGE_LEN TD_BT_MESSAGE_LEN

/* The port's implementation, or NULL when this port has no Bluetooth. */
const td_bt_ops_t *td_bt_port_ops(void);

/* ---- Classic gamepads (core-log entry 61)
 *
 * The window's fourth and fifth slots are the two Classic pads of
 * tang_btpad.c.  td_bt_ops_t is TinyDesk's now, with three LE slots and a
 * scan() that cannot ask for a Classic inquiry, so the pads have a table of
 * their own beside it, in the same shape and under the same contract: every
 * call returns at once, the reason for a refusal is in status().message, and
 * scan_results() answers -1 while an inquiry runs.  A slot is the
 * td_bt_slot_t the window already draws; a device is td_bt_device_t with
 * classic set, kind TD_BT_KIND_HID for a gamepad or joystick and
 * TD_BT_KIND_OTHER for another peripheral. */

#define TANG_BT_PADS 2

typedef struct {
    bool available;                /* false: this build has no Classic host */
    const char *pairings;          /* the pad file's state, in words */
    bool busy;                     /* a pad job is running */
    bool job_failed;
    char message[TANG_BT_MESSAGE_LEN];
    uint32_t scan_ms_left;
    td_bt_slot_t slot[TANG_BT_PADS];
} tang_bt_pad_status_t;

typedef struct {
    void (*status)(tang_bt_pad_status_t *out);
    bool (*scan)(void);
    int (*scan_results)(td_bt_device_t *out, int max);
    bool (*pair)(int pad, const td_bt_device_t *dev);
    bool (*set_reconnect)(int pad, bool on);
    bool (*forget)(int pad);
} tang_bt_pad_ops_t;

/* The port's pad table; never NULL (status() says when there are no pads). */
const tang_bt_pad_ops_t *tang_bt_pad_ops(void);

#endif /* TANG_BT_OPS_H */
