// TinyTang — the controller as the desktop's pointer.
//
// See tang_pad.h for the bit assignments and why they are what they are.  This
// file is the translation: pad word in, xterm SGR mouse reports out.

#include "tang_pad.h"

#include <stdio.h>
#include <string.h>

static int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void tang_pad_reset(tang_pad_t *p, int cols, int rows)
{
    p->cols = cols;
    p->rows = rows;
    p->x = cols / 2;
    p->y = rows / 2;
    p->fx = 0;
    p->fy = 0;
    p->prev = 0;
    p->left_down = false;
    p->right_down = false;
    p->middle_down = false;
}

void tang_mouse_boot_add(tang_mouse_t *m, const uint8_t *report, size_t len)
{
    if (len < 3) {
        return;
    }
    m->buttons = report[0];
    m->dx += (int8_t)report[1];
    m->dy += (int8_t)report[2];
    if (len >= 4) {
        m->wheel += (int8_t)report[3];
    }
}

/* Mouse counts to whole cells, keeping the remainder so slow movement still
 * adds up.  At an edge the remainder is dropped: pushing against the edge
 * should not store up movement that comes out when the mouse turns back. */
static void mouse_cells(int *frac, int counts, int pos, int max, int *moved_to)
{
    *frac += counts;
    const int cells = *frac / TANG_MOUSE_COUNTS_PER_CELL;
    *frac -= cells * TANG_MOUSE_COUNTS_PER_CELL;
    const int want = pos + cells;
    *moved_to = clampi(want, 0, max);
    if (*moved_to != want) {
        *frac = 0;
    }
}

/* SGR mouse: ESC [ < b ; x ; y M for a press or motion, m for a release.
 * In `b`, 32 marks motion and 3 means "no button"; coordinates are one-based.
 * A sequence that will not fit is dropped rather than truncated -- half an
 * escape sequence is worse than none, because it desynchronises the parser. */
static int emit(char *out, int n, size_t cap, int b, char final, int x, int y)
{
    char seq[32];
    const int len = snprintf(seq, sizeof(seq), "\x1b[<%d;%d;%d%c", b, x + 1, y + 1, final);
    if (len <= 0 || (size_t)len >= sizeof(seq) || (size_t)n + (size_t)len > cap) {
        return n;
    }
    memcpy(out + n, seq, (size_t)len);
    return n + len;
}

int tang_pad_step(tang_pad_t *p, uint16_t joy1, char *out, size_t cap, bool *toggle)
{
    return tang_pad_pointer(p, joy1, NULL, out, cap, toggle);
}

int tang_pad_pointer(tang_pad_t *p, uint16_t joy1, const tang_mouse_t *m,
                     char *out, size_t cap, bool *toggle)
{
    int n = 0;
    *toggle = false;

    /* L is a toggle, so it is the edge that matters: reporting the level would
     * flip the screen over and over for as long as the button was held. */
    if ((uint16_t)(joy1 & (uint16_t)~p->prev) & TANG_PAD_LB) {
        *toggle = true;
    }

    /* One cell per step.  A step comes every few tens of milliseconds, so a
     * held direction crosses the grid in a second or two -- fast enough to be
     * usable, slow enough to aim.  Opposite directions cancel: a pad that is
     * somehow reporting both should sit still, not pick a winner. */
    const int dx = ((joy1 & TANG_PAD_RIGHT) != 0) - ((joy1 & TANG_PAD_LEFT) != 0);
    const int dy = ((joy1 & TANG_PAD_DOWN)  != 0) - ((joy1 & TANG_PAD_UP)   != 0);

    int nx = clampi(p->x + dx, 0, p->cols - 1);
    int ny = clampi(p->y + dy, 0, p->rows - 1);
    if (m != NULL) {
        mouse_cells(&p->fx, m->dx, nx, p->cols - 1, &nx);
        mouse_cells(&p->fy, m->dy, ny, p->rows - 1, &ny);
    }
    const bool moved = (nx != p->x) || (ny != p->y);
    p->x = nx;
    p->y = ny;

    const uint8_t mb = m != NULL ? m->buttons : 0;
    const bool a = (joy1 & TANG_PAD_A) != 0 || (mb & TANG_MOUSE_LEFT) != 0;
    const bool b = (joy1 & TANG_PAD_B) != 0 || (mb & TANG_MOUSE_RIGHT) != 0;
    const bool c = (mb & TANG_MOUSE_MIDDLE) != 0;

    /* The button state at the moment of the move, so a held button drags a
     * window instead of pointing at it. */
    int held = 3;
    if (p->left_down) {
        held = 0;
    } else if (p->middle_down) {
        held = 1;
    } else if (p->right_down) {
        held = 2;
    }

    if (moved) {
        n = emit(out, n, cap, 32 | held, 'M', p->x, p->y);
    }
    if (a != p->left_down) {
        n = emit(out, n, cap, 0, a ? 'M' : 'm', p->x, p->y);
        p->left_down = a;
    }
    if (b != p->right_down) {
        n = emit(out, n, cap, 2, b ? 'M' : 'm', p->x, p->y);
        p->right_down = b;
    }
    if (c != p->middle_down) {
        n = emit(out, n, cap, 1, c ? 'M' : 'm', p->x, p->y);
        p->middle_down = c;
    }

    /* The wheel: one press per detent, 64 up and 65 down, which is what the
     * desktop's parser reads; a wheel has no release. */
    if (m != NULL && m->wheel != 0) {
        const int up = m->wheel > 0;
        int count = up ? m->wheel : -m->wheel;
        if (count > TANG_MOUSE_WHEEL_MAX) {
            count = TANG_MOUSE_WHEEL_MAX;
        }
        for (int i = 0; i < count; i++) {
            n = emit(out, n, cap, up ? 64 : 65, 'M', p->x, p->y);
        }
    }

    p->prev = joy1;
    return n;
}
