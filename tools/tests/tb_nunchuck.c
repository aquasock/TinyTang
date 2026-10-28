/* TinyTang — the Wii nunchuck driver (ports/bl616/tang_nunchuck.c) on the host.
 *
 * The four hardware hooks are a model of a genuine nunchuck behind the desktop
 * core's I2C engine: it answers 0xff until the documented wake-up writes,
 * reports the identity block at 0xFA and the six report bytes at 0x00.  So
 * the state machine, the decode and the pointer mapping all run as they do on
 * the board, against the sample values measured on this part (core-log
 * entry 63): center 123/131, full scale +-98, Z bit 0, C bit 1.
 *
 *   tools/tests/test_nunchuck.sh
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tang_nunchuck.h"

static int s_failed, s_checks;
#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        s_checks++;                                                            \
        if (!(cond)) {                                                         \
            s_failed++;                                                        \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                        \
            printf(__VA_ARGS__);                                               \
            printf("\n");                                                      \
        }                                                                      \
    } while (0)

/* ---- the model ---- */
static uint8_t  s_data[6];
static uint8_t  s_id[6] = {0x00, 0x00, 0xA4, 0x20, 0x00, 0x00};
static bool     s_ready = true;
static bool     s_awake;
static bool     s_read_fails;
static bool     s_write_fails;
static int      s_wake_writes;
static uint32_t s_ms;

bool tang_nunchuck_hw_ready(void) { return s_ready; }

bool tang_nunchuck_hw_read(uint8_t reg, uint8_t *buf, unsigned n)
{
    if (s_read_fails || n > 8) {
        return false;
    }
    if (!s_awake) {
        memset(buf, 0xFF, n);          /* what an un-woken part sends */
        return true;
    }
    const uint8_t *src = reg == TANG_NUNCHUCK_REG_ID ? s_id : s_data;
    if (reg != TANG_NUNCHUCK_REG_ID && reg != TANG_NUNCHUCK_REG_DATA) {
        return false;
    }
    memcpy(buf, src, n);
    return true;
}

bool tang_nunchuck_hw_write(uint8_t reg, const uint8_t *buf, unsigned n)
{
    if (s_write_fails || n != 1) {
        return false;
    }
    if (reg == TANG_NUNCHUCK_REG_INIT && buf[0] == 0x55) {
        s_wake_writes++;
    }
    if (reg == TANG_NUNCHUCK_REG_ENC && buf[0] == 0x00) {
        s_awake = true;                /* the pair is what wakes the part */
    }
    return true;
}

uint32_t tang_nunchuck_hw_ms(void) { return s_ms; }

/* ---- helpers ---- */
static tang_nunchuck_t *fresh(void)
{
    tang_nunchuck_t *n = tang_nunchuck_state();
    memset(n, 0, sizeof *n);
    n->state = TANG_NUNCHUCK_OFF;
    n->cx = 128;
    n->cy = 128;
    memset(s_data, 0, sizeof s_data);
    s_data[0] = 128;
    s_data[1] = 128;
    s_data[5] = 0x03;                  /* both buttons released */
    s_id[4] = 0x00;                    /* nunchuck */
    s_ready = true;
    s_awake = false;
    s_read_fails = s_write_fails = false;
    s_wake_writes = 0;
    s_ms = 0;
    return n;
}

static void report(uint8_t sx, uint8_t sy, unsigned buttons)
{
    s_data[0] = sx;
    s_data[1] = sy;
    s_data[5] = (uint8_t)((s_data[5] & 0xFCu) | (buttons & 0x03u));
}

/* One poll with a zeroed mouse; returns whether a report was read. */
static bool poll_once(tang_mouse_t *m, bool console_mode)
{
    memset(m, 0, sizeof *m);
    return tang_nunchuck_poll(m, console_mode);
}

/* ---- tests ---- */
static void test_decode(void)
{
    tang_nunchuck_raw_t r;
    const uint8_t released[6] = {123, 131, 0, 0, 0, 0x53};
    const uint8_t z[6]        = {123, 131, 0, 0, 0, 0x52};
    const uint8_t c[6]        = {123, 131, 0, 0, 0, 0x51};
    const uint8_t both[6]     = {123, 131, 0, 0, 0, 0x50};

    tang_nunchuck_decode(released, &r);
    CHECK(r.sx == 123 && r.sy == 131, "axes %d/%d", r.sx, r.sy);
    CHECK(!r.z && !r.c, "both buttons released");

    tang_nunchuck_decode(z, &r);
    CHECK(r.z && !r.c, "Z is bit 0, active low");
    tang_nunchuck_decode(c, &r);
    CHECK(!r.z && r.c, "C is bit 1, active low");
    tang_nunchuck_decode(both, &r);
    CHECK(r.z && r.c, "both held");
}

static void test_identity(void)
{
    const uint8_t nunchuck[6]  = {0x00, 0x00, 0xA4, 0x20, 0x00, 0x00};
    const uint8_t classic[6]   = {0x00, 0x00, 0xA4, 0x20, 0x01, 0x01};
    const uint8_t classicpro[6]= {0x01, 0x00, 0xA4, 0x20, 0x01, 0x01};
    const uint8_t ff[6]        = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    CHECK(tang_nunchuck_is_nunchuck(nunchuck), "a nunchuck identifies as one");
    CHECK(!tang_nunchuck_is_nunchuck(classic), "a Classic is not a nunchuck");
    CHECK(!tang_nunchuck_is_nunchuck(classicpro), "a Classic Pro is not a nunchuck");
    CHECK(tang_nunchuck_all_ff(ff), "all 0xff is the un-woken report");
    CHECK(!tang_nunchuck_all_ff(nunchuck), "a live report is not all 0xff");
}

static void test_mapping(void)
{
    tang_nunchuck_t *n = fresh();
    n->centered = true;
    n->cx = 123;
    n->cy = 131;

    const struct {
        int sx, sy;
        int dx, dy;
        const char *what;
    } cases[] = {
        {123, 131,    0,    0, "at rest"},
        {131, 131,    0,    0, "dead zone edge, +8"},
        {115, 131,    0,    0, "dead zone edge, -8"},
        {221, 131,   96,    0, "full right"},
        {24,  131,  -96,    0, "full left"},
        {123, 229,    0,  -96, "full up (the part's Y grows upward)"},
        {123, 33,     0,   96, "full down"},
        {123, 135,    0,    0, "small push stays dead"},
        {172, 131,   19,    0, "half stick is a quarter of the speed"},
        {123, 82,     0,   19, "up at half stick"},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        tang_nunchuck_raw_t rr = {.sx = (uint8_t)cases[i].sx,
                                  .sy = (uint8_t)cases[i].sy};
        tang_mouse_t mm = {0};
        tang_nunchuck_apply(n, &rr, &mm, false, 0);
        CHECK(mm.dx == cases[i].dx && mm.dy == cases[i].dy,
              "%s: got %d/%d, want %d/%d", cases[i].what,
              mm.dx, mm.dy, cases[i].dx, cases[i].dy);
    }
}

static void test_buttons(void)
{
    tang_nunchuck_t *n = fresh();
    n->centered = true;
    n->cx = 123;
    n->cy = 131;

    tang_nunchuck_raw_t z = {.sx = 123, .sy = 131, .z = true};
    tang_mouse_t m = {0};
    tang_nunchuck_apply(n, &z, &m, false, 0);
    CHECK(m.buttons == TANG_MOUSE_LEFT, "Z holds the left button (buttons %u)", m.buttons);
    CHECK(m.pressed == TANG_MOUSE_LEFT, "Z's press is latched");

    tang_mouse_t m2 = {0};
    tang_nunchuck_apply(n, &z, &m2, false, 0);
    CHECK(m2.buttons == TANG_MOUSE_LEFT && m2.pressed == 0,
          "a held Z is one press, not one per poll");

    tang_nunchuck_raw_t c = {.sx = 123, .sy = 131, .c = true};
    tang_mouse_t m3 = {0};
    tang_nunchuck_apply(n, &c, &m3, false, 0);
    CHECK(m3.buttons == TANG_MOUSE_RIGHT && m3.pressed == TANG_MOUSE_RIGHT,
          "C is the right button");

    /* The Bluetooth mouse's buttons are already in the struct; the nunchuck
     * adds to them rather than replacing them. */
    tang_mouse_t m4 = {.buttons = TANG_MOUSE_LEFT};
    tang_nunchuck_apply(n, &z, &m4, false, 0);
    CHECK(m4.buttons == TANG_MOUSE_LEFT, "the nunchuck does not clear another button");
}

static void test_console_wheel(void)
{
    tang_nunchuck_t *n = fresh();
    n->centered = true;
    n->cx = 123;
    n->cy = 131;

    tang_nunchuck_raw_t up = {.sx = 123, .sy = 229};
    tang_mouse_t m = {0};
    tang_nunchuck_apply(n, &up, &m, true, 1000);
    CHECK(m.wheel == 1 && m.dx == 0 && m.dy == 0,
          "console mode: stick up is one wheel detent and no pointer movement");
    m = (tang_mouse_t){0};
    tang_nunchuck_apply(n, &up, &m, true, 1100);
    CHECK(m.wheel == 0, "a held stick does not repeat before the gate");
    m = (tang_mouse_t){0};
    tang_nunchuck_apply(n, &up, &m, true, 1140);
    CHECK(m.wheel == 1, "the gate opens after TANG_NUNCHUCK_WHEEL_MS");

    tang_nunchuck_raw_t down = {.sx = 123, .sy = 33};
    m = (tang_mouse_t){0};
    tang_nunchuck_apply(n, &down, &m, true, 2000);
    CHECK(m.wheel == -1, "stick down is one detent the other way");

    tang_nunchuck_raw_t rest = {.sx = 123, .sy = 131};
    m = (tang_mouse_t){0};
    tang_nunchuck_apply(n, &rest, &m, true, 2100);
    CHECK(m.wheel == 0, "a centred stick scrolls nothing");
    m = (tang_mouse_t){0};
    tang_nunchuck_apply(n, &up, &m, true, 2110);
    CHECK(m.wheel == 1, "returning to the middle re-arms the gate immediately");
}

static void test_center_learning(void)
{
    tang_nunchuck_t *n = fresh();
    tang_mouse_t m;
    n->centered = false;

    (void)poll_once(&m, false);            /* wake the part */
    s_ms = 40;

    report(123, 131, 0x03);
    CHECK(poll_once(&m, false), "a sample is read");
    CHECK(n->cx == 123 && n->cy == 131, "the resting sample becomes the center (%d/%d)",
          n->cx, n->cy);

    /* A push held while asking to re-learn must not become the new zero. */
    n->relearn = true;
    n->centered = false;
    report(221, 131, 0x03);
    (void)poll_once(&m, false);
    CHECK(n->cx == 123 && n->cy == 131, "an implausible center is refused");

    n->relearn = true;
    n->centered = false;
    report(120, 140, 0x03);
    (void)poll_once(&m, false);
    CHECK(n->cx == 120 && n->cy == 140, "an explicit re-learn takes the next resting sample");
}

static void test_state_machine(void)
{
    tang_nunchuck_t *n = fresh();
    tang_mouse_t m;

    s_ready = false;
    CHECK(!poll_once(&m, false), "no bus: no read");
    CHECK(n->state == TANG_NUNCHUCK_OFF, "no bus is the off state");

    s_ready = true;
    CHECK(!poll_once(&m, false), "the first poll only wakes the part");
    CHECK(s_wake_writes == 1, "the wake-up writes go out (0x55 to 0xF0)");
    CHECK(n->state == TANG_NUNCHUCK_SLEEPING, "awake but unread is sleeping");

    s_ms = 40;
    report(123, 131, 0x03);
    CHECK(poll_once(&m, false), "the next poll reads a report");
    CHECK(n->state == TANG_NUNCHUCK_READY, "a report makes it ready");

    /* Replugged: the part answers 0xff again, and is woken again. */
    s_awake = false;
    s_ms = 80;
    CHECK(!poll_once(&m, false), "0xff is not a report");
    CHECK(n->state == TANG_NUNCHUCK_SLEEPING, "0xff sends it back to sleeping");
    s_ms = 1000;
    CHECK(!poll_once(&m, false), "the retry waits");
    s_ms = 2200;
    CHECK(!poll_once(&m, false), "after the retry interval it wakes again");
    CHECK(s_wake_writes == 2, "the wake-up was repeated");
    s_ms = 2240;
    report(123, 131, 0x03);
    CHECK(poll_once(&m, false), "and reads again");
}

static void test_unsupported(void)
{
    tang_nunchuck_t *n = fresh();
    tang_mouse_t m;

    s_id[4] = 0x01;                          /* a Classic Controller */
    s_id[5] = 0x01;
    (void)poll_once(&m, false);              /* wake */
    s_ms = 40;
    report(221, 229, 0x00);                  /* a Classic's bytes, misread as Z+C */
    CHECK(!poll_once(&m, false), "a Classic reports nothing");
    CHECK(n->state == TANG_NUNCHUCK_UNSUPPORTED, "a Classic is refused");
    CHECK(m.buttons == 0 && m.dx == 0 && m.dy == 0, "and moves no pointer");
}

int main(void)
{
    test_decode();
    test_identity();
    test_mapping();
    test_buttons();
    test_console_wheel();
    test_center_learning();
    test_state_machine();
    test_unsupported();
    printf("tang_nunchuck: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}
