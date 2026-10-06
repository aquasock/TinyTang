/*
 * Host test for the controller-to-pointer translation.
 *
 * Pure logic with an exact wire format on the other side, which makes it worth
 * checking byte for byte: a wrong bit moves the pointer the wrong way, and a
 * wrong sequence is silently ignored by the desktop's parser rather than
 * reported.  Neither shows up in a build.
 *
 *   tools/tests/test_pad.sh
 */
#include <stdio.h>
#include <string.h>

#include "tang_pad.h"

static int fails;
static int checks;

static void check_str(const char *what, const char *got, const char *want)
{
    checks++;
    if (strcmp(got, want) != 0) {
        fails++;
        printf("  FAIL: %s\n        got  \"%s\"\n        want \"%s\"\n", what, got, want);
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

/* Run one pad reading and return the bytes it produced. */
static const char *step(tang_pad_t *p, uint16_t joy1, bool *toggle)
{
    static char out[128];
    const int n = tang_pad_step(p, joy1, out, sizeof(out) - 1, toggle);
    out[n] = '\0';
    return out;
}

int main(void)
{
    tang_pad_t p;
    bool toggle;

    /* 80x45 starts the pointer in the middle: cell (40, 22). */
    tang_pad_reset(&p, 80, 45);
    check_int("start x", p.x, 40);
    check_int("start y", p.y, 22);

    /* Nothing held, nothing sent. */
    check_str("idle", step(&p, 0, &toggle), "");
    check_int("idle does not toggle", toggle, 0);

    /* Right: one cell, reported one-based, as motion with no button. */
    check_str("right", step(&p, TANG_PAD_RIGHT, &toggle), "\x1b[<35;42;23M");
    check_int("x after right", p.x, 41);
    check_str("down", step(&p, TANG_PAD_DOWN, &toggle), "\x1b[<35;42;24M");

    /* A held button clicks, then drags when the pointer moves. */
    check_str("A press", step(&p, TANG_PAD_A, &toggle), "\x1b[<0;42;24M");
    check_str("A held again sends nothing",
              step(&p, TANG_PAD_A, &toggle), "");
    check_str("A drag", step(&p, TANG_PAD_A | TANG_PAD_RIGHT, &toggle),
              "\x1b[<32;43;24M");
    check_str("A release", step(&p, 0, &toggle), "\x1b[<0;43;24m");

    /* B is the right button, which is what opens an icon's menu. */
    check_str("B press", step(&p, TANG_PAD_B, &toggle), "\x1b[<2;43;24M");
    check_str("B release", step(&p, 0, &toggle), "\x1b[<2;43;24m");

    /* L toggles on the edge, not the level. */
    check_str("LB press", step(&p, TANG_PAD_LB, &toggle), "");
    check_int("LB toggles", toggle, 1);
    step(&p, TANG_PAD_LB, &toggle);
    check_int("LB held does not toggle again", toggle, 0);
    step(&p, 0, &toggle);
    check_str("LB press again", step(&p, TANG_PAD_LB, &toggle), "");
    check_int("LB toggles again", toggle, 1);

    /* The pointer stays on the grid. */
    tang_pad_reset(&p, 80, 45);
    for (int i = 0; i < 200; i++) {
        step(&p, TANG_PAD_RIGHT, &toggle);
    }
    check_int("clamped at the right edge", p.x, 79);
    for (int i = 0; i < 200; i++) {
        step(&p, TANG_PAD_UP, &toggle);
    }
    check_int("clamped at the top edge", p.y, 0);

    /* Opposite directions cancel rather than stutter. */
    tang_pad_reset(&p, 80, 45);
    check_str("left and right cancel",
              step(&p, TANG_PAD_LEFT | TANG_PAD_RIGHT, &toggle), "");
    check_int("x unchanged", p.x, 40);

    /* ---- the mouse ---- */

    /* A boot report: buttons, X, Y as signed bytes, then an optional wheel.
     * Movement and wheel add up between steps; the buttons are replaced. */
    {
        tang_mouse_t m = { 0 };
        const uint8_t r1[4] = { 0x01, 0x05, 0xFD, 0x01 };   /* left, +5, -3, +1 */
        const uint8_t r2[3] = { 0x00, 0xF6, 0x02 };         /* none, -10, +2 */
        tang_mouse_boot_add(&m, r1, sizeof(r1));
        tang_mouse_boot_add(&m, r2, sizeof(r2));
        check_int("boot: dx adds up", m.dx, -5);
        check_int("boot: dy adds up", m.dy, -1);
        check_int("boot: wheel from the 4th byte", m.wheel, 1);
        check_int("boot: buttons replaced", m.buttons, 0);
        const uint8_t shrt[2] = { 0x01, 0x05 };
        tang_mouse_boot_add(&m, shrt, sizeof(shrt));
        check_int("boot: a short report is ignored", m.dx, -5);
    }

    /* Counts become cells, 16 to a cell, and the remainder is kept. */
    {
        tang_pad_reset(&p, 80, 45);
        static char out[256];
        tang_mouse_t m = { .dx = 10 };
        int n = tang_pad_pointer(&p, 0, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: 10 counts is not a cell yet", out, "");
        m.dx = 10;
        n = tang_pad_pointer(&p, 0, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: 20 counts is one cell", out, "\x1b[<35;42;23M");
        check_int("mouse: remainder kept", p.fx, 4);
        m.dx = 0;
        m.dy = -40;
        n = tang_pad_pointer(&p, 0, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: -40 counts up is two cells", out, "\x1b[<35;42;21M");
        check_int("mouse: negative remainder kept", p.fy, -8);

        /* Pushing into an edge stores nothing up. */
        m.dy = -16 * 100;
        (void)tang_pad_pointer(&p, 0, &m, out, sizeof(out) - 1, &toggle);
        check_int("mouse: clamped at the top", p.y, 0);
        check_int("mouse: no remainder at an edge", p.fy, 0);
    }

    /* Mouse buttons are the pad's buttons: either holds left; middle is 1. */
    {
        tang_pad_reset(&p, 80, 45);
        static char out[256];
        tang_mouse_t m = { .buttons = TANG_MOUSE_LEFT };
        int n = tang_pad_pointer(&p, 0, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: left press", out, "\x1b[<0;41;23M");
        /* The pad's A held as well and then the mouse let go: still held. */
        m.buttons = 0;
        n = tang_pad_pointer(&p, TANG_PAD_A, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: A keeps left held", out, "");
        m.dx = 16;
        n = tang_pad_pointer(&p, TANG_PAD_A, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: moving with left held drags", out, "\x1b[<32;42;23M");
        m.dx = 0;
        n = tang_pad_pointer(&p, 0, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: left release", out, "\x1b[<0;42;23m");
        m.buttons = TANG_MOUSE_RIGHT;
        n = tang_pad_pointer(&p, 0, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: right press", out, "\x1b[<2;42;23M");
        m.buttons = TANG_MOUSE_MIDDLE;
        n = tang_pad_pointer(&p, 0, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: right release, middle press", out,
                  "\x1b[<2;42;23m\x1b[<1;42;23M");
        m.buttons = 0;
        n = tang_pad_pointer(&p, 0, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: middle release", out, "\x1b[<1;42;23m");
    }

    /* The wheel: 64 up, 65 down, one per detent, at most four per step. */
    {
        tang_pad_reset(&p, 80, 45);
        static char out[256];
        tang_mouse_t m = { .wheel = 2 };
        int n = tang_pad_pointer(&p, 0, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: wheel up twice", out, "\x1b[<64;41;23M\x1b[<64;41;23M");
        m.wheel = -9;
        n = tang_pad_pointer(&p, 0, &m, out, sizeof(out) - 1, &toggle);
        out[n] = '\0';
        check_str("mouse: wheel down clamped to four", out,
                  "\x1b[<65;41;23M\x1b[<65;41;23M\x1b[<65;41;23M\x1b[<65;41;23M");
    }

    printf("tang_pad: %s (%d checks)\n", fails == 0 ? "PASS" : "FAIL", checks);
    return fails == 0 ? 0 : 1;
}
