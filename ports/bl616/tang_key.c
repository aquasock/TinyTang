// TinyTang — the keyboard as the desktop's typing input.  See tang_key.h.
//
// SPDX-License-Identifier: MIT

#include "tang_key.h"

#include <string.h>

/* The printable half of HID usage page 0x07, which is the range a boot
 * keyboard's report can name: usages 0x04 through 0x38 are the letters, the
 * digits and the punctuation.  Two bytes each because the byte that reaches
 * the parser is the *result*, not the intent -- Shift is applied here rather
 * than described, since the parser has no modifier to apply it with.
 *
 * The layout is US.  A keyboard whose firmware sends usages from another
 * layout still arrives as usages, so this stays correct for the report; what
 * changes is which character the user expected, and that is the keyboard's
 * business, not this table's. */
static const char usage_plain[0x39 - 0x04] = {
    /* 0x04 */ 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
    /* 0x0c */ 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
    /* 0x14 */ 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    /* 0x1c */ 'y', 'z', '1', '2', '3', '4', '5', '6',
    /* 0x24 */ '7', '8', '9', '0', '\r', 0x1b, 0x7f, '\t',
    /* 0x2c */ ' ', '-', '=', '[', ']', '\\', 0, ';',
    /* 0x34 */ '\'', '`', ',', '.', '/',
};

static const char usage_shifted[0x39 - 0x04] = {
    /* 0x04 */ 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    /* 0x0c */ 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    /* 0x14 */ 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    /* 0x1c */ 'Y', 'Z', '!', '@', '#', '$', '%', '^',
    /* 0x24 */ '&', '*', '(', ')', '\r', 0x1b, 0x7f, '\t',
    /* 0x2c */ ' ', '_', '+', '{', '}', '|', 0, ':', 
    /* 0x34 */ '"', '~', '<', '>', '?',
};

/* The editing and navigation keys, as the escape sequences a terminal sends.
 * These are the forms the parser already decodes -- input.c's CSI handling --
 * so they arrive as key events rather than as text. */
static const char *const csi_sequences[0x4F - 0x49 + 1] = {
    /* 0x49 Insert */ "\x1b[2~",
    /* 0x4a Home   */ "\x1b[H",
    /* 0x4b PgUp   */ "\x1b[5~",
    /* 0x4c Delete */ "\x1b[3~",
    /* 0x4d End    */ "\x1b[F",
    /* 0x4e PgDn   */ "\x1b[6~",
    /* 0x4f Right  */ "\x1b[C",
};

/* The arrow keys sit above that range and are not contiguous with it. */
static const char *csi_for(uint8_t usage)
{
    switch (usage) {
    case 0x50: return "\x1b[D";   /* Left  */
    case 0x51: return "\x1b[B";   /* Down  */
    case 0x52: return "\x1b[A";   /* Up    */
    default: break;
    }
    if (usage >= 0x49 && usage <= 0x4F) {
        return csi_sequences[usage - 0x49];
    }
    return NULL;
}

/* Append, or refuse rather than write half of something.  A partial escape
 * sequence is worse than a lost keystroke: the parser would read the rest of
 * it as later input. */
static int append(char *out, int n, size_t cap, const char *bytes, size_t len)
{
    if (len == 0 || (size_t)n + len > cap) {
        return n;
    }
    memcpy(out + n, bytes, len);
    return n + (int)len;
}

static int append_byte(char *out, int n, size_t cap, char b)
{
    if ((size_t)n + 1 > cap) {
        return n;
    }
    out[n] = b;
    return n + 1;
}

/* One usage code, translated to what the parser should receive.  Returns the
 * number of bytes appended. */
static int emit_usage(uint8_t usage, uint8_t mods, char *out, int n, size_t cap)
{
    const bool ctrl = (mods & (TANG_KEY_LCTRL | TANG_KEY_RCTRL)) != 0;
    const bool shift = (mods & (TANG_KEY_LSHIFT | TANG_KEY_RSHIFT)) != 0;

    if (usage == 0) {
        return n;
    }

    const char *csi = csi_for(usage);
    if (csi != NULL) {
        return append(out, n, cap, csi, strlen(csi));
    }

    if (usage >= 0x04 && usage <= 0x38) {
        const char plain = usage_plain[usage - 0x04];
        const char upper = usage_shifted[usage - 0x04];
        if (plain == 0) {
            return n;   /* 0x32, the non-US hash key: nothing to type here */
        }
        /* Ctrl with a letter is the control byte, 0x01..0x1a, which is exactly
         * what the parser turns back into ctrl+letter.  Ctrl+Space is 0x00,
         * which it reads as ctrl+space. */
        if (ctrl) {
            if (plain >= 'a' && plain <= 'z') {
                return append_byte(out, n, cap, (char)(plain - 'a' + 1));
            }
            if (plain == ' ') {
                return append_byte(out, n, cap, 0x00);
            }
        }
        return append_byte(out, n, cap, shift ? upper : plain);
    }

    return n;
}

void tang_key_reset(tang_key_t *k)
{
    memset(k, 0, sizeof(*k));
}

void tang_key_absorb(tang_key_t *k, uint8_t mods, const uint8_t keys[6])
{
    k->mods = mods;
    memcpy(k->keys, keys, sizeof(k->keys));
    k->held = false;
}

static bool key_in(const uint8_t keys[6], uint8_t usage)
{
    for (int i = 0; i < 6; i++) {
        if (keys[i] == usage) {
            return true;
        }
    }
    return false;
}

bool tang_key_toggle_down(const uint8_t keys[6])
{
    return key_in(keys, TANG_KEY_USAGE_TOGGLE);
}

int tang_key_step(tang_key_t *k, uint8_t mods, const uint8_t keys[6],
                  bool report_fresh, uint32_t now_ms, char *out, size_t cap)
{
    int n = 0;

    if (report_fresh) {
        k->last_report = now_ms;
    }

    /* A key that is down now and was not down last time is a press.  The
     * report carries state, not events, so the edge has to be found here. */
    uint8_t pressed = 0;
    for (int i = 0; i < 6; i++) {
        const uint8_t usage = keys[i];
        if (usage == 0 || usage == TANG_KEY_USAGE_TOGGLE) {
            continue;
        }
        if (!key_in(k->keys, usage)) {
            n = emit_usage(usage, mods, out, n, cap);
            pressed = usage;
        }
    }

    /* Auto-repeat: the device will not do it, because a boot keyboard reports
     * only state and leaves the rate to the host.  Only the last key pressed
     * repeats, which is what a desktop does -- the most recent key wins rather
     * than all held keys repeating at once. */
    if (pressed != 0) {
        k->held = true;
        k->hold_start = now_ms;
        k->last_repeat = now_ms;
    } else if (k->held) {
        /* Find the key still down that we were repeating. */
        uint8_t still = 0;
        for (int i = 5; i >= 0; i--) {
            if (keys[i] != 0 && keys[i] != TANG_KEY_USAGE_TOGGLE &&
                key_in(k->keys, keys[i])) {
                still = keys[i];
                break;
            }
        }
        /* Silence means the link, not the key.  Without this the last report
         * would repeat forever once the core stopped reporting, and a repeated
         * key is not a passive stall -- it drives the desktop to repaint, which
         * keeps the very link that failed busy. */
        const bool alive =
            (uint32_t)(now_ms - k->last_report) < TANG_KEY_REPORT_TIMEOUT_MS;
        if (still == 0 || !alive) {
            k->held = false;
        } else if ((uint32_t)(now_ms - k->hold_start) >= TANG_KEY_REPEAT_DELAY_MS &&
                   (uint32_t)(now_ms - k->last_repeat) >= TANG_KEY_REPEAT_RATE_MS) {
            n = emit_usage(still, mods, out, n, cap);
            k->last_repeat = now_ms;
        }
    }

    k->mods = mods;
    memcpy(k->keys, keys, sizeof(k->keys));
    return n;
}
