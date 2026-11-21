// TinyTang — Classic Bluetooth gamepads: file format and report decoding.
// See tang_btpad_codec.h.
//
// SPDX-License-Identifier: MIT
#include "tang_btpad_codec.h"

#include <string.h>

#include "tang_ble_bonds.h"   /* tang_bonds_crc32 */
#include "tang_pad.h"
#include "tang_pad_stick.h"

static const uint8_t k_magic[4] = { 'T', 'T', 'B', 'R' };

size_t tang_btpad_keys_encode(const tang_btpad_key_t k[TANG_BTPAD_SLOTS], uint8_t *out)
{
    memset(out, 0, TANG_BTPAD_FILE_LEN);
    memcpy(out, k_magic, 4);
    out[4] = TANG_BTPAD_VERSION;
    out[5] = TANG_BTPAD_SLOTS;
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        uint8_t *r = out + 8 + i * TANG_BTPAD_RECORD_LEN;
        if (!k[i].valid) {
            continue;
        }
        r[0] = 1;
        r[1] = k[i].kind;
        r[2] = k[i].flags;
        r[3] = k[i].off ? 1 : 0;
        memcpy(r + 4, k[i].addr, 6);
        memcpy(r + 10, k[i].key, 16);
        memcpy(r + 26, k[i].name, TANG_BTPAD_NAME_LEN);
        r[26 + TANG_BTPAD_NAME_LEN - 1] = '\0';
    }
    const size_t body = 8 + TANG_BTPAD_SLOTS * TANG_BTPAD_RECORD_LEN;
    const uint32_t crc = tang_bonds_crc32(out, body);
    for (int b = 0; b < 4; b++) {
        out[body + b] = (uint8_t)(crc >> (8 * b));
    }
    return TANG_BTPAD_FILE_LEN;
}

int tang_btpad_keys_decode(const uint8_t *in, size_t len, tang_btpad_key_t k[TANG_BTPAD_SLOTS])
{
    memset(k, 0, sizeof(tang_btpad_key_t) * TANG_BTPAD_SLOTS);
    if (len != TANG_BTPAD_FILE_LEN || memcmp(in, k_magic, 4) != 0 ||
        in[4] != TANG_BTPAD_VERSION || in[5] != TANG_BTPAD_SLOTS) {
        return -1;
    }
    const size_t body = 8 + TANG_BTPAD_SLOTS * TANG_BTPAD_RECORD_LEN;
    const uint32_t crc = (uint32_t)in[body] | ((uint32_t)in[body + 1] << 8) |
                         ((uint32_t)in[body + 2] << 16) | ((uint32_t)in[body + 3] << 24);
    if (crc != tang_bonds_crc32(in, body)) {
        return -1;
    }
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        const uint8_t *r = in + 8 + i * TANG_BTPAD_RECORD_LEN;
        if (r[0] != 1) {
            continue;
        }
        k[i].valid = true;
        k[i].kind = r[1];
        k[i].flags = r[2];
        k[i].off = r[3] != 0;
        memcpy(k[i].addr, r + 4, 6);
        memcpy(k[i].key, r + 10, 16);
        memcpy(k[i].name, r + 26, TANG_BTPAD_NAME_LEN);
        k[i].name[TANG_BTPAD_NAME_LEN - 1] = '\0';
    }
    return 0;
}

bool tang_btpad_switch_decode(const uint8_t *rep, size_t len, tang_btpad_input_t *in)
{
    if (len < 13 || rep[0] != 0xA1 || rep[1] != 0x3F) {
        return false;
    }
    in->buttons = (uint16_t)(rep[2] | (rep[3] << 8));
    in->hat = (uint8_t)(rep[4] & 0x0F);
    in->lx = (uint16_t)(rep[5] | (rep[6] << 8));
    in->ly = (uint16_t)(rep[7] | (rep[8] << 8));
    in->rx = (uint16_t)(rep[9] | (rep[10] << 8));
    in->ry = (uint16_t)(rep[11] | (rep[12] << 8));
    return true;
}

/* The directions: the hat, or the left stick past the dead zone. */
static uint16_t directions(const tang_btpad_input_t *in)
{
    uint16_t w = 0;
    /* The hat: 0 up, 1 up-right, 2 right ... 7 up-left; 8 and above centred. */
    static const uint16_t hat[8] = {
        TANG_PAD_UP, TANG_PAD_UP | TANG_PAD_RIGHT, TANG_PAD_RIGHT,
        TANG_PAD_DOWN | TANG_PAD_RIGHT, TANG_PAD_DOWN, TANG_PAD_DOWN | TANG_PAD_LEFT,
        TANG_PAD_LEFT, TANG_PAD_UP | TANG_PAD_LEFT,
    };
    if (in->hat < 8) {
        w |= hat[in->hat];
    }
    const int x = (int)in->lx - 0x8000;
    const int y = (int)in->ly - 0x8000;
    if (x <= -TANG_BTPAD_STICK_DEADZONE) {
        w |= TANG_PAD_LEFT;
    } else if (x >= TANG_BTPAD_STICK_DEADZONE) {
        w |= TANG_PAD_RIGHT;
    }
    if (y <= -TANG_BTPAD_STICK_DEADZONE) {
        w |= TANG_PAD_UP;
    } else if (y >= TANG_BTPAD_STICK_DEADZONE) {
        w |= TANG_PAD_DOWN;
    }
    return w;
}

uint16_t tang_btpad_word(const tang_btpad_input_t *in)
{
    uint16_t w = directions(in);
    if (in->buttons & TANG_BTPAD_SW_A) {
        w |= TANG_PAD_A;
    }
    if (in->buttons & TANG_BTPAD_SW_B) {
        w |= TANG_PAD_B;
    }
    return w;
}

uint16_t tang_btpad_core_bits(const tang_btpad_input_t *in)
{
    static const struct { uint16_t sw, pad; } map[] = {
        { TANG_BTPAD_SW_A, TANG_PAD_A },           { TANG_BTPAD_SW_B, TANG_PAD_B },
        { TANG_BTPAD_SW_X, TANG_PAD_X },           { TANG_BTPAD_SW_Y, TANG_PAD_Y },
        { TANG_BTPAD_SW_L, TANG_PAD_LB },          { TANG_BTPAD_SW_R, TANG_PAD_RB },
        { TANG_BTPAD_SW_MINUS, TANG_PAD_SELECT },  { TANG_BTPAD_SW_PLUS, TANG_PAD_START },
        { TANG_BTPAD_SW_HOME, TANG_PAD_HOME },
    };
    uint16_t w = directions(in);
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++) {
        if (in->buttons & map[i].sw) {
            w |= map[i].pad;
        }
    }
    return w;
}

void tang_btpad_tap_report(tang_btpad_tap_t *t, uint16_t buttons, uint32_t now_ms)
{
    const bool home = (buttons & TANG_BTPAD_SW_HOME) != 0;
    const bool others = (buttons & (uint16_t)~TANG_BTPAD_SW_HOME) != 0;
    if (home && !t->down) {
        t->alone = !others;
    } else if (others) {
        t->alone = false;
    }
    if (!home && t->down && t->alone) {
        t->tapped = true;
        t->until_ms = now_ms + TANG_BTPAD_TAP_MS;
    }
    t->down = home;
}

bool tang_btpad_tap_on(tang_btpad_tap_t *t, uint32_t now_ms)
{
    if (t->tapped && (int32_t)(t->until_ms - now_ms) <= 0) {
        t->tapped = false;
    }
    return t->tapped;
}

void tang_btpad_wii_sample(const tang_btpad_input_t *in, tang_wii_sample_t *s)
{
    memset(s, 0, sizeof *s);
    static const struct { uint16_t sw; uint32_t wii; } map[] = {
        { TANG_BTPAD_SW_A, TANG_WII_BTN_A },     { TANG_BTPAD_SW_B, TANG_WII_BTN_B },
        { TANG_BTPAD_SW_X, TANG_WII_BTN_X },     { TANG_BTPAD_SW_Y, TANG_WII_BTN_Y },
        { TANG_BTPAD_SW_L, TANG_WII_BTN_L },
        /* L, R, ZL and ZR are the games'; the key map gives them nothing. */
        { TANG_BTPAD_SW_ZL, TANG_WII_BTN_ZL },   { TANG_BTPAD_SW_ZR, TANG_WII_BTN_ZR },
        { TANG_BTPAD_SW_PLUS, TANG_WII_BTN_PLUS }, { TANG_BTPAD_SW_MINUS, TANG_WII_BTN_MINUS },
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++) {
        if (in->buttons & map[i].sw) {
            s->btn |= map[i].wii;
        }
    }
    static const uint32_t hat[8] = {
        TANG_WII_BTN_UP, TANG_WII_BTN_UP | TANG_WII_BTN_RIGHT, TANG_WII_BTN_RIGHT,
        TANG_WII_BTN_DOWN | TANG_WII_BTN_RIGHT, TANG_WII_BTN_DOWN,
        TANG_WII_BTN_DOWN | TANG_WII_BTN_LEFT, TANG_WII_BTN_LEFT,
        TANG_WII_BTN_UP | TANG_WII_BTN_LEFT,
    };
    if (in->hat < 8) {
        s->btn |= hat[in->hat];
    }
    s->left = (in->buttons & TANG_BTPAD_SW_A) != 0;
    s->right = (in->buttons & TANG_BTPAD_SW_B) != 0;
}

int tang_btpad_axis_counts(uint16_t v, int max_counts)
{
    return tang_stick_counts((int)v - 0x8000, max_counts);
}

void tang_btpad_sticks(const tang_btpad_input_t *in, bool console_mode, uint32_t now_ms,
                       uint32_t *next_wheel_ms, tang_mouse_t *m)
{
    /* The pad's own scale is the standard one, and its Y already grows
     * downward as the mouse's does (tang_pad.h): no negation. */
    tang_stick_apply((int)in->lx - 0x8000, (int)in->ly - 0x8000, (int)in->ry - 0x8000,
                     true, console_mode, now_ms, next_wheel_ms, m);
}
