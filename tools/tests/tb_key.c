/*
 * Host test for the keyboard-to-typing translation.
 *
 * Like the pad test, this is pure logic against an exact wire format, and the
 * failure mode is silence: a wrong byte is not rejected by the desktop's
 * parser, it is simply a different key, and a truncated escape sequence is
 * read as later input.  Neither shows up in a build, so the bytes are checked
 * one by one.
 *
 *   tools/tests/test_key.sh
 */
#include <stdio.h>
#include <string.h>

#include "tang_key.h"
#include "tang_pad.h"

static int fails;
static int checks;

static void check_str(const char *what, const char *got, const char *want)
{
    checks++;
    if (strcmp(got, want) != 0) {
        fails++;
        printf("  FAIL: %s\n        got  ", what);
        for (const char *p = got; *p; p++) printf("%02x ", (unsigned char)*p);
        printf("\n        want ");
        for (const char *p = want; *p; p++) printf("%02x ", (unsigned char)*p);
        printf("\n");
    }
}

static void check_int(const char *what, long got, long want)
{
    checks++;
    if (got != want) {
        fails++;
        printf("  FAIL: %s (got %ld, want %ld)\n", what, got, want);
    }
}

/* A report with up to two keys, which is all these cases need. */
static int press(tang_key_t *k, uint8_t mods, uint8_t k0, uint8_t k1,
                 uint32_t now, char *out, size_t cap)
{
    uint8_t keys[6] = { k0, k1, 0, 0, 0, 0 };
    const int n = tang_key_step(k, mods, keys, true, now, out, cap);
    out[n] = '\0';
    return n;
}

/* The same report with the link gone quiet: what a dead link looks like once
 * the heartbeat stops arriving. */
static int press_stale(tang_key_t *k, uint8_t mods, uint8_t k0,
                       uint32_t now, char *out, size_t cap)
{
    uint8_t keys[6] = { k0, 0, 0, 0, 0, 0 };
    const int n = tang_key_step(k, mods, keys, false, now, out, cap);
    out[n] = '\0';
    return n;
}

static int release(tang_key_t *k, uint32_t now, char *out, size_t cap)
{
    uint8_t keys[6] = { 0, 0, 0, 0, 0, 0 };
    const int n = tang_key_step(k, 0, keys, true, now, out, cap);
    out[n] = '\0';
    return n;
}

int main(void)
{
    char out[128];

    /* Plain and shifted characters. */
    tang_key_t k;
    tang_key_reset(&k);
    press(&k, 0, 0x04, 0, 1000, out, sizeof(out));
    check_str("plain 'a'", out, "a");

    tang_key_reset(&k);
    press(&k, TANG_KEY_LSHIFT, 0x04, 0, 1000, out, sizeof(out));
    check_str("shift+'a' -> 'A'", out, "A");

    tang_key_reset(&k);
    press(&k, 0, 0x1E, 0, 1000, out, sizeof(out));
    check_str("'1'", out, "1");

    tang_key_reset(&k);
    press(&k, TANG_KEY_RSHIFT, 0x1E, 0, 1000, out, sizeof(out));
    check_str("rightshift+'1' -> '!'", out, "!");

    tang_key_reset(&k);
    press(&k, TANG_KEY_LSHIFT, 0x2F, 0, 1000, out, sizeof(out));
    check_str("shift+'[' -> '{'", out, "{");

    /* Control characters, in the forms the parser names. */
    tang_key_reset(&k);
    press(&k, 0, 0x28, 0, 1000, out, sizeof(out));
    check_str("Enter -> CR", out, "\r");

    tang_key_reset(&k);
    press(&k, 0, 0x2A, 0, 1000, out, sizeof(out));
    check_str("Backspace -> DEL", out, "\x7f");

    tang_key_reset(&k);
    press(&k, 0, 0x2B, 0, 1000, out, sizeof(out));
    check_str("Tab -> TAB", out, "\t");

    tang_key_reset(&k);
    press(&k, 0, 0x2C, 0, 1000, out, sizeof(out));
    check_str("Space", out, " ");

    tang_key_reset(&k);
    press(&k, 0, 0x29, 0, 1000, out, sizeof(out));
    check_str("Esc -> lone ESC", out, "\x1b");

    tang_key_reset(&k);
    press(&k, TANG_KEY_LCTRL, 0x04, 0, 1000, out, sizeof(out));
    check_str("ctrl+'a' -> 0x01", out, "\x01");

    tang_key_reset(&k);
    press(&k, TANG_KEY_RCTRL, 0x1D, 0, 1000, out, sizeof(out));
    check_str("ctrl+'z' -> 0x1a", out, "\x1a");

    /* Navigation, which must arrive as sequences rather than as text. */
    tang_key_reset(&k);
    press(&k, 0, 0x52, 0, 1000, out, sizeof(out));
    check_str("Up -> CSI A", out, "\x1b[A");

    tang_key_reset(&k);
    press(&k, 0, 0x50, 0, 1000, out, sizeof(out));
    check_str("Left -> CSI D", out, "\x1b[D");

    tang_key_reset(&k);
    press(&k, 0, 0x4C, 0, 1000, out, sizeof(out));
    check_str("Delete -> CSI 3~", out, "\x1b[3~");

    tang_key_reset(&k);
    press(&k, 0, 0x4A, 0, 1000, out, sizeof(out));
    check_str("Home -> CSI H", out, "\x1b[H");

    /* Two keys at once are two characters. */
    tang_key_reset(&k);
    press(&k, 0, 0x04, 0x05, 1000, out, sizeof(out));
    check_str("'a' and 'b' together", out, "ab");

    /* A report that repeats the same held key types nothing by itself. */
    tang_key_reset(&k);
    press(&k, 0, 0x04, 0, 1000, out, sizeof(out));
    check_str("first press", out, "a");
    press(&k, 0, 0x04, 0, 1050, out, sizeof(out));
    check_str("still held, before the delay", out, "");

    /* ...but it does once the delay has passed, and then at the rate. */
    press(&k, 0, 0x04, 0, 1600, out, sizeof(out));
    check_str("held past the delay -> repeat", out, "a");
    press(&k, 0, 0x04, 0, 1620, out, sizeof(out));
    check_str("before the repeat rate -> silent", out, "");
    press(&k, 0, 0x04, 0, 1660, out, sizeof(out));
    check_str("at the repeat rate -> repeat", out, "a");

    /* The link dying while a key is held must not repeat it forever.
     *
     * This is the bug that made the desktop lock up: the last report is frozen,
     * the held key looks held, and the repeat drives the desktop to repaint
     * for as long as the link stays broken.  Reports arriving is what
     * "held" means; silence is the link, not the key. */
    tang_key_reset(&k);
    press(&k, 0, 0x04, 0, 1000, out, sizeof(out));
    check_str("stale test: first press", out, "a");
    /* Heartbeat still arriving: repeat behaves. */
    press(&k, 0, 0x04, 0, 1600, out, sizeof(out));
    check_str("stale test: alive and held -> repeats", out, "a");
    /* Now the link stops.  A report is still "fresh" for TANG_KEY_REPORT_TIMEOUT_MS
     * after it arrives, so repeat continues through that window and then
     * stops -- which is exactly the behaviour that matters: the last few
     * repeats cover the gap between heartbeats, and a link that is really gone
     * goes quiet instead of repeating forever. */
    press_stale(&k, 0, 0x04, 1700, out, sizeof(out));
    check_str("stale test: 100ms since the last report -> still repeats", out, "a");
    press_stale(&k, 0, 0x04, 1900, out, sizeof(out));
    check_str("stale test: at the timeout -> stops", out, "");
    press_stale(&k, 0, 0x04, 5000, out, sizeof(out));
    check_str("stale test: long dead -> must not repeat", out, "");

    /* And a fresh report afterwards starts typing again. */
    tang_key_reset(&k);
    press(&k, 0, 0x05, 0, 6000, out, sizeof(out));
    check_str("stale test: link back -> types again", out, "b");

    /* Releasing stops it. */
    release(&k, 1700, out, sizeof(out));
    check_str("release types nothing", out, "");
    press(&k, 0, 0x04, 0, 2000, out, sizeof(out));
    check_str("a fresh press types once", out, "a");

    /* A buffer too small for a sequence drops it whole rather than truncating. */
    tang_key_reset(&k);
    uint8_t keys[6] = { 0x4C, 0, 0, 0, 0, 0 };   /* Delete -> "\x1b[3~" */
    char tiny[3];
    const int tn = tang_key_step(&k, 0, keys, true, 1000, tiny, sizeof(tiny));
    check_int("truncated sequence refused, not split", tn, 0);

    /* Nothing held, nothing typed. */
    tang_key_reset(&k);
    uint8_t none[6] = { 0, 0, 0, 0, 0, 0 };
    const int nn = tang_key_step(&k, 0, none, true, 1000, out, sizeof(out));
    check_int("idle report types nothing", nn, 0);

    /* F12 is reserved for switching the screen: it never types and never
     * becomes the repeating key. */
    tang_key_reset(&k);
    press(&k, 0, 0x45, 0, 1000, out, sizeof(out));
    check_str("F12 types nothing", out, "");
    press(&k, 0, 0x45, 0, 2000, out, sizeof(out));
    check_str("F12 held does not repeat", out, "");
    {
        const uint8_t f12[6] = { 0x04, 0x45, 0, 0, 0, 0 };
        const uint8_t a_only[6] = { 0x04, 0, 0, 0, 0, 0 };
        check_int("toggle seen in a report", tang_key_toggle_down(f12), 1);
        check_int("toggle absent from a report", tang_key_toggle_down(a_only), 0);
    }

    /* Pressing F12 while a key repeats must not steal the repeat: the held
     * key goes on repeating, because F12 is not a key to the typing path. */
    tang_key_reset(&k);
    press(&k, 0, 0x04, 0, 0, out, sizeof(out));
    check_str("F12 test: a typed", out, "a");
    press(&k, 0, 0x04, 0x45, 600, out, sizeof(out));
    check_str("F12 test: a keeps repeating with F12 down", out, "a");

    /* Absorbed while the core had the screen: a key still held when TinyDesk
     * comes back is neither a fresh press nor a repeat. */
    tang_key_reset(&k);
    {
        const uint8_t held[6] = { 0x52, 0, 0, 0, 0, 0 };   /* Up, held in a game */
        tang_key_absorb(&k, 0, held);
    }
    press(&k, 0, 0x52, 0, 1000, out, sizeof(out));
    check_str("absorbed key: not a press on return", out, "");
    press(&k, 0, 0x52, 0, 1700, out, sizeof(out));
    check_str("absorbed key: does not repeat", out, "");
    release(&k, 1800, out, sizeof(out));
    press(&k, 0, 0x52, 0, 1900, out, sizeof(out));
    check_str("absorbed key: pressed again types", out, "\x1b[A");

    /* Pointer mode for a keyboard the core does not see: left-alt turns the
     * arrows, Enter and Esc into pad bits and withholds them, and leaves every
     * other key to the typing path. */
    {
        uint8_t keys[6] = { 0x52, 0x04, 0x28, 0x29, 0x4F, 0x51 };
        const uint16_t pad = tang_key_pointer(TANG_KEY_LALT, keys);
        check_int("pointer: Up, Enter, Esc, Right, Down to pad bits", pad,
                  TANG_PAD_UP | TANG_PAD_A | TANG_PAD_B | TANG_PAD_RIGHT | TANG_PAD_DOWN);
        check_int("pointer: Up withheld", keys[0], 0);
        check_int("pointer: a left for typing", keys[1], 0x04);
        check_int("pointer: Enter withheld", keys[2], 0);
        check_int("pointer: Esc withheld", keys[3], 0);
        check_int("pointer: Right withheld", keys[4], 0);
        check_int("pointer: Down withheld", keys[5], 0);
    }
    {
        uint8_t keys[6] = { 0x50, 0, 0, 0, 0, 0 };
        check_int("pointer: Left is bit 6", tang_key_pointer(TANG_KEY_LALT, keys),
                  TANG_PAD_LEFT);
    }
    {
        /* Right-alt is not the mode (patch 0006 moved it to left-alt). */
        uint8_t keys[6] = { 0x52, 0x28, 0, 0, 0, 0 };
        check_int("pointer: right-alt is not pointer mode",
                  tang_key_pointer(TANG_KEY_RALT, keys), 0);
        check_int("pointer: right-alt leaves Up", keys[0], 0x52);
        check_int("pointer: no modifier is not pointer mode",
                  tang_key_pointer(0, keys), 0);
        check_int("pointer: no modifier leaves Enter", keys[1], 0x28);
    }

    printf("tb_key: %d checks, %d failures\n", checks, fails);
    return fails == 0 ? 0 : 1;
}
