/* TinyTang — the Wii extension controller driver (ports/bl616/tang_wiipad.c) on
 * the host.
 *
 * The four hardware hooks are a model of a genuine part behind the desktop
 * core's I2C engine: it answers 0xff until the documented wake-up writes,
 * reports the identity block at 0xFA and the report at 0x00, and records the
 * data-format write (0x03 to 0xFE) that a Classic needs.  The bytes are the
 * ones measured on this board: the nunchuck at 123/131 with Z and C
 * (core-log entry 63), and the Classic's format-3 report through an 8BitDo
 * receiver (entry 65), whose sticks never move because the receiver mirrors
 * them onto the d-pad (WII-004).
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
static uint8_t  s_data[8];
static uint8_t  s_id[6] = {0x00, 0x00, 0xA4, 0x20, 0x00, 0x00};
static bool     s_ready = true;
static bool     s_awake;
static bool     s_read_fails;
static int      s_wake_writes;
static int      s_format_writes;
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
    if (reg == TANG_WIIPAD_REG_FORMAT) {
        s_format_writes++;
        s_id[4] = buf[0];              /* the format code is the identity's 5th byte */
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
    memset(s_data, 0xFF, sizeof s_data);
    s_data[0] = 128;
    s_data[1] = 128;
    s_data[5] = 0x03;                  /* both buttons released */
    s_ready = true;
    s_awake = false;
    s_read_fails = false;
    s_wake_writes = 0;
    s_format_writes = 0;
    s_ms = 0;
    return n;
}

static void use_classic(void)
{
    const uint8_t classic_id[6] = {0x00, 0x00, 0xA4, 0x20, 0x01, 0x01};
    memcpy(s_id, classic_id, sizeof s_id);
}

static void report(const uint8_t *bytes, unsigned n)
{
    memset(s_data, 0xFF, sizeof s_data);
    memcpy(s_data, bytes, n);
}

/* The measured format-3 rest report from the 8BitDo receiver. */
static void classic_rest(void)
{
    const uint8_t rest[8] = {0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0xff, 0xff};
    report(rest, sizeof rest);
}

/* Nunchuck report: stick plus the two buttons in byte 5's low two bits. */
static void nunchuck_report(uint8_t sx, uint8_t sy, unsigned buttons)
{
    memset(s_data, 0xFF, sizeof s_data);
    s_data[0] = sx;
    s_data[1] = sy;
    s_data[5] = (uint8_t)(0xFCu | (buttons & 0x03u));
}

static bool poll_once(tang_mouse_t *m, bool console_mode)
{
    memset(m, 0, sizeof *m);
    return tang_wiipad_poll(m, console_mode, NULL);
}

/* Wake a part, let the driver identify it (and select format 3 for a
 * Classic), and read one report.  Identification is a poll of its own: the
 * report read beside it may be in the other format, so it is dropped. */
static bool read_once(tang_mouse_t *m)
{
    (void)poll_once(m, false);         /* wake */
    s_ms += 40;
    (void)poll_once(m, false);         /* identify */
    s_ms += 40;
    return poll_once(m, false);
}

#define read_classic read_once

/* A Classic sample with the given buttons held and the sticks at rest. */
static tang_wii_sample_t classic_sample(uint32_t btn)
{
    tang_wii_sample_t s = {.lx = 132, .ly = 134, .rx = 134, .ry = 134, .btn = btn};
    return s;
}

/* ---- tests ---- */
static void test_decode_nunchuck(void)
{
    tang_wii_sample_t s;
    const uint8_t released[8] = {123, 131, 0, 0, 0, 0x53, 0xff, 0xff};
    const uint8_t z[8]        = {123, 131, 0, 0, 0, 0x52, 0xff, 0xff};
    const uint8_t c[8]        = {123, 131, 0, 0, 0, 0x51, 0xff, 0xff};
    const uint8_t both[8]     = {123, 131, 0, 0, 0, 0x50, 0xff, 0xff};

    tang_wii_decode(TANG_WII_KIND_NUNCHUCK, released, &s);
    CHECK(s.lx == 123 && s.ly == 131, "axes %d/%d", s.lx, s.ly);
    CHECK(!s.left && !s.right, "both buttons released");
    CHECK(s.rx == 0 && s.ry == 0, "a nunchuck has no right stick");

    tang_wii_decode(TANG_WII_KIND_NUNCHUCK, z, &s);
    CHECK(s.left && !s.right, "Z is bit 0, active low");
    tang_wii_decode(TANG_WII_KIND_NUNCHUCK, c, &s);
    CHECK(!s.left && s.right, "C is bit 1, active low");
    tang_wii_decode(TANG_WII_KIND_NUNCHUCK, both, &s);
    CHECK(s.left && s.right, "both held");
}

static void test_decode_classic(void)
{
    tang_wii_sample_t s;

    const uint8_t rest[8] = {0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0xff, 0xff};
    tang_wii_decode(TANG_WII_KIND_CLASSIC, rest, &s);
    CHECK(s.lx == 132 && s.rx == 134, "classic sticks at rest %d/%d", s.lx, s.rx);
    CHECK(s.ly == 134 && s.ry == 134, "classic sticks at rest %d/%d", s.ly, s.ry);
    CHECK(!s.left && !s.right, "classic A and B released");

    const uint8_t a[8] = {0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0xff, 0xef};
    const uint8_t b[8] = {0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0xff, 0xbf};
    tang_wii_decode(TANG_WII_KIND_CLASSIC, a, &s);
    CHECK(s.left && !s.right, "A is byte 7 bit 4");
    tang_wii_decode(TANG_WII_KIND_CLASSIC, b, &s);
    CHECK(!s.left && s.right, "B is byte 7 bit 6");

    /* The d-pad and the rest of the buttons, from the trace's own bytes. */
    const uint8_t up[8]     = {0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0xff, 0xfe};
    const uint8_t left[8]   = {0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0xff, 0xfd};
    const uint8_t down[8]   = {0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0xbf, 0xff};
    const uint8_t right[8]  = {0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0x7f, 0xff};
    const uint8_t plus[8]   = {0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0xfb, 0xff};
    const uint8_t minus[8]  = {0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0xef, 0xff};
    const uint8_t shoulder[8]={0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0xdf, 0xff};
    const uint8_t home[8]   = {0x84, 0x86, 0x86, 0x86, 0x00, 0x00, 0xf7, 0xff};

    tang_wii_decode(TANG_WII_KIND_CLASSIC, up, &s);
    CHECK(s.btn == TANG_WII_BTN_UP, "d-pad up");
    tang_wii_decode(TANG_WII_KIND_CLASSIC, left, &s);
    CHECK(s.btn == TANG_WII_BTN_LEFT, "d-pad left");
    tang_wii_decode(TANG_WII_KIND_CLASSIC, down, &s);
    CHECK(s.btn == TANG_WII_BTN_DOWN, "d-pad down");
    tang_wii_decode(TANG_WII_KIND_CLASSIC, right, &s);
    CHECK(s.btn == TANG_WII_BTN_RIGHT, "d-pad right");
    tang_wii_decode(TANG_WII_KIND_CLASSIC, plus, &s);
    CHECK(s.btn == TANG_WII_BTN_PLUS, "plus");
    tang_wii_decode(TANG_WII_KIND_CLASSIC, minus, &s);
    CHECK(s.btn == TANG_WII_BTN_MINUS, "minus");
    tang_wii_decode(TANG_WII_KIND_CLASSIC, shoulder, &s);
    CHECK(s.btn == TANG_WII_BTN_L, "the L shoulder");
    tang_wii_decode(TANG_WII_KIND_CLASSIC, home, &s);
    CHECK(s.btn == TANG_WII_BTN_HOME, "Home");
}

static void test_identity(void)
{
    const uint8_t nunchuck[6]   = {0x00, 0x00, 0xA4, 0x20, 0x00, 0x00};
    const uint8_t classic[6]    = {0x00, 0x00, 0xA4, 0x20, 0x01, 0x01};
    const uint8_t classic3[6]   = {0x00, 0x00, 0xA4, 0x20, 0x03, 0x01};
    const uint8_t classicpro[6] = {0x01, 0x00, 0xA4, 0x20, 0x01, 0x01};
    const uint8_t guitar[6]     = {0x00, 0x00, 0xA4, 0x20, 0x01, 0x03};
    const uint8_t ff[6]         = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    CHECK(tang_wii_identify(nunchuck) == TANG_WII_KIND_NUNCHUCK, "a nunchuck identifies as one");
    CHECK(tang_wii_identify(classic) == TANG_WII_KIND_CLASSIC, "a Classic identifies as one");
    CHECK(tang_wii_identify(classic3) == TANG_WII_KIND_CLASSIC,
          "a Classic in format 3 still identifies as one");
    CHECK(tang_wii_identify(classicpro) == TANG_WII_KIND_CLASSIC, "so does a Classic Pro");
    CHECK(tang_wii_identify(guitar) == TANG_WII_KIND_NONE, "a guitar is neither");
    CHECK(tang_wiipad_all_ff(ff, sizeof ff), "all 0xff is the un-woken report");
    CHECK(!tang_wiipad_all_ff(nunchuck, sizeof nunchuck), "a live report is not all 0xff");
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
        tang_wii_apply(n, &s, &m, false, 0, NULL);
        CHECK(m.dx == cases[i].dx && m.dy == cases[i].dy,
              "%s: got %d/%d, want %d/%d", cases[i].what,
              m.dx, m.dy, cases[i].dx, cases[i].dy);
    }
}

static void test_classic_sticks_ignored(void)
{
    tang_wiipad_t *n = fresh();
    n->kind = TANG_WII_KIND_CLASSIC;

    /* The receiver maps this part's sticks onto the d-pad and never reports
     * their axes (WII-004), so nothing in the stick bytes may move anything. */
    const struct {
        int lx, ly, rx, ry;
        const char *what;
    } cases[] = {
        {0,   0,   0,   0,   "all sticks hard over"},
        {255, 255, 255, 255, "and hard over the other way"},
        {132, 134, 134, 134, "at rest"},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        tang_wii_sample_t s = {.lx = cases[i].lx, .ly = cases[i].ly,
                               .rx = cases[i].rx, .ry = cases[i].ry};
        tang_mouse_t m = {0};
        tang_wii_keys_t k = {0};
        n->btn_prev = 0;
        tang_wii_apply(n, &s, &m, false, 0, &k);
        CHECK(m.dx == 0 && m.dy == 0 && m.wheel == 0,
              "%s: a Classic's sticks move nothing (%d/%d)", cases[i].what,
              m.dx, m.dy);
        CHECK(k.n == 0, "%s: and send no key", cases[i].what);
        m = (tang_mouse_t){0};
        k = (tang_wii_keys_t){0};
        tang_wii_apply(n, &s, &m, true, 5000, &k);
        CHECK(m.wheel == 0, "%s: and scroll nothing at the prompt", cases[i].what);
    }
    CHECK(!n->centered && !n->rcentered, "and no center is learned for them");
}

static void test_buttons(void)
{
    tang_wiipad_t *n = fresh();
    n->kind = TANG_WII_KIND_CLASSIC;

    tang_wii_sample_t l = classic_sample(TANG_WII_BTN_A);
    l.left = true;
    tang_mouse_t m = {0};
    tang_wii_apply(n, &l, &m, false, 0, NULL);
    CHECK(m.buttons == TANG_MOUSE_LEFT, "A holds the left button (buttons %u)", m.buttons);
    CHECK(m.pressed == TANG_MOUSE_LEFT, "A's press is latched");

    tang_mouse_t m2 = {0};
    tang_wii_apply(n, &l, &m2, false, 0, NULL);
    CHECK(m2.buttons == TANG_MOUSE_LEFT && m2.pressed == 0,
          "a held A is one press, not one per poll");

    tang_wii_sample_t r = classic_sample(TANG_WII_BTN_B);
    r.right = true;
    tang_mouse_t m3 = {0};
    tang_wii_apply(n, &r, &m3, false, 0, NULL);
    CHECK(m3.buttons == TANG_MOUSE_RIGHT && m3.pressed == TANG_MOUSE_RIGHT,
          "B is the right button");

    tang_mouse_t m4 = {.buttons = TANG_MOUSE_LEFT};
    tang_wii_apply(n, &r, &m4, false, 0, NULL);
    CHECK(m4.buttons == (TANG_MOUSE_LEFT | TANG_MOUSE_RIGHT),
          "the pad does not clear another button");
}

static void test_console_wheel(void)
{
    tang_wiipad_t *n = fresh();
    n->kind = TANG_WII_KIND_NUNCHUCK;
    n->centered = true;
    n->cx = 123;
    n->cy = 131;

    tang_wii_sample_t up = {.lx = 123, .ly = 229};
    tang_mouse_t m = {0};
    tang_wii_apply(n, &up, &m, true, 1000, NULL);
    CHECK(m.wheel == 1 && m.dx == 0 && m.dy == 0,
          "console mode: the nunchuck's stick up is one detent and no pointer movement");
    m = (tang_mouse_t){0};
    tang_wii_apply(n, &up, &m, true, 1100, NULL);
    CHECK(m.wheel == 0, "a held stick does not repeat before the gate");
    m = (tang_mouse_t){0};
    tang_wii_apply(n, &up, &m, true, 1140, NULL);
    CHECK(m.wheel == 1, "the gate opens after TANG_WIIPAD_WHEEL_MS");

    tang_wii_sample_t down = {.lx = 123, .ly = 33};
    m = (tang_mouse_t){0};
    tang_wii_apply(n, &down, &m, true, 2000, NULL);
    CHECK(m.wheel == -1, "stick down is one detent the other way");
}

static void test_keys(void)
{
    tang_wiipad_t *n = fresh();
    n->kind = TANG_WII_KIND_CLASSIC;

    const struct {
        uint32_t btn;
        const char *want;
        const char *what;
    } cases[] = {
        {TANG_WII_BTN_X,      "\x1b",     "X is Escape"},
        {TANG_WII_BTN_Y,      "\t",       "Y is Tab"},
        {TANG_WII_BTN_PLUS,   "\r",       "plus is Enter"},
        {TANG_WII_BTN_MINUS,  "\x1b[21~", "minus is F10"},
        {TANG_WII_BTN_L,      "",         "L is unmapped"},
        {TANG_WII_BTN_R,      "",         "R alone is the cursor modifier, not a key"},
        {TANG_WII_BTN_ZL,     "",         "ZL is unmapped for now"},
        {TANG_WII_BTN_ZR,     "",         "ZR is unmapped for now"},
        {TANG_WII_BTN_A,      "",         "A is a click, not a key"},
        {TANG_WII_BTN_B,      "",         "B is a click, not a key"},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        tang_wii_sample_t s = classic_sample(cases[i].btn);
        tang_mouse_t m = {0};
        tang_wii_keys_t k = {0};
        n->btn_prev = 0;
        tang_wii_apply(n, &s, &m, false, 0, &k);
        CHECK(k.n == (int)strlen(cases[i].want) &&
              memcmp(k.keys, cases[i].want, (size_t)k.n) == 0,
              "%s: got %d bytes", cases[i].what, k.n);
    }

    /* Home is the screen switch, not a key. */
    {
        tang_wii_sample_t s = classic_sample(TANG_WII_BTN_HOME);
        tang_mouse_t m = {0};
        tang_wii_keys_t k = {0};
        n->btn_prev = 0;
        tang_wii_apply(n, &s, &m, false, 0, &k);
        CHECK(k.n == 0 && k.toggle, "Home asks for the screen switch");
    }
}

static void test_dpad(void)
{
    tang_wiipad_t *n = fresh();
    n->kind = TANG_WII_KIND_CLASSIC;

    /* The d-pad is the keyboard's arrows: arrow keys normally, repeating as
     * the keyboard does. */
    {
        tang_wii_sample_t s = classic_sample(TANG_WII_BTN_UP);
        tang_mouse_t m = {0};
        tang_wii_keys_t k = {0};
        n->btn_prev = 0;
        tang_wii_apply(n, &s, &m, false, 0, &k);
        CHECK(k.n == 3 && memcmp(k.keys, "\x1b[A", 3) == 0, "d-pad up is the Up arrow");
        CHECK(m.dx == 0 && m.dy == 0, "and moves no pointer without L");

        m = (tang_mouse_t){0};
        k = (tang_wii_keys_t){0};
        tang_wii_apply(n, &s, &m, false, 400, &k);
        CHECK(k.n == 0, "it does not repeat before the keyboard's delay");
        m = (tang_mouse_t){0};
        k = (tang_wii_keys_t){0};
        tang_wii_apply(n, &s, &m, false, 500, &k);
        CHECK(k.n == 3, "it repeats after the keyboard's delay");
        m = (tang_mouse_t){0};
        k = (tang_wii_keys_t){0};
        tang_wii_apply(n, &s, &m, false, 540, &k);
        CHECK(k.n == 0, "and then at the keyboard's rate");
        m = (tang_mouse_t){0};
        k = (tang_wii_keys_t){0};
        tang_wii_apply(n, &s, &m, false, 550, &k);
        CHECK(k.n == 3, "the next repeat comes at the rate");
    }

    /* With L held the d-pad is the pointer, a cell a poll, and sends no key. */
    {
        tang_wii_sample_t s = classic_sample(TANG_WII_BTN_UP | TANG_WII_BTN_R);
        tang_mouse_t m = {0};
        tang_wii_keys_t k = {0};
        n->btn_prev = 0;
        tang_wii_apply(n, &s, &m, false, 0, &k);
        CHECK(k.n == 0, "R plus the d-pad sends no arrow");
        CHECK(m.dy == -16, "R plus d-pad up moves the pointer a cell (%d)", m.dy);
        m = (tang_mouse_t){0};
        k = (tang_wii_keys_t){0};
        tang_wii_apply(n, &s, &m, false, 40, &k);
        CHECK(m.dy == -16, "and another cell on the next poll");
    }

    /* A second direction added while the first is held is a diagonal: the
     * held mask is used, not the newest bit, so no slamming two buttons in
     * one poll is needed. */
    {
        tang_wii_sample_t right = classic_sample(TANG_WII_BTN_RIGHT | TANG_WII_BTN_R);
        tang_mouse_t m = {0};
        tang_wii_keys_t k = {0};
        n->btn_prev = 0;
        tang_wii_apply(n, &right, &m, false, 0, &k);
        CHECK(m.dx == 16 && m.dy == 0, "right alone moves right (%d/%d)", m.dx, m.dy);

        tang_wii_sample_t up = classic_sample(TANG_WII_BTN_RIGHT | TANG_WII_BTN_UP |
                                              TANG_WII_BTN_R);
        m = (tang_mouse_t){0};
        k = (tang_wii_keys_t){0};
        tang_wii_apply(n, &up, &m, false, 40, &k);
        CHECK(m.dx == 16 && m.dy == -16, "adding up while right is held is a diagonal (%d/%d)",
              m.dx, m.dy);

        m = (tang_mouse_t){0};
        k = (tang_wii_keys_t){0};
        tang_wii_apply(n, &up, &m, false, 80, &k);
        CHECK(m.dx == 16 && m.dy == -16, "and the diagonal holds while both are down");
    }

    /* At the prompt the d-pad is the arrows, and L takes them nowhere: there
     * is no pointer, which is what Alt+arrows do there too. */
    {
        tang_wii_sample_t s = classic_sample(TANG_WII_BTN_UP);
        tang_mouse_t m = {0};
        tang_wii_keys_t k = {0};
        n->btn_prev = 0;
        tang_wii_apply(n, &s, &m, true, 1000, &k);
        CHECK(k.n == 3 && memcmp(k.keys, "\x1b[A", 3) == 0,
              "at the prompt the d-pad is the Up arrow");

        tang_wii_sample_t held = classic_sample(TANG_WII_BTN_UP | TANG_WII_BTN_R);
        m = (tang_mouse_t){0};
        k = (tang_wii_keys_t){0};
        n->btn_prev = 0;
        tang_wii_apply(n, &held, &m, true, 2000, &k);
        CHECK(k.n == 0 && m.dx == 0 && m.dy == 0,
              "at the prompt R takes the d-pad nowhere");
    }
}

static void test_center_learning(void)
{
    tang_wiipad_t *n = fresh();
    tang_mouse_t m;

    n->centered = false;
    nunchuck_report(123, 131, 0x03);
    CHECK(read_once(&m), "a nunchuck sample is read");
    CHECK(n->cx == 123 && n->cy == 131, "the resting sample becomes the center (%d/%d)",
          n->cx, n->cy);

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
    CHECK(!poll_once(&m, false), "the identify poll adopts the part and drops its report");
    CHECK(n->state == TANG_WIIPAD_READY, "adoption makes it ready");
    CHECK(n->kind == TANG_WII_KIND_NUNCHUCK, "and identifies the nunchuck");
    CHECK(s_format_writes == 0, "a nunchuck is left in its own format");
    s_ms = 80;
    CHECK(poll_once(&m, false), "the next poll reads a report");

    /* Replugged: the part answers 0xff again, and is woken again. */
    s_awake = false;
    s_ms = 120;
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
    nunchuck_report(221, 229, 0x00);
    (void)poll_once(&m, false);              /* wake */
    s_ms = 40;
    CHECK(!poll_once(&m, false), "a guitar reports nothing");
    CHECK(n->state == TANG_WIIPAD_UNSUPPORTED, "a part this driver cannot use is refused");
    CHECK(m.buttons == 0 && m.dx == 0 && m.dy == 0, "and moves no pointer");

    /* A known part swapped in is picked up without a reboot. */
    use_classic();
    classic_rest();
    s_ms += 2100;
    CHECK(!poll_once(&m, false), "the unsupported retry adopts the new part");
    CHECK(n->kind == TANG_WII_KIND_CLASSIC, "the Classic is adopted");
    CHECK(s_format_writes == 1, "and format 3 is selected");
    s_ms += 40;
    CHECK(poll_once(&m, false), "the next poll reads the format-3 report");

    /* And a swap from a reporting nunchuck to a Classic is noticed by the
     * periodic identity check. */
    n = fresh();
    nunchuck_report(123, 131, 0x03);
    CHECK(read_once(&m), "the nunchuck reports");
    CHECK(n->kind == TANG_WII_KIND_NUNCHUCK, "as a nunchuck");

    use_classic();
    classic_rest();
    s_ms += TANG_WIIPAD_ID_MS + 1;
    CHECK(!poll_once(&m, false), "the swap is noticed, and format 3 is selected");
    CHECK(n->kind == TANG_WII_KIND_CLASSIC, "the Classic is adopted");
    s_ms += 40;
    CHECK(poll_once(&m, false), "the format-3 report is read");
    CHECK(!n->centered, "and no stick center is learned for it");
    CHECK(m.dx == 0 && m.dy == 0, "a rest sample moves nothing after the swap");
}

int main(void)
{
    test_decode_nunchuck();
    test_decode_classic();
    test_identity();
    test_mapping_nunchuck();
    test_classic_sticks_ignored();
    test_buttons();
    test_console_wheel();
    test_keys();
    test_dpad();
    test_center_learning();
    test_state_machine();
    test_swap_and_unsupported();
    printf("tang_wiipad: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}
