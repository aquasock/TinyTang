// TinyTang — the Bluetooth LE keyboard and mouse as input sources.  See tang_ble.c.
//
// SPDX-License-Identifier: MIT

#ifndef TANG_BLE_H
#define TANG_BLE_H

#include <stdbool.h>
#include <stdint.h>

#include "tang_ble_bonds.h"
#include "tang_pad.h"

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

/* The connected mouse: its movement and wheel since the last call, and its
 * buttons now (tang_pad.h's tang_mouse_t, from the HID boot mouse report).
 * Movement and wheel are handed over once and then cleared, so nothing moved
 * between two polls is lost and nothing is applied twice.
 *
 * Returns true while a mouse is connected and set up; otherwise everything is
 * zero, so a button held when the link dropped is released. */
bool tang_ble_mouse(tang_mouse_t *out);

/* Reconnect the devices paired before the reset: reads /sd/ble/bonds.bin,
 * and if anything is paired, starts the radio, restores the keys and lets the
 * host reconnect each device when it next advertises.  Returns at once; the
 * work runs on a background task.  Called once, after the boot script. */
void tang_ble_boot(void);

/* What `ble` reports, as data, for the desktop's Bluetooth window. */
typedef enum {
    TANG_BLE_RADIO_OFF,       /* starts at boot when paired, or on a command */
    TANG_BLE_RADIO_STARTING,
    TANG_BLE_RADIO_UP,
    TANG_BLE_RADIO_FAILED,    /* see enable_error */
} tang_ble_radio_t;

/* One slot: what is connected to it now, and what is paired to it.
 * Addresses are least significant byte first, as tang_bond_t keeps them. */
typedef struct {
    const char *state;              /* idle, connecting, pairing, discovering, ready, waiting */
    bool     ready;                 /* connected and delivering input */
    bool     active;                /* not idle: addr and name are set */
    uint8_t  addr[6];
    char     name[TANG_BOND_NAME_LEN];
    bool     boot_protocol;         /* while ready: boot rather than report protocol */
    uint32_t reports;
    bool     paired;                /* paired_addr and paired_name are set */
    uint8_t  paired_addr[6];
    char     paired_name[TANG_BOND_NAME_LEN];
    bool     reconnects;            /* reconnected by itself when it advertises */
} tang_ble_slot_info_t;

typedef struct {
    tang_ble_radio_t radio;
    int      enable_error;          /* bt_enable's error while FAILED */
    uint32_t stack_heap;            /* heap the stack took at start, while UP */
    const char *pairings;           /* the card file's state, as `ble` words it */
    tang_ble_slot_info_t slot[TANG_BOND_SLOTS];   /* 0 keyboard, 1 mouse */
} tang_ble_info_t;

/* A snapshot of the radio and both slots.  Never blocks and starts nothing,
 * so a window may call it from its tick. */
void tang_ble_info(tang_ble_info_t *out);

#endif /* TANG_BLE_H */
