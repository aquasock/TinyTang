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
 * A and B, and LB for a moment after a tap of home (tang_btpad_codec.h).  0
 * when the slot is empty, disconnected or idle.  The desk layer takes LB from
 * it, to show and hide the desktop. */
uint16_t tang_btpad_pad_word(int pad);

/* The word the game is given for pad `pad` (FPGA_CMD_JOYSTICK): the SNES
 * order with HOME in bit 12 (tang_pad.h), home held as a level, so a core sees
 * its chords.  0 when the slot is empty, disconnected or idle. */
uint16_t tang_btpad_core_word(int pad);

/* The connected pads' controls as the desktop's input, beside the wired Wii
 * pad's (tang_wiipad_poll): the same key map as the Wii Classic Controller
 * (tang_btpad_codec.h), movement, buttons and wheel merged into `m`, keys
 * appended to `k` (which may be NULL).  `console_mode` as for the Wii pad:
 * the stick walks the shell's history instead of moving a pointer.  Called
 * from the desk layer's task only. */
void tang_btpad_poll(tang_mouse_t *m, bool console_mode, tang_wii_keys_t *k);

/* ------------------------------------------------- the Bluetooth window's side
 *
 * The same contract as tang_ble.h's window calls: a request returns at once,
 * NULL when the job was handed to tang_ble.c's background task or why it was
 * refused, and progress and the outcome are read with tang_btpad_info().  The
 * shell's `btpad pair`, `on` and `off` run the same jobs and wait for them. */

#define TANG_BTPAD_PADS       2
#define TANG_BTPAD_FOUND_MAX  16
#define TANG_BTPAD_TEXT_LEN   24

typedef enum {
    TANG_BTPAD_JOB_NONE,
    TANG_BTPAD_JOB_SCAN,
    TANG_BTPAD_JOB_PAIR,
    TANG_BTPAD_JOB_ON,
    TANG_BTPAD_JOB_OFF,
    TANG_BTPAD_JOB_FORGET,
} tang_btpad_job_t;

/* One device a Classic inquiry found.  Addresses least significant byte first. */
typedef struct {
    uint8_t addr[6];
    uint8_t cod[3];                 /* Class of Device, as the inquiry gives it */
    int8_t  rssi;
    bool    pad;                    /* the class is a gamepad or joystick */
    bool    peripheral;             /* the major class is peripheral (HID) */
    char    name[TANG_BTPAD_TEXT_LEN];
} tang_btpad_found_t;

typedef struct {
    const char *state;              /* idle, waiting, connecting, opening channels, ready, off */
    bool     saved;                 /* addr, name and wii describe the pairing */
    uint8_t  addr[6];
    char     name[TANG_BTPAD_TEXT_LEN];
    bool     wii;                   /* paired by legacy PIN: the Wii family */
    bool     off;                   /* its reconnections are refused */
    bool     linked;                /* a link is up (or coming up) */
    bool     ready;                 /* reporting */
    uint32_t reports;
    char     last[72];              /* its latest note, without "pad N: " */
} tang_btpad_slot_info_t;

typedef struct {
    bool     available;             /* false: the LE-only build, no Classic host */
    const char *pairings;           /* the card file's state, as `btpad` words it */
    tang_btpad_job_t job;           /* the latest job */
    bool     job_running;
    bool     job_failed;
    char     job_msg[64];
    uint32_t scan_ms_left;          /* while an inquiry runs */
    tang_btpad_slot_info_t pad[TANG_BTPAD_PADS];
} tang_btpad_info_t;

/* A snapshot; never blocks and starts nothing. */
void tang_btpad_info(tang_btpad_info_t *out);

/* A Classic inquiry of about 10 s, starting the radio if it is off. */
const char *tang_btpad_scan_start(void);

/* The last inquiry's devices, strongest first; -1 while one runs. */
int tang_btpad_scan_results(tang_btpad_found_t *out, int max);

/* Pair the pad at `addr` into pad slot `pad` (0 or 1), replacing that slot's
 * pairing.  `name` may be NULL. */
const char *tang_btpad_pair_start(int pad, const uint8_t addr[6], const char *name);

/* On: the pad reconnects by itself when woken.  Off: its link is dropped and
 * its reconnections refused, and the pairing is kept.  Kept on the card. */
const char *tang_btpad_set_on(int pad, bool on);

/* Drop the pad's link and delete its pairing from the board and the card. */
const char *tang_btpad_forget(int pad);

#endif /* TANG_BTPAD_H */
