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
// It lives in the port rather than in td_sysinfo_t because that header is
// inside the tinydesk submodule, and this project already carries four patches
// against it (TOOL-010: a patch stops being recognised once a later cycle edits
// the lines it added).  Adding a `bt` member beside `net` is an upstream
// change; until it happens, td_bt_ops in tang_bt_port.c is where the port
// publishes this table.
//
// The names here are td_net_ops_t's where a meaning exists: status, scan,
// scan_results, forget.  Two differ because Bluetooth's nouns do -- `pair`
// takes a device and the slot to put it in, and `paired` lists what is already
// stored, where Wi-Fi has connect/disconnect against a network.

#ifndef TANG_BT_OPS_H
#define TANG_BT_OPS_H

#include <stdbool.h>
#include <stdint.h>

/* Slots: 0 keyboard, 1 mouse, 2 combo (keyboard and touchpad in one device).
 * A port with one slot uses 0; the table does not care how many there are,
 * only that `slot` is below TANG_BT_SLOTS. */
#define TANG_BT_SLOTS      3
#define TANG_BT_NAME_LEN   24
#define TANG_BT_ADDR_LEN   6
#define TANG_BT_MAX_DEVICES 24
#define TANG_BT_MESSAGE_LEN 64

/* One slot: what is connected to it now, and what is paired to it.  Addresses
 * are least significant byte first, as the board keeps them. */
typedef struct {
    const char *state;      /* idle, connecting, pairing, discovering, ready, waiting */
    bool active;            /* not idle: addr and name are set */
    bool ready;             /* connected and delivering input */
    bool connected;         /* an alias of ready, for callers that think in
                             * terms of a link rather than of input */
    uint8_t addr[TANG_BT_ADDR_LEN];
    char name[TANG_BT_NAME_LEN];
    bool boot_protocol;     /* HID: boot rather than report protocol */
    uint32_t reports;       /* input reports delivered */
    bool paired;            /* a pairing is stored for this slot */
    uint8_t paired_addr[TANG_BT_ADDR_LEN];
    char paired_name[TANG_BT_NAME_LEN];
    bool reconnects;        /* reconnects by itself when it advertises */
    char last[TANG_BT_MESSAGE_LEN + 32];   /* its latest event, in words */
} td_bt_slot_t;

/* The radio's lifecycle, as the window shows it. */
typedef enum {
    TD_BT_RADIO_OFF,        /* starts at boot when devices are paired, or on a scan */
    TD_BT_RADIO_STARTING,
    TD_BT_RADIO_UP,
    TD_BT_RADIO_FAILED,     /* see enable_error */
} td_bt_radio_t;

/* Everything the radio is doing, as one snapshot.  Never blocks and starts
 * nothing, so a window may call it from its periodic tick -- the contract
 * td_net_ops_t's status() has. */
typedef struct {
    td_bt_radio_t radio;
    bool radio_up;          /* the stack is running */
    int  enable_error;      /* the stack's error while radio is FAILED */
    uint32_t stack_heap;    /* heap the stack took at start, while radio is UP */
    const char *pairings;   /* the pairing store's state, in words */
    bool busy;              /* a scan, pair or connect is in progress */
    bool job_failed;        /* the last operation failed */
    char message[TANG_BT_MESSAGE_LEN];  /* outcome of the last operation */
    uint32_t scan_ms_left;  /* while a scan runs, how long it has left */
    int  paired;            /* slots holding a stored pairing */
    int  connected;         /* slots delivering input now */
    /* The radio cannot run beside Wi-Fi on this board: one Bluetooth LE stack
     * and no room for both (BLE-002).  A port that can run both reports
     * false; one that cannot says so here rather than failing at runtime. */
    bool wifi_exclusive;
    td_bt_slot_t slot[TANG_BT_SLOTS];
} td_bt_status_t;

/* What a discovered device looks like to a pairing menu. */
typedef enum {
    TD_BT_KIND_OTHER,       /* gave a name but advertises no HID service */
    TD_BT_KIND_HID,         /* advertises the HID service */
    TD_BT_KIND_KEYBOARD,    /* HID appearance: keyboard */
    TD_BT_KIND_MOUSE,       /* HID appearance: mouse */
} td_bt_kind_t;

/* One device seen by the last scan. */
typedef struct {
    uint8_t  addr[TANG_BT_ADDR_LEN];   /* least significant byte first */
    uint8_t  addr_type;
    int8_t   rssi;                     /* dBm, the strongest heard */
    td_bt_kind_t kind;
    char     name[TANG_BT_NAME_LEN];
} td_bt_device_t;

/* One slot's stored pairing. */
typedef struct {
    int      slot;
    uint8_t  addr[TANG_BT_ADDR_LEN];
    char     name[TANG_BT_NAME_LEN];
    bool     connected;      /* delivering input now */
    bool     reconnects;     /* reconnects by itself when it advertises */
} td_bt_paired_t;

typedef struct {
    /* The radio, the slots and the last operation. */
    void (*status)(td_bt_status_t *out);

    /* Listen for a while and collect devices.  False when the request was
     * refused -- the radio is busy, or there is too little heap to start it --
     * in which case status().message says why. */
    bool (*scan)(void);

    /* The last scan's devices, strongest first; -1 while a scan runs. */
    int (*scan_results)(td_bt_device_t *out, int max);

    /* Connect `dev` and store it in `slot`, replacing that slot's pairing. */
    bool (*pair)(int slot, const td_bt_device_t *dev);

    /* Let go of a slot's device without forgetting it: it stops being used
     * until the slot is turned back on or repaired. */
    bool (*disconnect)(int slot);

    /* The stored pairings, one per slot that has one; -1 when the request
     * cannot be answered yet. */
    int (*paired)(td_bt_paired_t *out, int max);

    /* Delete a slot's pairing from the board and the card. */
    bool (*forget)(int slot);

    /* Whether a slot's device reconnects by itself. */
    bool (*set_reconnect)(int slot, bool on);
} td_bt_ops_t;

/* The port's implementation, or NULL when this port has no Bluetooth. */
const td_bt_ops_t *td_bt_port_ops(void);

#endif /* TANG_BT_OPS_H */
