/*
 * tb_btpad.c -- tang_btpad_codec.c on the host: the Classic pad pairings file
 * and the Switch 0x3F report, decoded into the pad word the desk layer takes.
 * The reports are ones the board received from the 8BitDo pad (BLE-021).
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>

#include "tang_btpad_codec.h"
#include "tang_pad.h"

static int fails, checks;
static void check(int ok, const char *what)
{
    checks++;
    if (!ok) {
        fails++;
        printf("  FAIL: %s\n", what);
    }
}

static uint16_t word_of(const uint8_t *r, size_t n, int *ok)
{
    tang_btpad_input_t in;
    *ok = tang_btpad_switch_decode(r, n, &in);
    return *ok ? tang_btpad_word(&in) : 0;
}

int main(void)
{
    int ok;
    /* At rest: hat 8, sticks centred (0x7ff0 / 0x8000). */
    const uint8_t rest[13] = { 0xa1, 0x3f, 0x00, 0x00, 0x08, 0xf0, 0x7f, 0x00, 0x80, 0x00, 0x80, 0xf0, 0x7f };
    check(word_of(rest, 13, &ok) == 0 && ok, "a pad at rest is not an empty word");

    uint8_t r[13];
    memcpy(r, rest, 13); r[2] = 0x02;                       /* A */
    check(word_of(r, 13, &ok) == TANG_PAD_A, "A is not TANG_PAD_A");
    memcpy(r, rest, 13); r[2] = 0x01;                       /* B */
    check(word_of(r, 13, &ok) == TANG_PAD_B, "B is not TANG_PAD_B");
    memcpy(r, rest, 13); r[3] = 0x10;                       /* home */
    check(word_of(r, 13, &ok) == TANG_PAD_LB, "home is not LB");
    memcpy(r, rest, 13); r[2] = 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80;   /* Y X L R ZL ZR */
    r[3] = 0x01 | 0x02 | 0x04 | 0x08 | 0x20;                /* minus plus clicks capture */
    check(word_of(r, 13, &ok) == 0, "a button the desktop does not use set a bit");

    /* The hat, as the board saw it: 00 up, 02 right, 04 down, 06 left. */
    const struct { uint8_t hat; uint16_t want; } hats[] = {
        { 0, TANG_PAD_UP }, { 1, TANG_PAD_UP | TANG_PAD_RIGHT }, { 2, TANG_PAD_RIGHT },
        { 3, TANG_PAD_DOWN | TANG_PAD_RIGHT }, { 4, TANG_PAD_DOWN },
        { 5, TANG_PAD_DOWN | TANG_PAD_LEFT }, { 6, TANG_PAD_LEFT },
        { 7, TANG_PAD_UP | TANG_PAD_LEFT }, { 8, 0 }, { 15, 0 },
    };
    for (size_t i = 0; i < sizeof hats / sizeof hats[0]; i++) {
        memcpy(r, rest, 13); r[4] = hats[i].hat;
        char what[48];
        snprintf(what, sizeof what, "hat %u is the wrong direction", hats[i].hat);
        check(word_of(r, 13, &ok) == hats[i].want, what);
    }

    /* A report the board received with the left stick pushed: X 0xb6c0
     * (right of centre past the dead zone), Y 0xfff0 (fully down), and the
     * hat at 04 (down) with R held. */
    const uint8_t pushed[13] = { 0xa1, 0x3f, 0x20, 0x00, 0x04, 0xc0, 0xb6, 0xf0, 0xff, 0x00, 0x80, 0xf0, 0x7f };
    check(word_of(pushed, 13, &ok) == (TANG_PAD_RIGHT | TANG_PAD_DOWN), "a pushed stick is not right+down");
    /* Inside the dead zone: drift is nothing. */
    memcpy(r, rest, 13); r[5] = 0x00; r[6] = 0x9f;          /* X 0x9f00, +0x1f00 */
    r[7] = 0x00; r[8] = 0x60;                               /* Y 0x6000, -0x2000 */
    check(word_of(r, 13, &ok) == 0, "a stick inside the dead zone moved");
    memcpy(r, rest, 13); r[7] = 0x00; r[8] = 0x00;          /* Y 0x0000: up */
    check(word_of(r, 13, &ok) == TANG_PAD_UP, "a stick pushed up is not UP");
    memcpy(r, rest, 13); r[5] = 0x00; r[6] = 0x00;          /* X 0x0000: left */
    check(word_of(r, 13, &ok) == TANG_PAD_LEFT, "a stick pushed left is not LEFT");

    /* Not a 0x3F report, or too short: refused. */
    check(!tang_btpad_switch_decode(rest, 12, &(tang_btpad_input_t){0}), "a short report decoded");
    memcpy(r, rest, 13); r[1] = 0x30;
    check(!tang_btpad_switch_decode(r, 13, &(tang_btpad_input_t){0}), "report 0x30 decoded as 0x3F");
    memcpy(r, rest, 13); r[0] = 0xa2;
    check(!tang_btpad_switch_decode(r, 13, &(tang_btpad_input_t){0}), "an output report decoded");

    /* The Wii Classic sample: each button to its Classic bit, A and B the
     * pointer's buttons, home in nothing (the desk layer takes it as LB). */
    {
        static const struct { uint8_t b1, b2; uint32_t wii; } m[] = {
            { 0x02, 0, TANG_WII_BTN_A }, { 0x01, 0, TANG_WII_BTN_B },
            { 0x08, 0, TANG_WII_BTN_X }, { 0x04, 0, TANG_WII_BTN_Y },
            { 0x10, 0, TANG_WII_BTN_L }, { 0x20, 0, 0 },      /* R: never the D-pad pointer */
            { 0x40, 0, TANG_WII_BTN_ZL }, { 0x80, 0, TANG_WII_BTN_ZR },
            { 0, 0x02, TANG_WII_BTN_PLUS }, { 0, 0x01, TANG_WII_BTN_MINUS },
            { 0, 0x10, 0 }, { 0, 0x20, 0 }, { 0, 0x04, 0 }, { 0, 0x08, 0 },
        };
        for (size_t i = 0; i < sizeof m / sizeof m[0]; i++) {
            tang_btpad_input_t in;
            memcpy(r, rest, 13); r[2] = m[i].b1; r[3] = m[i].b2;
            tang_btpad_switch_decode(r, 13, &in);
            tang_wii_sample_t smp;
            tang_btpad_wii_sample(&in, &smp);
            char what[64];
            snprintf(what, sizeof what, "buttons %02x %02x are not Wii bits %04x",
                     m[i].b1, m[i].b2, (unsigned)m[i].wii);
            check(smp.btn == m[i].wii, what);
            check(smp.left == (m[i].b1 == 0x02) && smp.right == (m[i].b1 == 0x01),
                  "the pointer buttons are not A and B");
            check(smp.lx == 0 && smp.ly == 0 && smp.rx == 0 && smp.ry == 0,
                  "the sample carries sticks a Classic's never does");
        }
        tang_btpad_input_t in;
        memcpy(r, rest, 13); r[4] = 1;                       /* hat up-right */
        tang_btpad_switch_decode(r, 13, &in);
        tang_wii_sample_t smp;
        tang_btpad_wii_sample(&in, &smp);
        check(smp.btn == (TANG_WII_BTN_UP | TANG_WII_BTN_RIGHT), "a diagonal is not two D-pad bits");
        memcpy(r, rest, 13); r[4] = 6;                       /* hat left */
        tang_btpad_switch_decode(r, 13, &in);
        tang_btpad_wii_sample(&in, &smp);
        check(smp.btn == TANG_WII_BTN_LEFT, "hat left is not the D-pad's left");
    }

    /* The sticks: the pointer on the desktop, the wheel at the prompt. */
    {
        check(tang_btpad_axis_counts(0x8000, 96) == 0, "a centred axis moved");
        check(tang_btpad_axis_counts(0x8000 + TANG_BTPAD_POINTER_DEADZONE, 96) == 0,
              "the edge of the dead zone moved");
        check(tang_btpad_axis_counts(0xfff0, 96) == 96, "full right is not full counts");
        check(tang_btpad_axis_counts(0x0000, 96) == -96, "full left is not full counts");
        const int half = tang_btpad_axis_counts(0xC000, 96);
        check(half > 30 && half < 60, "half travel is not about half the counts");

        tang_btpad_input_t in;
        uint32_t next = 0;
        tang_mouse_t mo = { 0 };
        tang_btpad_switch_decode(pushed, 13, &in);           /* right, fully down */
        tang_btpad_sticks(&in, false, 1000, &next, &mo);
        check(mo.dx > 0 && mo.dy == TANG_BTPAD_POINTER_MAX_COUNTS && mo.wheel == 0,
              "the left stick did not move the pointer at its full speed");

        memcpy(r, rest, 13); r[7] = 0x00; r[8] = 0x00;       /* left stick up */
        tang_btpad_switch_decode(r, 13, &in);
        memset(&mo, 0, sizeof mo);
        next = 1000;
        tang_btpad_sticks(&in, true, 1000, &next, &mo);
        check(mo.wheel == 1 && mo.dx == 0 && mo.dy == 0, "up at the prompt is not one wheel step back");
        tang_btpad_sticks(&in, true, 1000 + TANG_WIIPAD_WHEEL_MS - 1, &next, &mo);
        check(mo.wheel == 1, "the wheel stepped again too soon");
        tang_btpad_sticks(&in, true, 1000 + TANG_WIIPAD_WHEEL_MS, &next, &mo);
        check(mo.wheel == 2, "a held stick did not step the wheel again");
        tang_btpad_switch_decode(rest, 13, &in);
        tang_btpad_sticks(&in, true, 1200, &next, &mo);
        memcpy(r, rest, 13); r[7] = 0xf0; r[8] = 0xff;       /* down, at once */
        tang_btpad_switch_decode(r, 13, &in);
        tang_btpad_sticks(&in, true, 1201, &next, &mo);
        check(mo.wheel == 1, "the wheel did not re-arm at the middle, or down is not -1");

        memcpy(r, rest, 13); r[11] = 0x00; r[12] = 0x00;     /* right stick up */
        tang_btpad_switch_decode(r, 13, &in);
        memset(&mo, 0, sizeof mo);
        next = 0;
        tang_btpad_sticks(&in, false, 2000, &next, &mo);
        check(mo.wheel == 1 && mo.dx == 0 && mo.dy == 0, "the right stick is not the desktop's wheel");
    }

    /* The pairings file: round trip, then damage. */
    tang_btpad_key_t k[TANG_BTPAD_SLOTS] = { 0 }, back[TANG_BTPAD_SLOTS];
    k[1].valid = true;
    k[1].kind = TANG_BTPAD_KIND_SWITCH;
    k[1].flags = 0x01;
    const uint8_t addr[6] = { 0x7f, 0x4d, 0x17, 0xd8, 0x17, 0xe4 };
    memcpy(k[1].addr, addr, 6);
    for (int i = 0; i < 16; i++) {
        k[1].key[i] = (uint8_t)(0xa0 + i);
    }
    strcpy(k[1].name, "Pro Controller");
    uint8_t file[TANG_BTPAD_FILE_LEN];
    check(tang_btpad_keys_encode(k, file) == TANG_BTPAD_FILE_LEN, "encode length");
    check(tang_btpad_keys_decode(file, sizeof file, back) == 0, "a good file did not decode");
    check(!back[0].valid && back[1].valid, "slots moved in the round trip");
    check(back[1].kind == TANG_BTPAD_KIND_SWITCH && back[1].flags == 0x01, "kind or flags lost");
    check(memcmp(back[1].addr, addr, 6) == 0 && memcmp(back[1].key, k[1].key, 16) == 0, "address or key lost");
    check(strcmp(back[1].name, "Pro Controller") == 0, "name lost");
    file[8 + TANG_BTPAD_RECORD_LEN + 12] ^= 0x01;           /* one bit of the key */
    check(tang_btpad_keys_decode(file, sizeof file, back) != 0, "a damaged file decoded");
    check(!back[0].valid && !back[1].valid, "a damaged file left a slot valid");
    file[8 + TANG_BTPAD_RECORD_LEN + 12] ^= 0x01;
    check(tang_btpad_keys_decode(file, sizeof file - 1, back) != 0, "a short file decoded");
    file[4] = 2;
    check(tang_btpad_keys_decode(file, sizeof file, back) != 0, "another version decoded");

    printf("btpad: %s (%d checks)\n", fails ? "FAIL" : "PASS", checks);
    return fails ? 1 : 0;
}
