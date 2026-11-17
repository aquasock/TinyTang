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

#endif /* TANG_BT_OPS_H */
