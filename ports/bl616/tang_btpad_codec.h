// TinyTang — Classic Bluetooth gamepads: the parts with no Bluetooth stack in
// them, so they can be checked on the host (tools/tests/test_btpad.sh).
//
// The pairings file.  The SDK keeps BR/EDR link keys in RAM only on this
// board (its flash store needs CONFIG_BT_SETTINGS, which needs a key-value
// partition the flash layout does not have -- FLS-002), so tang_btpad.c copies
// a pad's link key here after it pairs, writes /sd/root/ble/br_keys.bin, and
// puts the keys back into the host's table when the radio starts.  Explicit
// little-endian bytes, not a struct copy:
//
//     0   "TTBR"       magic
//     4   version      1
//     5   slots        TANG_BTPAD_SLOTS
//     6   reserved     0, 0
//     8   slot records, TANG_BTPAD_RECORD_LEN bytes each:
//           0  valid   1 if the slot holds a pad
//           1  kind    TANG_BTPAD_KIND_*
//           2  flags   struct bt_keys_link_key.flags
//           3  off     1 if the pad is turned off: its reconnections are
//                      refused until it is turned on (0 before core-log
//                      entry 61, which reads as on)
//           4  addr    6 bytes, bt_addr_t.val (least significant first)
//          10  key     16 bytes
//          26  name    24 bytes, NUL-terminated
//     ..  CRC-32 (tang_bonds_crc32) of every byte before it
//
// The keys are stored unencrypted, like the LE pairings (tang_ble_bonds.h).
//
// The report.  An 8BitDo pad in Switch mode, and a Switch Pro Controller,
// send the "simple" report 0x3F until told otherwise (BLE-021): after the
// HIDP header 0xA1, the ID 0x3F, two button bytes, a hat nibble and four
// little-endian 16-bit sticks.  tang_btpad_switch_decode() reads it, and
// tang_btpad_word() turns it into the pad word the desk layer takes from the
// dock's pad (tang_pad.h), so a Bluetooth pad drives the desktop the same way.
//
// SPDX-License-Identifier: MIT
#ifndef TANG_BTPAD_CODEC_H
#define TANG_BTPAD_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tang_pad.h"       /* tang_mouse_t */
#include "tang_pad_stick.h" /* the standard stick handling */
#include "tang_wiipad.h"    /* tang_wii_sample_t */

#define TANG_BTPAD_SLOTS       2
#define TANG_BTPAD_NAME_LEN    24
#define TANG_BTPAD_RECORD_LEN  50
#define TANG_BTPAD_FILE_LEN    (8 + TANG_BTPAD_SLOTS * TANG_BTPAD_RECORD_LEN + 4)
#define TANG_BTPAD_VERSION     1

/* How a pad reports.  Only the Switch layout exists so far; the Wii family
 * (BLE-022) is the next. */
#define TANG_BTPAD_KIND_SWITCH 1
/* The Wii family (BLE-022): the Wii U Pro Controller and the Wiimote.  They
 * pair with a legacy PIN and speak the Wii Remote protocol; their reports are
 * not decoded yet, `btpad watch` shows them raw. */
#define TANG_BTPAD_KIND_WII    2

typedef struct {
    bool    valid;
    uint8_t kind;
    uint8_t flags;
    bool    off;                       /* turned off: refuse its reconnections */
    uint8_t addr[6];
    uint8_t key[16];
    char    name[TANG_BTPAD_NAME_LEN];
} tang_btpad_key_t;

/* Write the file image for `k` into `out` (TANG_BTPAD_FILE_LEN bytes).
 * Returns the length written. */
size_t tang_btpad_keys_encode(const tang_btpad_key_t k[TANG_BTPAD_SLOTS], uint8_t *out);

/* Read a file image into `k`.  Returns 0, or -1 when the length, magic,
 * version, slot count or CRC is wrong, leaving every slot invalid. */
int tang_btpad_keys_decode(const uint8_t *in, size_t len, tang_btpad_key_t k[TANG_BTPAD_SLOTS]);

/* The Switch simple report, decoded.  Button bits are the report's own:
 * buttons & 0xFF is its first byte (B 0x01, A 0x02, Y 0x04, X 0x08, L 0x10,
 * R 0x20, ZL 0x40, ZR 0x80), buttons >> 8 its second (minus 0x01, plus 0x02,
 * left stick 0x04, right stick 0x08, home 0x10, capture 0x20).  The hat is
 * 0 up, clockwise to 7 up-left, 8 centred.  Sticks are as sent, centre about
 * 0x8000; Y grows downward. */
typedef struct {
    uint16_t buttons;
    uint8_t  hat;
    uint16_t lx, ly, rx, ry;
} tang_btpad_input_t;

#define TANG_BTPAD_SW_B       0x0001u
#define TANG_BTPAD_SW_A       0x0002u
#define TANG_BTPAD_SW_Y       0x0004u
#define TANG_BTPAD_SW_X       0x0008u
#define TANG_BTPAD_SW_L       0x0010u
#define TANG_BTPAD_SW_R       0x0020u
#define TANG_BTPAD_SW_ZL      0x0040u
#define TANG_BTPAD_SW_ZR      0x0080u
#define TANG_BTPAD_SW_MINUS   0x0100u
#define TANG_BTPAD_SW_PLUS    0x0200u
#define TANG_BTPAD_SW_LSTICK  0x0400u
#define TANG_BTPAD_SW_RSTICK  0x0800u
#define TANG_BTPAD_SW_HOME    0x1000u
#define TANG_BTPAD_SW_CAPTURE 0x2000u

/* The input report as received on the interrupt channel, HIDP header
 * included (a1 3f ...).  Returns true and fills `in` for a 0x3F report of at
 * least 13 bytes; false for anything else, leaving `in` alone. */
bool tang_btpad_switch_decode(const uint8_t *rep, size_t len, tang_btpad_input_t *in);

/* How far a stick must lean from 0x8000 to count as a direction in the pad
 * word: the wheel's lean, a little over a third of the travel. */
#define TANG_BTPAD_STICK_DEADZONE TANG_STICK_WHEEL_DZ

/* The pad word (tang_pad.h) for a decoded report: the D-pad or the left
 * stick are the directions, A and B are A and B.  Home is not in it: LB, the
 * desk layer's show and hide, is a tap of home (tang_btpad_tap_*), joined by
 * tang_btpad_pad_word().  The desktop's keys and pointer come from the
 * functions below, and the game's word from tang_btpad_core_bits(). */
uint16_t tang_btpad_word(const tang_btpad_input_t *in);

/* The word the core is given for this pad (FPGA_CMD_JOYSTICK): the pad word's
 * directions, A, B, X, Y, L as LB, R as RB, minus as SELECT, plus as START
 * and home as HOME (tang_pad.h), by position, so the SNES-labelled pad's
 * buttons are the SNES pad's.  ZL, ZR, the stick clicks and capture are not
 * in it. */
uint16_t tang_btpad_core_bits(const tang_btpad_input_t *in);

/* A tap of home: pressed and released with no other button pressed in
 * between, the rule TinyNES's desk_link.sv applies to the wired pad, so home
 * held with another button is a game's chord (minus quits, L saves, R
 * loads) and does not also show the desktop.  The tap is reported as LB for
 * TANG_BTPAD_TAP_MS after the release, long enough for the desk layer's
 * 40 ms poll to see its edge. */
#define TANG_BTPAD_TAP_MS 260u

typedef struct {
    bool     down;        /* home is held */
    bool     alone;       /* and nothing else has been pressed since */
    bool     tapped;      /* a tap is being reported */
    uint32_t until_ms;    /* until then */
} tang_btpad_tap_t;

/* One report's buttons (tang_btpad_input_t.buttons) at `now_ms`. */
void tang_btpad_tap_report(tang_btpad_tap_t *t, uint16_t buttons, uint32_t now_ms);

/* Whether a tap is being reported at `now_ms`. */
bool tang_btpad_tap_on(tang_btpad_tap_t *t, uint32_t now_ms);

/* The report as the wired Wii Classic Controller's sample, so a Bluetooth pad
 * drives the console and the desktop through the same key map
 * (tang_wii_apply, kind CLASSIC): A and B are the pointer's left and right
 * buttons, X Escape, Y Tab, plus Enter, minus F10, and the D-pad the arrow
 * keys.  Home is left out: the desk layer takes it as LB from the pad word,
 * and both would toggle the desktop twice.  The sticks are not in the sample;
 * tang_btpad_sticks() hands them to the standard stick handling. */
void tang_btpad_wii_sample(const tang_btpad_input_t *in, tang_wii_sample_t *s);

/* The sticks into the mouse through the desktop's standard stick handling
 * (tang_pad_stick.h), which this pad defined: on the desktop the left stick
 * moves the pointer and the right stick's vertical axis is the wheel; at the
 * console prompt the left stick's vertical axis is the wheel, which walks the
 * shell's history.  *next_wheel_ms paces the wheel. */
void tang_btpad_sticks(const tang_btpad_input_t *in, bool console_mode, uint32_t now_ms,
                       uint32_t *next_wheel_ms, tang_mouse_t *m);

/* A stick axis as signed pointer counts (tang_stick_counts on this pad's
 * 16-bit axis): 0 inside the dead zone, then in proportion out to max_counts
 * at full travel. */
#define TANG_BTPAD_POINTER_DEADZONE TANG_STICK_POINTER_DZ
#define TANG_BTPAD_POINTER_MAX_COUNTS TANG_STICK_POINTER_MAX
int tang_btpad_axis_counts(uint16_t v, int max_counts);

#endif /* TANG_BTPAD_CODEC_H */
