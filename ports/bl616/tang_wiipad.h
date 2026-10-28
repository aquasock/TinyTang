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
 * Classic is put in data format 3 (0x03 at 0xFE) and reports eight bytes:
 * left stick X, right stick X, left stick Y, right stick Y, the two triggers,
 * then the buttons, with A as byte 7 bit 4 and B as bit 6.  A Classic report
 * decoded as a nunchuck would read as hard over, which is why the identity is
 * checked before the report is used.
 *
 * Both sticks are velocity controls: push to move, release to stop.  The left
 * stick is mapped exactly as the nunchuck's, the right stick at half the
 * counts, and both add into the one pointer.  On a Classic, A is the left
 * button and B the right.  At the shell prompt the left stick's
 * vertical axis becomes the wheel, as the Bluetooth mouse's wheel does there.
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
#define TANG_WIIPAD_REG_DATA   0x00u
#define TANG_WIIPAD_REG_ID     0xFAu
#define TANG_WIIPAD_REG_INIT   0xF0u
#define TANG_WIIPAD_REG_ENC    0xFBu
#define TANG_WIIPAD_REG_FORMAT 0xFEu   /* data format code, writable */

/* The format the Classic family is put in: 0x03, eight bytes with 8-bit
 * sticks.  A genuine Classic supports 0x01, 0x02 and 0x03; a third-party
 * receiver built for the NES/SNES Classic sends nothing but a fixed block
 * until the host selects it (WII-004), which is what TinyNES does too. */
#define TANG_WIIPAD_FORMAT3 0x03u
#define TANG_WIIPAD_CLASSIC_LEN 8u

/* Pointer counts at full deflection, one poll.  tang_pad.h counts 16 to a
 * cell on the 80x45 grid, so the left stick is six cells per 40 ms poll -- a
 * second to cross the screen held over, a quarter of that at half stick --
 * and the right stick, at half the counts, is three. */
#define TANG_WIIPAD_MAX_COUNTS   96
#define TANG_WIIPAD_RIGHT_COUNTS 48

/* Both parts' sticks are 8-bit in the formats the driver uses -- the
 * nunchuck's always, the Classic's in format 3 -- with about 98 counts of
 * travel each way on the nunchuck measured here (WII-002).  The dead zone is
 * the same proportion of the travel for both. */
#define TANG_WIIPAD_DZ_8   8
#define TANG_WIIPAD_FULL_8 98

/* While the vertical axis drives the shell's history, one detent per this
 * many milliseconds. */
#define TANG_WIIPAD_WHEEL_MS 140

/* The Classic's d-pad is its only pointing input: the receiver maps the pad's
 * analog sticks onto it and never reports their axes (WII-004), so the driver
 * reads no sticks on that part.  It works as the keyboard's arrows work here:
 * normally they are the arrow keys, and with the right shoulder held they
 * move the pointer instead, one cell a poll.  The arrows repeat as the
 * keyboard's do: this long before the first repeat, then this often. */
#define TANG_WIIPAD_ARROW_DELAY_MS 500
#define TANG_WIIPAD_ARROW_RATE_MS   50

/* The bytes a poll may produce for the desktop's input, and the screen-switch
 * request Home makes (it is not a key: the layer's toggle is firmware-side). */
#define TANG_WIIPAD_KEYS_MAX 16

typedef struct {
    char keys[TANG_WIIPAD_KEYS_MAX];
    int  n;
    bool toggle;
} tang_wii_keys_t;

/* The buttons, as a bitmask of what is held.  A and B are also the pointer's
 * buttons; the rest are the key map (WII-004). */
#define TANG_WII_BTN_A      (1u << 0)
#define TANG_WII_BTN_B      (1u << 1)
#define TANG_WII_BTN_X      (1u << 2)
#define TANG_WII_BTN_Y      (1u << 3)
#define TANG_WII_BTN_ZL     (1u << 4)
#define TANG_WII_BTN_ZR     (1u << 5)
#define TANG_WII_BTN_PLUS   (1u << 6)
#define TANG_WII_BTN_MINUS  (1u << 7)
#define TANG_WII_BTN_HOME   (1u << 8)
#define TANG_WII_BTN_L      (1u << 9)
#define TANG_WII_BTN_R      (1u << 10)
#define TANG_WII_BTN_UP     (1u << 11)
#define TANG_WII_BTN_DOWN   (1u << 12)
#define TANG_WII_BTN_LEFT   (1u << 13)
#define TANG_WII_BTN_RIGHT  (1u << 14)

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
    int      lx, ly;  /* left stick (the nunchuck's only stick) */
    int      rx, ry;  /* right stick; zero on a part without one */
    bool     left;    /* the left button: Z on a nunchuck, A on a Classic */
    bool     right;   /* the right button: C on a nunchuck, B on a Classic */
    uint32_t btn;     /* the TANG_WII_BTN_* bits that are held */
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
    uint8_t  last[8];      /* the last report, for the command */
    bool     have_last;
    uint8_t  buttons;      /* the pointer buttons last reported */
    uint32_t btn_prev;     /* the key buttons held at the last report */
    uint32_t dpad_dir;     /* the d-pad direction repeating, 0 when none */
    uint32_t dpad_next_ms; /* when the next arrow repeat is due */
    uint32_t next_try_ms;  /* when to retry a failed wake-up or read */
    uint32_t next_wheel_ms;
    uint32_t next_id_ms;   /* when to re-read the identity */
} tang_wiipad_t;

/* Forget everything but the centers: the next poll wakes the part again. */
void tang_wiipad_reset(void);

/* Read the part and merge its movement and buttons into `m`, which may
 * already carry the Bluetooth mouse.  `console_mode` turns the vertical axis
 * into wheel detents instead of pointer movement.  `k`, when not NULL,
 * receives the bytes the key map produced and Home's screen-switch request.
 * Returns true when a report was read. */
bool tang_wiipad_poll(tang_mouse_t *m, bool console_mode, tang_wii_keys_t *k);

/* ---- the pieces, for the host test ---- */
/* The report to the sticks and the two buttons, in `kind`'s packing: six
 * bytes for a nunchuck, eight for a Classic in format 3. */
void tang_wii_decode(tang_wii_kind_t kind, const uint8_t b[8],
                     tang_wii_sample_t *s);
/* Which part the six bytes at 0xFA identify, or NONE. */
tang_wii_kind_t tang_wii_identify(const uint8_t id[6]);
/* True when every byte is 0xff, which is what an un-woken part sends. */
bool tang_wiipad_all_ff(const uint8_t *b, unsigned n);
/* Add one decoded report to the mouse, exactly as poll() does after a read.
 * `k` may be NULL. */
void tang_wii_apply(tang_wiipad_t *n, const tang_wii_sample_t *s,
                    tang_mouse_t *m, bool console_mode, uint32_t now_ms,
                    tang_wii_keys_t *k);
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
