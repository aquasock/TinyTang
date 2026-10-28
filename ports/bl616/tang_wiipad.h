/* TinyTang — Wii extension controllers as the desktop's pointer.
 *
 * Two parts share the adapter on the USB-A port and the same protocol: the
 * nunchuck (WII-002) and the Classic Controller (WII-003).  Both answer at
 * 0x52 on the PCA9548A's channel 0 (BRD-011, BRD-012), both are woken by
 * writing 0x55 to register 0xF0 and 0x00 to 0xFB, and both are identified by
 * the six bytes at 0xFA.  The desktop core's engine reads them, so no core
 * change is needed: this file is the driver and the mapping, and
 * tang_osd_desk.c merges the result into the same tang_mouse_t the Bluetooth
 * mouse fills.
 *
 * The nunchuck's six bytes at 0x00 are stick X, stick Y, three accelerometer
 * bytes and a last byte whose bits 0 and 1 are Z and C, active low.  The
 * Classic's are a different packing (format 0x01): a 6-bit left stick in bits
 * 5:0 of bytes 0 and 1, a 5-bit right stick split across bytes 0-2, the
 * analog triggers in bytes 2 and 3, and the buttons in bytes 4 and 5, with
 * the L and R shoulders as byte 4 bits 5 and 1.  A Classic report decoded as
 * a nunchuck would read as hard over and fire on the d-pad, which is why the
 * identity is checked before the report is used.
 *
 * Both sticks are velocity controls: push to move, release to stop.  The left
 * stick is mapped exactly as the nunchuck's, the right stick at half the
 * counts, and both add into the one pointer.  The left shoulder is the left
 * button and the right the right; on a Classic those are analog triggers, so
 * a firm pull counts even when the click at the end of the travel has not
 * quite closed.  At the shell prompt the left stick's vertical axis becomes
 * the wheel, as the Bluetooth mouse's wheel does there.
 *
 * The mapping is a pure function of the six bytes, so the host test can drive
 * it with recorded samples (tools/tests/tb_wiipad.c); the four hooks at the
 * end are the hardware, supplied by tang_i2c.cpp on the board.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef TANG_WIIPAD_H
#define TANG_WIIPAD_H

#include <stdbool.h>
#include <stdint.h>

#include "tang_pad.h"   /* tang_mouse_t */

#ifdef __cplusplus
extern "C" {
#endif

#define TANG_WIIPAD_ADDR 0x52u

/* Register addresses inside the extension controller's block. */
#define TANG_WIIPAD_REG_DATA 0x00u
#define TANG_WIIPAD_REG_ID   0xFAu
#define TANG_WIIPAD_REG_INIT 0xF0u
#define TANG_WIIPAD_REG_ENC  0xFBu

/* Pointer counts at full deflection, one poll.  tang_pad.h counts 16 to a
 * cell on the 80x45 grid, so the left stick is six cells per 40 ms poll -- a
 * second to cross the screen held over, a quarter of that at half stick --
 * and the right stick, at half the counts, is three. */
#define TANG_WIIPAD_MAX_COUNTS   96
#define TANG_WIIPAD_RIGHT_COUNTS 48

/* The nunchuck's stick is 8-bit with about 98 counts of travel each way; the
 * Classic's left stick is 6-bit with about 22 and its right 5-bit with about
 * 12, all measured on the parts fitted here (WII-002, WII-003).  The dead
 * zones are the same proportion of the travel as the nunchuck's. */
#define TANG_WIIPAD_DZ_8   8
#define TANG_WIIPAD_FULL_8 98
#define TANG_WIIPAD_DZ_6   2
#define TANG_WIIPAD_FULL_6 22
#define TANG_WIIPAD_DZ_5   1
#define TANG_WIIPAD_FULL_5 12

/* A Classic shoulder is an analog trigger whose digital click closes only at
 * the end of the travel (about 29 of 31 here).  This much pull counts as a
 * press, so a firm squeeze works without bottoming the trigger out. */
#define TANG_WIIPAD_TRIGGER_CLICK 24

/* While the vertical axis drives the shell's history, one detent per this
 * many milliseconds. */
#define TANG_WIIPAD_WHEEL_MS 140

/* How often the identity is re-read while a part is reporting, so a
 * controller swapped under power is noticed without a reboot. */
#define TANG_WIIPAD_ID_MS 5000

typedef enum {
    TANG_WII_KIND_NONE = 0,
    TANG_WII_KIND_NUNCHUCK,
    TANG_WII_KIND_CLASSIC,
} tang_wii_kind_t;

typedef enum {
    TANG_WIIPAD_OFF = 0,      /* no I2C bus in the loaded core, or the link is held */
    TANG_WIIPAD_SLEEPING,     /* the part has not answered its wake-up yet */
    TANG_WIIPAD_READY,        /* a known part, reporting */
    TANG_WIIPAD_UNSUPPORTED,  /* a Wii extension part this driver cannot use */
} tang_wii_state_t;

/* One decoded report, in the part's own counts. */
typedef struct {
    int  lx, ly;      /* left stick (the nunchuck's only stick) */
    int  rx, ry;      /* right stick; zero on a part without one */
    bool left;        /* the left button: Z on a nunchuck, L on a Classic */
    bool right;       /* the right button: C on a nunchuck, R on a Classic */
} tang_wii_sample_t;

typedef struct {
    tang_wii_kind_t kind;
    tang_wii_state_t state;
    bool     awake;        /* the wake-up writes have gone out */
    bool     identified;   /* 0xFA has been read since the last wake-up */
    bool     centered;     /* the left stick's center was learned */
    bool     rcentered;    /* the right stick's center was learned */
    bool     relearn;      /* `wiipad center` asked for new centers */
    int      cx, cy;       /* the left stick's center */
    int      crx, cry;     /* the right stick's center */
    uint8_t  last[6];      /* the last report, for the command */
    bool     have_last;
    uint8_t  buttons;      /* the buttons last reported, as tang_mouse_t bits */
    uint32_t next_try_ms;  /* when to retry a failed wake-up or read */
    uint32_t next_wheel_ms;
    uint32_t next_id_ms;   /* when to re-read the identity */
} tang_wiipad_t;

/* Forget everything but the centers: the next poll wakes the part again. */
void tang_wiipad_reset(void);

/* Read the part and merge its movement and buttons into `m`, which may
 * already carry the Bluetooth mouse.  `console_mode` turns the vertical axis
 * into wheel detents instead of pointer movement.  Returns true when a
 * report was read. */
bool tang_wiipad_poll(tang_mouse_t *m, bool console_mode);

/* ---- the pieces, for the host test ---- */
/* The six report bytes to the sticks and the two buttons, in `kind`'s
 * packing. */
void tang_wii_decode(tang_wii_kind_t kind, const uint8_t b[6],
                     tang_wii_sample_t *s);
/* Which part the six bytes at 0xFA identify, or NONE. */
tang_wii_kind_t tang_wii_identify(const uint8_t id[6]);
/* True when every byte is 0xff, which is what an un-woken part sends. */
bool tang_wiipad_all_ff(const uint8_t b[6]);
/* Add one decoded report to the mouse, exactly as poll() does after a read. */
void tang_wii_apply(tang_wiipad_t *n, const tang_wii_sample_t *s,
                    tang_mouse_t *m, bool console_mode, uint32_t now_ms);
/* The state the module keeps, for the host test to inspect and reset. */
tang_wiipad_t *tang_wiipad_state(void);

/* ---- the shell command ---- */
int tang_wiipad_register(void);

/* ---- the hardware (tang_i2c.cpp) ---- */
bool tang_wiipad_hw_ready(void);
bool tang_wiipad_hw_read(uint8_t reg, uint8_t *buf, unsigned n);
bool tang_wiipad_hw_write(uint8_t reg, const uint8_t *buf, unsigned n);
uint32_t tang_wiipad_hw_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* TANG_WIIPAD_H */
