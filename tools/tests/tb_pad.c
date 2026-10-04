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

    printf("tang_pad: %s (%d checks)\n", fails == 0 ? "PASS" : "FAIL", checks);
    return fails == 0 ? 0 : 1;
}
