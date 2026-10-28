/* TinyTang — the Wii extension controller driver (ports/bl616/tang_wiipad.c) on
 * the host.
 *
 * The four hardware hooks are a model of a genuine part behind the desktop
 * core's I2C engine: it answers 0xff until the documented wake-up writes,
 * reports the identity block at 0xFA and the six report bytes at 0x00.  The
 * sample bytes are the ones measured on this board (core-log entries 63 and
 * 64): the nunchuck at 123/131 with Z and C, and the Classic Controller at
 * 32/32 and 16/16 with its shoulders.
 *
 *   tools/tests/test_wiipad.sh
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tang_wiipad.h"

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
static int      s_wake_writes;
static uint32_t s_ms;

bool tang_wiipad_hw_ready(void) { return s_ready; }

bool tang_wiipad_hw_read(uint8_t reg, uint8_t *buf, unsigned n)
{
    if (s_read_fails || n > 8) {
        return false;
    }
    if (!s_awake) {
        memset(buf, 0xFF, n);          /* what an un-woken part sends */
        return true;
    }
    if (reg != TANG_WIIPAD_REG_ID && reg != TANG_WIIPAD_REG_DATA) {
        return false;
    }
    memcpy(buf, reg == TANG_WIIPAD_REG_ID ? s_id : s_data, n);
    return true;
}

bool tang_wiipad_hw_write(uint8_t reg, const uint8_t *buf, unsigned n)
{
    if (n != 1) {
        return false;
    }
    if (reg == TANG_WIIPAD_REG_INIT && buf[0] == 0x55) {
        s_wake_writes++;
    }
    if (reg == TANG_WIIPAD_REG_ENC && buf[0] == 0x00) {
        s_awake = true;                /* the pair is what wakes the part */
    }
    return true;
}

uint32_t tang_wiipad_hw_ms(void) { return s_ms; }

/* ---- helpers ---- */
static tang_wiipad_t *fresh(void)
{
    tang_wiipad_reset();
    tang_wiipad_t *n = tang_wiipad_state();
    const uint8_t nunchuck_id[6] = {0x00, 0x00, 0xA4, 0x20, 0x00, 0x00};
    memcpy(s_id, nunchuck_id, sizeof s_id);
    memset(s_data, 0, sizeof s_data);
    s_data[0] = 128;
    s_data[1] = 128;
    s_data[5] = 0x03;                  /* both buttons released */
    s_ready = true;
    s_awake = false;
    s_read_fails = false;
    s_wake_writes = 0;
    s_ms = 0;
    return n;
}

static void use_classic(void)
{
    const uint8_t classic_id[6] = {0x00, 0x00, 0xA4, 0x20, 0x01, 0x01};
    memcpy(s_id, classic_id, sizeof s_id);
}

static void report(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                   uint8_t b4, uint8_t b5)
{
    s_data[0] = b0;
    s_data[1] = b1;
    s_data[2] = b2;
    s_data[3] = b3;
    s_data[4] = b4;
    s_data[5] = b5;
}

/* Nunchuck report: stick plus the two buttons in byte 5's low two bits. */
static void nunchuck_report(uint8_t sx, uint8_t sy, unsigned buttons)
{
    report(sx, sy, 0, 0, 0, (uint8_t)(0xFCu | (buttons & 0x03u)));
}

static bool poll_once(tang_mouse_t *m, bool console_mode)
{
    memset(m, 0, sizeof *m);
    return tang_wiipad_poll(m, console_mode);
}

/* Wake the part and read one report, with the model as it stands. */
static bool read_once(tang_mouse_t *m)
{
    (void)poll_once(m, false);         /* wake */
    s_ms += 40;
    return poll_once(m, false);
}

/* ---- tests ---- */
static void test_decode_nunchuck(void)
{
    tang_wii_sample_t s;

    tang_wii_decode(TANG_WII_KIND_NUNCHUCK, (const uint8_t[6]){123, 131, 0, 0, 0, 0x53}, &s);
    CHECK(s.lx == 123 && s.ly == 131, "axes %d/%d", s.lx, s.ly);
    CHECK(!s.left && !s.right, "both buttons released");
    CHECK(s.rx == 0 && s.ry == 0, "a nunchuck has no right stick");

    tang_wii_decode(TANG_WII_KIND_NUNCHUCK, (const uint8_t[6]){123, 131, 0, 0, 0, 0x52}, &s);
    CHECK(s.left && !s.right, "Z is bit 0, active low");
    tang_wii_decode(TANG_WII_KIND_NUNCHUCK, (const uint8_t[6]){123, 131, 0, 0, 0, 0x51}, &s);
    CHECK(!s.left && s.right, "C is bit 1, active low");
    tang_wii_decode(TANG_WII_KIND_NUNCHUCK, (const uint8_t[6]){123, 131, 0, 0, 0, 0x50}, &s);
    CHECK(s.left && s.right, "both held");
}

static void test_decode_classic(void)
{
    tang_wii_sample_t s;

    /* The measured rest report. */
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0xa0, 0x20, 0x10, 0x61, 0xff, 0xff}, &s);
    CHECK(s.lx == 32 && s.ly == 32, "classic left stick at rest %d/%d", s.lx, s.ly);
    CHECK(s.rx == 16 && s.ry == 16, "classic right stick at rest %d/%d", s.rx, s.ry);
    CHECK(!s.left && !s.right, "classic shoulders released");

    /* Left stick: left, right, up, down (the values read on the bench). */
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0x86, 0x1e, 0x10, 0x61, 0xff, 0xff}, &s);
    CHECK(s.lx == 6 && s.ly == 30, "classic left stick left %d/%d", s.lx, s.ly);
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0xb6, 0x24, 0x10, 0x61, 0xff, 0xff}, &s);
    CHECK(s.lx == 54 && s.ly == 36, "classic left stick right %d/%d", s.lx, s.ly);
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0xa0, 0x3a, 0x10, 0x61, 0xff, 0xff}, &s);
    CHECK(s.ly == 58, "classic left stick up %d", s.ly);
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0x9e, 0x06, 0x10, 0x61, 0xff, 0xff}, &s);
    CHECK(s.ly == 6, "classic left stick down %d", s.ly);

    /* Right stick: its five bits are split across bytes 0, 1 and 2. */
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0xe0, 0x9f, 0x10, 0x61, 0xff, 0xff}, &s);
    CHECK(s.rx == 28, "classic right stick left %d", s.rx);
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0x1f, 0xa0, 0x10, 0x61, 0xff, 0xff}, &s);
    CHECK(s.rx == 4, "classic right stick right %d", s.rx);
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0x9f, 0x20, 0x9c, 0x61, 0xff, 0xff}, &s);
    CHECK(s.ry == 28, "classic right stick up %d", s.ry);
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0x9f, 0x20, 0x03, 0x61, 0xff, 0xff}, &s);
    CHECK(s.ry == 3, "classic right stick down %d", s.ry);

    /* Shoulders: the end-of-travel click, and a firm pull short of it. */
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0x9f, 0x20, 0x70, 0xc1, 0xdf, 0xff}, &s);
    CHECK(s.left && !s.right, "L closes into the left button");
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0x9f, 0x20, 0x10, 0x7d, 0xfd, 0xff}, &s);
    CHECK(!s.left && s.right, "R closes into the right button");

    /* Analog only: 25 of 31 counts is a press, 10 is not. */
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0x9f, 0x20, 0x60, 0x20, 0xff, 0xff}, &s);
    CHECK(s.left, "a firm pull counts without the click");
    tang_wii_decode(TANG_WII_KIND_CLASSIC, (const uint8_t[6]){0x9f, 0x20, 0x20, 0x40, 0xff, 0xff}, &s);
    CHECK(!s.left, "a light pull does not");
}

static void test_identity(void)
{
    const uint8_t nunchuck[6]   = {0x00, 0x00, 0xA4, 0x20, 0x00, 0x00};
    const uint8_t classic[6]    = {0x00, 0x00, 0xA4, 0x20, 0x01, 0x01};
    const uint8_t classicpro[6] = {0x01, 0x00, 0xA4, 0x20, 0x01, 0x01};
    const uint8_t guitar[6]     = {0x00, 0x00, 0xA4, 0x20, 0x01, 0x03};
    const uint8_t ff[6]         = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    CHECK(tang_wii_identify(nunchuck) == TANG_WII_KIND_NUNCHUCK, "a nunchuck identifies as one");
    CHECK(tang_wii_identify(classic) == TANG_WII_KIND_CLASSIC, "a Classic identifies as one");
    CHECK(tang_wii_identify(classicpro) == TANG_WII_KIND_CLASSIC, "so does a Classic Pro");
    CHECK(tang_wii_identify(guitar) == TANG_WII_KIND_NONE, "a guitar is neither");
    CHECK(tang_wiipad_all_ff(ff), "all 0xff is the un-woken report");
    CHECK(!tang_wiipad_all_ff(nunchuck), "a live report is not all 0xff");
}

static void test_mapping_nunchuck(void)
{
    tang_wiipad_t *n = fresh();
    n->kind = TANG_WII_KIND_NUNCHUCK;
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
        {123, 229,    0,  -96, "full up (Y grows upward)"},
        {123, 33,     0,   96, "full down"},
        {172, 131,   19,    0, "half stick is a quarter of the speed"},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        tang_wii_sample_t s = {.lx = cases[i].sx, .ly = cases[i].sy};
        tang_mouse_t m = {0};
        tang_wii_apply(n, &s, &m, false, 0);
        CHECK(m.dx == cases[i].dx && m.dy == cases[i].dy,
              "%s: got %d/%d, want %d/%d", cases[i].what,
              m.dx, m.dy, cases[i].dx, cases[i].dy);
    }
}

static void test_mapping_classic(void)
{
    tang_wiipad_t *n = fresh();
    n->kind = TANG_WII_KIND_CLASSIC;
    n->centered = true;
    n->rcentered = true;
    n->cx = 32;
    n->cy = 32;
    n->crx = 16;
    n->cry = 16;

    const struct {
        int lx, ly, rx, ry;
        int dx, dy;
        const char *what;
    } cases[] = {
        {32, 32, 16, 16,     0,    0, "at rest"},
        {54, 32, 16, 16,    96,    0, "left stick full right"},
        {5,  32, 16, 16,   -96,    0, "left stick full left"},
        {32, 58, 16, 16,     0,  -96, "left stick full up"},
        {32,  6, 16, 16,     0,   96, "left stick full down"},
        {32, 32, 28, 16,   -48,    0, "right stick full left (its X grows left)"},
        {32, 32,  4, 16,    48,    0, "right stick full right"},
        {32, 32, 16, 28,     0,  -48, "right stick full up"},
        {32, 32, 16,  3,     0,   48, "right stick full down"},
        {32, 32, 17, 16,     0,    0, "right stick inside its dead zone"},
        {54, 32,  4, 16,   144,    0, "both sticks add"},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        tang_wii_sample_t s = {.lx = cases[i].lx, .ly = cases[i].ly,
                               .rx = cases[i].rx, .ry = cases[i].ry};
        tang_mouse_t m = {0};
        tang_wii_apply(n, &s, &m, false, 0);
        CHECK(m.dx == cases[i].dx && m.dy == cases[i].dy,
              "%s: got %d/%d, want %d/%d", cases[i].what,
              m.dx, m.dy, cases[i].dx, cases[i].dy);
    }
}

static void test_buttons(void)
{
    tang_wiipad_t *n = fresh();
    n->kind = TANG_WII_KIND_CLASSIC;
    n->centered = true;
    n->rcentered = true;
    n->cx = 32;
    n->cy = 32;
    n->crx = 16;
    n->cry = 16;

    tang_wii_sample_t l = {.lx = 32, .ly = 32, .rx = 16, .ry = 16, .left = true};
    tang_mouse_t m = {0};
    tang_wii_apply(n, &l, &m, false, 0);
    CHECK(m.buttons == TANG_MOUSE_LEFT, "L holds the left button (buttons %u)", m.buttons);
    CHECK(m.pressed == TANG_MOUSE_LEFT, "L's press is latched");

    tang_mouse_t m2 = {0};
    tang_wii_apply(n, &l, &m2, false, 0);
    CHECK(m2.buttons == TANG_MOUSE_LEFT && m2.pressed == 0,
          "a held L is one press, not one per poll");

    tang_wii_sample_t r = {.lx = 32, .ly = 32, .rx = 16, .ry = 16, .right = true};
    tang_mouse_t m3 = {0};
    tang_wii_apply(n, &r, &m3, false, 0);
    CHECK(m3.buttons == TANG_MOUSE_RIGHT && m3.pressed == TANG_MOUSE_RIGHT,
          "R is the right button");

    /* The Bluetooth mouse's buttons are already in the struct; the pad adds to
     * them rather than replacing them. */
    tang_mouse_t m4 = {.buttons = TANG_MOUSE_LEFT};
    tang_wii_apply(n, &r, &m4, false, 0);
    CHECK(m4.buttons == (TANG_MOUSE_LEFT | TANG_MOUSE_RIGHT),
          "the pad does not clear another button");
}

static void test_console_wheel(void)
{
    tang_wiipad_t *n = fresh();
    n->kind = TANG_WII_KIND_CLASSIC;
    n->centered = true;
    n->rcentered = true;
    n->cx = 32;
    n->cy = 32;
    n->crx = 16;
    n->cry = 16;

    tang_wii_sample_t up = {.lx = 32, .ly = 58, .rx = 16, .ry = 16};
    tang_mouse_t m = {0};
    tang_wii_apply(n, &up, &m, true, 1000);
    CHECK(m.wheel == 1 && m.dx == 0 && m.dy == 0,
          "console mode: left stick up is one detent and no pointer movement");
    m = (tang_mouse_t){0};
    tang_wii_apply(n, &up, &m, true, 1100);
    CHECK(m.wheel == 0, "a held stick does not repeat before the gate");
    m = (tang_mouse_t){0};
    tang_wii_apply(n, &up, &m, true, 1140);
    CHECK(m.wheel == 1, "the gate opens after TANG_WIIPAD_WHEEL_MS");

    tang_wii_sample_t down = {.lx = 32, .ly = 6, .rx = 16, .ry = 16};
    m = (tang_mouse_t){0};
    tang_wii_apply(n, &down, &m, true, 2000);
    CHECK(m.wheel == -1, "stick down is one detent the other way");

    /* The right stick has no business in the shell's history. */
    tang_wii_sample_t right = {.lx = 32, .ly = 32, .rx = 28, .ry = 28};
    m = (tang_mouse_t){0};
    tang_wii_apply(n, &right, &m, true, 3000);
    CHECK(m.wheel == 0, "the right stick does not scroll the console");
}

static void test_center_learning(void)
{
    tang_wiipad_t *n = fresh();
    tang_mouse_t m;

    n->centered = false;
    nunchuck_report(123, 131, 0x03);
    CHECK(read_once(&m), "a sample is read");
    CHECK(n->cx == 123 && n->cy == 131, "the resting sample becomes the center (%d/%d)",
          n->cx, n->cy);

    /* A push held while asking to re-learn must not become the new zero. */
    n->relearn = true;
    n->centered = false;
    nunchuck_report(221, 131, 0x03);
    (void)poll_once(&m, false);
    CHECK(n->cx == 123 && n->cy == 131, "an implausible center is refused");

    n->relearn = true;
    n->centered = false;
    nunchuck_report(120, 140, 0x03);
    (void)poll_once(&m, false);
    CHECK(n->cx == 120 && n->cy == 140, "an explicit re-learn takes the next resting sample");

    /* The Classic learns both sticks; a held right stick is refused too. */
    n = fresh();
    use_classic();
    report(0xa0, 0x20, 0x10, 0x61, 0xff, 0xff);
    CHECK(read_once(&m), "a classic sample is read");
    CHECK(n->kind == TANG_WII_KIND_CLASSIC, "the Classic is identified");
    CHECK(n->cx == 32 && n->cy == 32 && n->crx == 16 && n->cry == 16,
          "both centers learned (%d/%d %d/%d)", n->cx, n->cy, n->crx, n->cry);

    n->relearn = true;
    report(0xa0, 0x20, 0x10, 0xff, 0xff, 0xff);   /* right stick slammed down */
    (void)poll_once(&m, false);
    CHECK(n->cry == 16, "a held right stick is not learned as the center (%d)", n->cry);
}

static void test_state_machine(void)
{
    tang_wiipad_t *n = fresh();
    tang_mouse_t m;

    s_ready = false;
    CHECK(!poll_once(&m, false), "no bus: no read");
    CHECK(n->state == TANG_WIIPAD_OFF, "no bus is the off state");

    s_ready = true;
    CHECK(!poll_once(&m, false), "the first poll only wakes the part");
    CHECK(s_wake_writes == 1, "the wake-up writes go out (0x55 to 0xF0)");
    CHECK(n->state == TANG_WIIPAD_SLEEPING, "awake but unread is sleeping");

    s_ms = 40;
    nunchuck_report(123, 131, 0x03);
    CHECK(poll_once(&m, false), "the next poll reads a report");
    CHECK(n->state == TANG_WIIPAD_READY, "a report makes it ready");
    CHECK(n->kind == TANG_WII_KIND_NUNCHUCK, "and identifies the nunchuck");

    /* Replugged: the part answers 0xff again, and is woken again. */
    s_awake = false;
    s_ms = 80;
    CHECK(!poll_once(&m, false), "0xff is not a report");
    CHECK(n->state == TANG_WIIPAD_SLEEPING, "0xff sends it back to sleeping");
    s_ms = 1000;
    CHECK(!poll_once(&m, false), "the retry waits");
    s_ms = 2200;
    CHECK(!poll_once(&m, false), "after the retry interval it wakes again");
    CHECK(s_wake_writes == 2, "the wake-up was repeated");
}

static void test_swap_and_unsupported(void)
{
    tang_wiipad_t *n = fresh();
    tang_mouse_t m;

    /* A part this driver does not handle: refused, and nothing moves. */
    const uint8_t guitar[6] = {0x00, 0x00, 0xA4, 0x20, 0x01, 0x03};
    memcpy(s_id, guitar, sizeof s_id);
    (void)poll_once(&m, false);              /* wake */
    s_ms = 40;
    nunchuck_report(221, 229, 0x00);
    CHECK(!poll_once(&m, false), "a guitar reports nothing");
    CHECK(n->state == TANG_WIIPAD_UNSUPPORTED, "a part this driver cannot use is refused");
    CHECK(m.buttons == 0 && m.dx == 0 && m.dy == 0, "and moves no pointer");

    /* A known part swapped in is picked up without a reboot. */
    use_classic();
    s_ms += 2100;
    CHECK(!poll_once(&m, false), "the unsupported retry adopts the new part");
    CHECK(n->kind == TANG_WII_KIND_CLASSIC, "the Classic is adopted");
    CHECK(n->state == TANG_WIIPAD_READY, "and is ready");

    /* And a swap from a reporting nunchuck to a Classic is noticed by the
     * periodic identity check. */
    n = fresh();
    nunchuck_report(123, 131, 0x03);
    CHECK(read_once(&m), "the nunchuck reports");
    CHECK(n->kind == TANG_WII_KIND_NUNCHUCK, "as a nunchuck");

    use_classic();
    report(0xa0, 0x20, 0x10, 0x61, 0xff, 0xff);   /* the Classic at rest */
    s_ms += TANG_WIIPAD_ID_MS + 1;
    CHECK(poll_once(&m, false), "the swap is noticed on the identity check");
    CHECK(n->kind == TANG_WII_KIND_CLASSIC, "the Classic is adopted");
    CHECK(n->cx == 32 && n->cy == 32 && n->crx == 16 && n->cry == 16,
          "and its centers are learned from the new part (%d/%d %d/%d)",
          n->cx, n->cy, n->crx, n->cry);
    CHECK(m.dx == 0 && m.dy == 0, "a rest sample moves nothing after the swap");
}

int main(void)
{
    test_decode_nunchuck();
    test_decode_classic();
    test_identity();
    test_mapping_nunchuck();
    test_mapping_classic();
    test_buttons();
    test_console_wheel();
    test_center_learning();
    test_state_machine();
    test_swap_and_unsupported();
    printf("tang_wiipad: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}
