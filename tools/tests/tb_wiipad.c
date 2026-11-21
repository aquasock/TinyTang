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

/* The loaded core's reports (FPGA_RESP_WII), as the link hands them over. */
static bool     s_core_have;
static uint8_t  s_core_r[6];
static uint32_t s_core_seq;

bool tang_wiipad_hw_core_report(uint8_t r[6], uint32_t *seq)
{
    if (!s_core_have) {
        return false;
    }
    memcpy(r, s_core_r, 6);
    *seq = s_core_seq;
    return true;
}

/* A new report from the core: the four stick bytes, then report bytes 6 and
 * 7, the buttons, active low. */
static void core_report(uint8_t lx, uint8_t rx, uint8_t ly, uint8_t ry,
                        uint8_t b6, uint8_t b7)
{
    const uint8_t r[6] = { lx, rx, ly, ry, b6, b7 };
    memcpy(s_core_r, r, 6);
    s_core_have = true;
    s_core_seq++;
}

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
    s_core_have = false;
    s_core_seq = 0;
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
    const uint8_t classic0[6]   = {0x00, 0x00, 0xA4, 0x20, 0x00, 0x01};
    const uint8_t classicpro[6] = {0x01, 0x00, 0xA4, 0x20, 0x01, 0x01};
    const uint8_t guitar[6]     = {0x00, 0x00, 0xA4, 0x20, 0x01, 0x03};
    const uint8_t ff[6]         = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    CHECK(tang_wii_identify(nunchuck) == TANG_WII_KIND_NUNCHUCK, "a nunchuck identifies as one");
    CHECK(tang_wii_identify(classic) == TANG_WII_KIND_CLASSIC, "a Classic identifies as one");
    CHECK(tang_wii_identify(classic3) == TANG_WII_KIND_CLASSIC,
          "a Classic in format 3 still identifies as one");
    CHECK(tang_wii_identify(classic0) == TANG_WII_KIND_CLASSIC,
          "a Classic reading format 0 (the receiver after TinySNES) is one too");
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
        /* The standard stick (tang_pad_stick.h, the 8BitDo's): 98 counts
         * from the center is full deflection, the dead zone is about 9 of
         * them, then linear to TANG_STICK_POINTER_MAX. */
        {123, 131,    0,    0, "at rest"},
        {131, 131,    0,    0, "inside the dead zone, +8"},
        {115, 131,    0,    0, "inside the dead zone, -8"},
        {133, 131,    0,    0, "dead zone edge, +10"},
        {140, 131,    4,    0, "just past the dead zone"},
        {221, 131,   48,    0, "full right"},
        {24,  131,  -48,    0, "full left"},
        {255, 131,   48,    0, "past full is clamped"},
        {123, 229,    0,  -48, "full up (Y grows upward)"},
        {123, 33,     0,   48, "full down"},
        {172, 131,   21,    0, "half stick is linear, not squared"},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        tang_wii_sample_t s = {.lx = cases[i].sx, .ly = cases[i].sy, .sticks = true};
        tang_mouse_t m = {0};
        tang_wii_apply(n, &s, &m, false, 0, NULL);
        CHECK(m.dx == cases[i].dx && m.dy == cases[i].dy,
              "%s: got %d/%d, want %d/%d", cases[i].what,
              m.dx, m.dy, cases[i].dx, cases[i].dy);
    }
}

static void test_classic_sticks(void)
{
    tang_wiipad_t *n = fresh();
    n->kind = TANG_WII_KIND_CLASSIC;
    tang_mouse_t m;

    /* The genuine Classic's rest in format 3, measured on the board (WII-003):
     * the first sample becomes both centers and moves nothing. */
    tang_wii_sample_t rest = {.lx = 128, .ly = 128, .rx = 130, .ry = 133, .sticks = true};
    m = (tang_mouse_t){0};
    tang_wii_apply(n, &rest, &m, false, 0, NULL);
    CHECK(n->centered && n->rcentered && n->cx == 128 && n->cy == 128 &&
          n->crx == 130 && n->cry == 133, "the rest becomes both centers (%d/%d %d/%d)",
          n->cx, n->cy, n->crx, n->cry);
    CHECK(m.dx == 0 && m.dy == 0 && m.wheel == 0, "and moves nothing");

    const struct {
        int lx, ly, rx, ry;
        int dx, dy, wheel;
        const char *what;
    } cases[] = {
        {226, 128, 130, 133,  48,   0,  0, "left stick full right is the pointer"},
        {30,  128, 130, 133, -48,   0,  0, "left stick full left"},
        {128, 226, 130, 133,   0, -48,  0, "left stick up moves the pointer up"},
        {128, 30,  130, 133,   0,  48,  0, "left stick down"},
        {177, 128, 130, 133,  21,   0,  0, "half stick is the 8BitDo's half speed"},
        {128, 128, 130, 231,   0,   0,  1, "right stick up is wheel-up"},
        {128, 128, 130, 35,    0,   0, -1, "right stick down is wheel-down"},
        {128, 128, 232, 133,   0,   0,  0, "the right stick's X does nothing"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        tang_wii_sample_t s = {.lx = cases[i].lx, .ly = cases[i].ly,
                               .rx = cases[i].rx, .ry = cases[i].ry, .sticks = true};
        m = (tang_mouse_t){0};
        n->next_wheel_ms = 0;
        tang_wii_apply(n, &s, &m, false, 1000 + (uint32_t)i * 1000u, NULL);
        CHECK(m.dx == cases[i].dx && m.dy == cases[i].dy && m.wheel == cases[i].wheel,
              "%s: got %d/%d wheel %d", cases[i].what, m.dx, m.dy, m.wheel);
    }

    /* At the prompt the left stick walks the history and the right does
     * nothing, as on the 8BitDo. */
    {
        tang_wii_sample_t up = {.lx = 128, .ly = 226, .rx = 130, .ry = 133, .sticks = true};
        m = (tang_mouse_t){0};
        n->next_wheel_ms = 0;
        tang_wii_apply(n, &up, &m, true, 20000, NULL);
        CHECK(m.wheel == 1 && m.dx == 0 && m.dy == 0, "at the prompt left stick up is wheel-up");
        tang_wii_sample_t rup = {.lx = 128, .ly = 128, .rx = 130, .ry = 231, .sticks = true};
        m = (tang_mouse_t){0};
        tang_wii_apply(n, &rup, &m, true, 30000, NULL);
        CHECK(m.wheel == 0 && m.dx == 0 && m.dy == 0, "and the right stick does nothing there");
    }

    /* The 8BitDo receiver folds an SN30's sticks into the d-pad and leaves
     * the axis bytes at rest (WII-004): it has no pointer, and none is faked
     * from the d-pad, which stays the arrows. */
    {
        n = fresh();
        n->kind = TANG_WII_KIND_CLASSIC;
        tang_wii_sample_t s = classic_sample(TANG_WII_BTN_RIGHT);
        s.sticks = true;
        tang_wii_keys_t k = {0};
        m = (tang_mouse_t){0};
        tang_wii_apply(n, &s, &m, false, 0, &k);
        CHECK(m.dx == 0 && m.dy == 0 && m.wheel == 0,
              "the receiver's d-pad moves no pointer (%d/%d)", m.dx, m.dy);
        CHECK(k.n == 3 && memcmp(k.keys, "\x1b[C", 3) == 0, "it is the Right arrow");
    }
}

static void test_nunchuck_home(void)
{
    tang_wiipad_t *n = fresh();
    n->kind = TANG_WII_KIND_NUNCHUCK;
    tang_mouse_t m;
    tang_wii_keys_t k;

    /* Z, then C added: Z clicks, the chord is Home, and the buttons let go. */
    tang_wii_sample_t z = {.lx = 123, .ly = 131, .left = true, .sticks = true};
    tang_wii_sample_t zc = {.lx = 123, .ly = 131, .left = true, .right = true, .sticks = true};
    tang_wii_sample_t c = {.lx = 123, .ly = 131, .right = true, .sticks = true};
    tang_wii_sample_t none = {.lx = 123, .ly = 131, .sticks = true};

    m = (tang_mouse_t){0}; k = (tang_wii_keys_t){0};
    tang_wii_apply(n, &z, &m, false, 0, &k);
    CHECK(m.pressed == TANG_MOUSE_LEFT && !k.toggle, "Z alone is the left click");

    m = (tang_mouse_t){0}; k = (tang_wii_keys_t){0};
    tang_wii_apply(n, &zc, &m, false, 40, &k);
    CHECK(k.toggle, "C and Z together are Home");
    CHECK(m.buttons == 0 && m.pressed == 0, "and neither is a button while held (%u/%u)",
          m.buttons, m.pressed);

    m = (tang_mouse_t){0}; k = (tang_wii_keys_t){0};
    tang_wii_apply(n, &zc, &m, false, 80, &k);
    CHECK(!k.toggle, "held, the chord is one Home, not one per poll");

    m = (tang_mouse_t){0}; k = (tang_wii_keys_t){0};
    tang_wii_apply(n, &c, &m, false, 120, &k);
    CHECK(m.buttons == 0 && m.pressed == 0 && !k.toggle,
          "letting Z go first leaves C held but not clicking");

    m = (tang_mouse_t){0}; k = (tang_wii_keys_t){0};
    tang_wii_apply(n, &zc, &m, false, 140, &k);
    CHECK(!k.toggle && m.buttons == 0,
          "pressing Z again before C is let go is not a second Home");

    m = (tang_mouse_t){0}; k = (tang_wii_keys_t){0};
    tang_wii_apply(n, &none, &m, false, 160, &k);
    m = (tang_mouse_t){0}; k = (tang_wii_keys_t){0};
    tang_wii_apply(n, &c, &m, false, 200, &k);
    CHECK(m.pressed == TANG_MOUSE_RIGHT && !k.toggle,
          "once both are up, C alone is the right click again");

    /* Both in one poll: Home and no click at all. */
    n = fresh();
    n->kind = TANG_WII_KIND_NUNCHUCK;
    m = (tang_mouse_t){0}; k = (tang_wii_keys_t){0};
    tang_wii_apply(n, &zc, &m, false, 0, &k);
    CHECK(k.toggle && m.buttons == 0 && m.pressed == 0,
          "C and Z pressed in one poll are Home and no click");

    /* A Classic has its own Home: A and B together are two clicks. */
    n = fresh();
    n->kind = TANG_WII_KIND_CLASSIC;
    tang_wii_sample_t ab = classic_sample(TANG_WII_BTN_A | TANG_WII_BTN_B);
    ab.left = true;
    ab.right = true;
    m = (tang_mouse_t){0}; k = (tang_wii_keys_t){0};
    tang_wii_apply(n, &ab, &m, false, 0, &k);
    CHECK(!k.toggle && m.buttons == (TANG_MOUSE_LEFT | TANG_MOUSE_RIGHT),
          "on a Classic A and B together are both buttons, not Home");
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

    tang_wii_sample_t up = {.lx = 123, .ly = 229, .sticks = true};
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

    tang_wii_sample_t down = {.lx = 123, .ly = 33, .sticks = true};
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
        {TANG_WII_BTN_R,      "",         "R is a game's button, not a key"},
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

    /* X is counted, once a press, so an app can tell it from the keyboard's
     * Escape; held, it is not counted again, and no other button counts. */
    {
        const uint32_t before = tang_wiipad_x_presses();
        tang_wii_sample_t s = classic_sample(TANG_WII_BTN_X);
        tang_mouse_t m = {0};
        tang_wii_keys_t k = {0};
        n->btn_prev = 0;
        tang_wii_apply(n, &s, &m, false, 0, &k);
        CHECK(tang_wiipad_x_presses() == before + 1, "X is counted");
        k = (tang_wii_keys_t){0};
        tang_wii_apply(n, &s, &m, false, 10, &k);
        CHECK(tang_wiipad_x_presses() == before + 1 && k.n == 0, "a held X is not counted again");
        tang_wii_sample_t y = classic_sample(TANG_WII_BTN_Y);
        n->btn_prev = 0;
        tang_wii_apply(n, &y, &m, false, 20, &k);
        CHECK(tang_wiipad_x_presses() == before + 1, "Y is not X");
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

    /* R is a game's button: with it held the d-pad is still the arrows and
     * moves no pointer, on the desktop and at the prompt (BLE-023). */
    {
        tang_wii_sample_t s = classic_sample(TANG_WII_BTN_UP | TANG_WII_BTN_R);
        tang_mouse_t m = {0};
        tang_wii_keys_t k = {0};
        n->btn_prev = 0;
        tang_wii_apply(n, &s, &m, false, 0, &k);
        CHECK(k.n == 3 && memcmp(k.keys, "\x1b[A", 3) == 0, "R plus d-pad up is the Up arrow");
        CHECK(m.dx == 0 && m.dy == 0, "and moves no pointer (%d/%d)", m.dx, m.dy);

        tang_wii_sample_t p = classic_sample(TANG_WII_BTN_UP);
        m = (tang_mouse_t){0};
        k = (tang_wii_keys_t){0};
        n->btn_prev = 0;
        tang_wii_apply(n, &p, &m, true, 1000, &k);
        CHECK(k.n == 3 && memcmp(k.keys, "\x1b[A", 3) == 0,
              "at the prompt the d-pad is the Up arrow");
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

    /* The 8BitDo receiver as a TinySNES game leaves it: identity byte 4 reads
     * format 0.  It is a Classic, so it is adopted and put back in format 3
     * rather than refused. */
    n = fresh();
    use_classic();
    s_id[4] = 0x00;
    classic_rest();
    (void)poll_once(&m, false);              /* wake */
    s_ms += 40;
    CHECK(!poll_once(&m, false), "the format-0 receiver is adopted");
    CHECK(n->state == TANG_WIIPAD_READY && n->kind == TANG_WII_KIND_CLASSIC,
          "as a Classic, not refused");
    CHECK(s_format_writes == 1 && s_id[4] == 0x03, "and format 3 is selected");
    s_ms += 40;
    CHECK(poll_once(&m, false), "then it reports");

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
    CHECK(n->centered && n->cx == 0x84 && n->cy == 0x86,
          "and its resting sticks become the centers (%d/%d)", n->cx, n->cy);
    CHECK(m.dx == 0 && m.dy == 0, "a rest sample moves nothing after the swap");
}

/* A game core holds the bus: the Classic is read through the core's own
 * reports, with the same decode and key map, Home left to the core's LB, and
 * only while new reports keep coming. */
static void test_core_report(void)
{
    tang_wiipad_t *n = fresh();
    s_ready = false;                     /* no I2C engine in this core */
    tang_mouse_t m;
    tang_wii_keys_t k;

    memset(&k, 0, sizeof k);
    m = (tang_mouse_t){0};
    CHECK(!tang_wiipad_poll(&m, false, &k), "a core with no report is nothing to read");
    CHECK(!tang_wiipad_core_live() && n->state == TANG_WIIPAD_OFF, "and the pad is off");

    /* The genuine Classic's rest (WII-003): it becomes both centers. */
    s_ms = 1000;
    core_report(128, 130, 128, 133, 0xff, 0xff);
    m = (tang_mouse_t){0};
    CHECK(tang_wiipad_poll(&m, false, &k), "a core's report is read");
    CHECK(tang_wiipad_core_live(), "the core's reports are live");
    CHECK(n->kind == TANG_WII_KIND_CLASSIC && n->state == TANG_WIIPAD_READY,
          "the core's pad is a ready Classic");
    CHECK(n->cx == 128 && n->cy == 128 && n->crx == 130 && n->cry == 133,
          "the first report is the centers (%d/%d %d/%d)", n->cx, n->cy, n->crx, n->cry);
    CHECK(m.dx == 0 && m.dy == 0 && m.wheel == 0, "and moves nothing");
    CHECK(s_wake_writes == 0, "nothing was written to a bus the core holds");

    /* The standard sticks, as on the bus. */
    s_ms += 40;
    core_report(226, 130, 128, 133, 0xff, 0xff);
    m = (tang_mouse_t){0};
    (void)tang_wiipad_poll(&m, false, &k);
    CHECK(m.dx == 48 && m.dy == 0, "left stick full right is the pointer (%d/%d)", m.dx, m.dy);
    s_ms += 40;
    core_report(128, 130, 226, 133, 0xff, 0xff);
    m = (tang_mouse_t){0};
    (void)tang_wiipad_poll(&m, false, &k);
    CHECK(m.dy == -48, "left stick up moves the pointer up (%d)", m.dy);

    /* The key map: X Escape, the d-pad the arrows, A the left button. */
    s_ms += 40;
    core_report(128, 130, 128, 133, 0xff, 0xf7);          /* X */
    memset(&k, 0, sizeof k);
    m = (tang_mouse_t){0};
    (void)tang_wiipad_poll(&m, false, &k);
    CHECK(k.n == 1 && k.keys[0] == 0x1b, "X is Escape through the core (%d bytes)", k.n);
    s_ms += 40;
    core_report(128, 130, 128, 133, 0xff, 0xfe);          /* d-pad up */
    memset(&k, 0, sizeof k);
    (void)tang_wiipad_poll(&m, false, &k);
    CHECK(k.n == 3 && memcmp(k.keys, "\x1b[A", 3) == 0, "the d-pad is the arrows (%d bytes)", k.n);
    s_ms += 40;
    core_report(128, 130, 128, 133, 0xff, 0xef);          /* A */
    memset(&k, 0, sizeof k);
    m = (tang_mouse_t){0};
    (void)tang_wiipad_poll(&m, false, &k);
    CHECK((m.pressed & TANG_MOUSE_LEFT) && (m.buttons & TANG_MOUSE_LEFT), "A is the left button");

    /* Home is the core's: its tap is LB in the pad word, so here it is not
     * the screen switch, nor anything else. */
    s_ms += 40;
    core_report(128, 130, 128, 133, 0xf7, 0xff);          /* Home */
    memset(&k, 0, sizeof k);
    (void)tang_wiipad_poll(&m, false, &k);
    CHECK(!k.toggle && k.n == 0, "Home through the core is not the switch (%d bytes)", k.n);
    s_ms += 40;
    core_report(128, 130, 128, 133, 0xff, 0xff);
    (void)tang_wiipad_poll(&m, false, &k);

    /* The same report read again is not a new one: once the reports stop the
     * pad is let go. */
    s_ms += TANG_WIIPAD_CORE_MS - 1;
    CHECK(tang_wiipad_poll(&m, false, &k) && tang_wiipad_core_live(),
          "live until TANG_WIIPAD_CORE_MS after the last report");
    s_ms += 1;
    CHECK(!tang_wiipad_poll(&m, false, &k), "no new report for TANG_WIIPAD_CORE_MS");
    CHECK(!tang_wiipad_core_live() && n->state == TANG_WIIPAD_OFF, "and the pad is let go");

    /* Back after a gap with X already held: not a press, and the centers are
     * learned again from that report. */
    s_ms += 1000;
    core_report(130, 128, 126, 131, 0xff, 0xf7);
    memset(&k, 0, sizeof k);
    m = (tang_mouse_t){0};
    CHECK(tang_wiipad_poll(&m, false, &k), "the reports are read again");
    CHECK(k.n == 0, "a button held across the gap is not a press (%d bytes)", k.n);
    CHECK(n->cx == 130 && n->cy == 126, "the centers are learned again (%d/%d)", n->cx, n->cy);

    /* The desktop core again: its bus wins and the core's reports are not
     * used, and the part is woken on the bus. */
    s_ready = true;
    s_ms += 40;
    core_report(226, 130, 128, 133, 0xff, 0xff);
    m = (tang_mouse_t){0};
    (void)tang_wiipad_poll(&m, false, &k);
    CHECK(!tang_wiipad_core_live(), "a core with the bus is read on the bus");
    CHECK(m.dx == 0, "the core's report is not used beside the bus (%d)", m.dx);
    CHECK(s_wake_writes == 1, "the part is woken on the bus (%d)", s_wake_writes);
}

int main(void)
{
    test_decode_nunchuck();
    test_decode_classic();
    test_identity();
    test_mapping_nunchuck();
    test_classic_sticks();
    test_nunchuck_home();
    test_buttons();
    test_console_wheel();
    test_keys();
    test_dpad();
    test_center_learning();
    test_state_machine();
    test_swap_and_unsupported();
    test_core_report();
    printf("tang_wiipad: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}
