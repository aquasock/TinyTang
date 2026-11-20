// TinyTang — what tang_ble.c and tang_btpad.c call of each other.
//
// One Bluetooth host serves both: tang_ble.c owns the radio, the connection
// callbacks and the background task, and hands every Classic (BR/EDR) link
// to tang_btpad.c, which owns the pads.  Only in a build with
// CONFIG_BT_BREDR.
//
// SPDX-License-Identifier: MIT
#ifndef TANG_BTPAD_HOOKS_H
#define TANG_BTPAD_HOOKS_H

#if defined(CONFIG_BT_BREDR)

#include <stdbool.h>

#include "bluetooth.h"
#include "conn.h"
#include "tdsh.h"

/* ---------------------------------------------------- tang_btpad.c provides */

/* Whether a connection is a Classic link, which tang_btpad.c handles. */
bool tang_btpad_is_br(struct bt_conn *conn);

/* The connection callbacks, for Classic links. */
void tang_btpad_connected(struct bt_conn *conn, u8_t err);
void tang_btpad_disconnected(struct bt_conn *conn, u8_t reason);
void tang_btpad_security(struct bt_conn *conn, bt_security_t level, enum bt_security_err err);
void tang_btpad_paired(struct bt_conn *conn, bool bonded, int failed);

/* A legacy PIN request on a Classic link (the Wii family pairs this way):
 * the auth callbacks' pincode_entry.  The reply is sent later, from the
 * background task. */
void tang_btpad_pincode(struct bt_conn *conn, bool highsec);

/* At boot, before the radio: read the pairings from the card.  True when a
 * pad is saved, so the radio should start for it to reconnect. */
bool tang_btpad_load(void);

/* Each time the radio has come up: the saved link keys into the host's
 * table, the HID PSM servers, and page scan, so a woken pad can connect. */
void tang_btpad_radio_up(void);

/* The background task's share: save the pairings when they changed and open
 * the HID channels a link is waiting for.  tang_btpad_busy() says whether it
 * should be called again soon (a link is waiting) rather than on a signal. */
void tang_btpad_work(void);
bool tang_btpad_busy(void);

/* The shell commands. */
int tang_btpad_cmd(tdsh_session_t *session, int argc, char **argv);
int tang_btscan_cmd(tdsh_session_t *session, int argc, char **argv);

/* ----------------------------------------------------- tang_ble.c provides */

/* Bring the radio up (once), as `ble` does; 0 when it is up. */
int tang_ble_radio_start(bool verbose);

/* The one-change-at-a-time claim the LE commands and the Bluetooth window
 * share: a pairing holds it while it scans and connects. */
bool tang_ble_claim(void);
void tang_ble_release(void);

/* Why the Bluetooth window may not start the radio now (it failed before, or
 * too little heap is free), or NULL.  The pad jobs check it as the LE scan
 * does, so the window never starts the radio into a heap it would exhaust. */
const char *tang_ble_radio_refusal(void);

/* Wake the background task to call tang_btpad_work(). */
void tang_ble_btpad_signal(void);

#endif /* CONFIG_BT_BREDR */
#endif /* TANG_BTPAD_HOOKS_H */
