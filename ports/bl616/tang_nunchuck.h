/* TinyTang — a Wii nunchuck as the desktop's pointer.
 *
 * The nunchuck hangs off the same USB-A I2C bus as the RTC (BRD-011), on the
 * Wii adapter's STEMMA QT chain, through the PCA9548A's channel 0 (BRD-012).
 * The desktop core's engine reads it, so no core change is needed: this file
 * is the driver and the mapping, and tang_osd_desk.c merges the result into
 * the same tang_mouse_t the Bluetooth mouse fills.
 *
 * The part is a genuine Nintendo nunchuck, and its protocol is the documented
 * one (WII-001): writes of 0x55 to register 0xF0 and 0x00 to 0xFB wake it and
 * disable encryption, six bytes at 0xFA identify it, and six at 0x00 are the
 * report -- stick X, stick Y, three accelerometer bytes, then the buttons in
 * bit 0 (Z) and bit 1 (C) of the last byte, active low.
 *
 * A part that has not been woken answers 0xff for its data: this nunchuck did
 * exactly that on the first read, and live data after the two writes, on the
 * same engine and the same read shape.  So all-0xff is treated as "asleep,
 * wake it again", never as "absent".  It is not the same symptom as the
 * Classic Controller's 0xff in BRD-011, which came after its wake-up ACKed
 * on an engine that read with a repeated START; that was the read shape, and
 * the STOP-then-read engine this one uses read the Classic correctly.
 *
 * The stick is a velocity control, not a position: push it to move, let go to
 * stop.  Z is the left button and C the right, which is the whole mouse the
 * part can be.  At the shell prompt the vertical axis becomes the wheel, as
 * the Bluetooth mouse's wheel does there: up walks back through the history.
 *
 * The mapping is a pure function of the six bytes, so the host test can drive
 * it with recorded samples (tools/tests/tb_nunchuck.c); the four hooks at the
 * end are the hardware, supplied by tang_i2c.cpp on the board.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef TANG_NUNCHUCK_H
#define TANG_NUNCHUCK_H

#include <stdbool.h>
#include <stdint.h>

#include "tang_pad.h"   /* tang_mouse_t */

#ifdef __cplusplus
extern "C" {
#endif

#define TANG_NUNCHUCK_ADDR 0x52u

/* Register addresses inside the extension controller's block. */
#define TANG_NUNCHUCK_REG_DATA 0x00u
#define TANG_NUNCHUCK_REG_ID   0xFAu
#define TANG_NUNCHUCK_REG_INIT 0xF0u
#define TANG_NUNCHUCK_REG_ENC  0xFBu

/* How far the stick has to move from its learned center before the pointer
 * moves at all, and the full-scale deflection measured on this part.  Both
 * are counts of the raw 0..255 axis. */
#define TANG_NUNCHUCK_DEADZONE 8
#define TANG_NUNCHUCK_FULL     98

/* Pointer counts at full deflection, one poll.  tang_pad.h counts 16 to a
 * cell on the 80x45 grid, so this is six cells per 40 ms poll -- about a
 * second to cross the screen held over, a quarter of that at half stick. */
#define TANG_NUNCHUCK_MAX_COUNTS 96

/* While the vertical axis drives the shell's history, one detent per this
 * many milliseconds, so a held stick walks the history rather than flooding
 * the line editor. */
#define TANG_NUNCHUCK_WHEEL_MS 140

typedef enum {
    TANG_NUNCHUCK_OFF = 0,      /* no I2C bus in the loaded core, or the link is held */
    TANG_NUNCHUCK_SLEEPING,     /* the part has not answered its wake-up yet */
    TANG_NUNCHUCK_READY,        /* a nunchuck, reporting */
    TANG_NUNCHUCK_UNSUPPORTED,  /* a Wii extension part that is not a nunchuck */
} tang_nunchuck_state_t;

/* One decoded report: the two stick axes and the two buttons. */
typedef struct {
    uint8_t sx;
    uint8_t sy;
    bool    z;      /* held */
    bool    c;      /* held */
} tang_nunchuck_raw_t;

typedef struct {
    tang_nunchuck_state_t state;
    bool     awake;        /* the wake-up writes have gone out */
    bool     identified;   /* 0xFA has been read since the last wake-up */
    bool     centered;     /* cx, cy were learned from a resting sample */
    bool     relearn;      /* `nunchuck center` asked for a new center */
    int      cx, cy;       /* the stick's center */
    uint8_t  last[6];      /* the last report, for the command */
    bool     have_last;
    uint8_t  buttons;      /* the buttons last reported, as tang_mouse_t bits */
    uint32_t next_try_ms;  /* when to retry a failed wake-up or read */
    uint32_t next_wheel_ms;
} tang_nunchuck_t;

/* Forget everything but the center: the next poll wakes the part again. */
void tang_nunchuck_reset(void);

/* Read the part and merge its movement and buttons into `m`, which may
 * already carry the Bluetooth mouse.  `console_mode` turns the vertical axis
 * into wheel detents instead of pointer movement.  Returns true when a
 * report was read. */
bool tang_nunchuck_poll(tang_mouse_t *m, bool console_mode);

/* ---- the pieces, for the host test ---- */
/* The six report bytes to the two axes and the two buttons. */
void tang_nunchuck_decode(const uint8_t b[6], tang_nunchuck_raw_t *r);
/* True when the six bytes at 0xFA are a nunchuck's identity. */
bool tang_nunchuck_is_nunchuck(const uint8_t id[6]);
/* True when every byte is 0xff, which is what an un-woken part sends. */
bool tang_nunchuck_all_ff(const uint8_t b[6]);
/* Add one decoded report to the mouse, exactly as poll() does after a read. */
void tang_nunchuck_apply(tang_nunchuck_t *n, const tang_nunchuck_raw_t *r,
                         tang_mouse_t *m, bool console_mode, uint32_t now_ms);
/* The state the module keeps, for the host test to inspect and reset. */
tang_nunchuck_t *tang_nunchuck_state(void);

/* ---- the shell command ---- */
int tang_nunchuck_register(void);

/* ---- the hardware (tang_i2c.cpp) ---- */
bool tang_nunchuck_hw_ready(void);
bool tang_nunchuck_hw_read(uint8_t reg, uint8_t *buf, unsigned n);
bool tang_nunchuck_hw_write(uint8_t reg, const uint8_t *buf, unsigned n);
uint32_t tang_nunchuck_hw_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* TANG_NUNCHUCK_H */
