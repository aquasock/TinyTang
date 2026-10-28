/* TinyTang — Wii extension controllers as the desktop's pointer.
 * See tang_wiipad.h for what it is and why it is shaped this way.
 *
 * SPDX-License-Identifier: MIT
 */
#include "tang_wiipad.h"

#include <stdlib.h>
#include <string.h>

#ifdef TANG_WIIPAD_HOST
#define LOCK()   do { } while (0)
#define UNLOCK() do { } while (0)
#else
#include "FreeRTOS.h"
#include "task.h"

#include "tdsh.h"

int tdsh_printf(const char *fmt, ...);

/* The state is touched by the desk task and by the shell command.  Only the
 * state is copied inside the critical section; every bus access happens
 * outside it, because FreeRTOS forbids blocking calls there and the FPGA
 * transaction blocks (TOOL-015). */
#define LOCK()   taskENTER_CRITICAL()
#define UNLOCK() taskEXIT_CRITICAL()
#endif

static tang_wiipad_t s_pad = {
    .kind = TANG_WII_KIND_NONE,
    .state = TANG_WIIPAD_OFF,
    .cx = 128,
    .cy = 128,
    .crx = 16,
    .cry = 16,
};

/* The centers a part starts from before one has been learned: the middle of
 * its own encoding. */
static void default_centers(tang_wiipad_t *n)
{
    /* Both parts report 8-bit sticks in the formats this driver uses, so both
     * start from the middle of that range until a resting sample is learned. */
    n->cx = 128;
    n->cy = 128;
    n->crx = 128;
    n->cry = 128;
}

void tang_wiipad_reset(void)
{
    LOCK();
    memset(&s_pad, 0, sizeof s_pad);
    s_pad.kind = TANG_WII_KIND_NONE;
    s_pad.state = TANG_WIIPAD_OFF;
    default_centers(&s_pad);
    UNLOCK();
}

tang_wiipad_t *tang_wiipad_state(void)
{
    return &s_pad;
}

/* ------------------------------------------------------------- the pieces */

void tang_wii_decode(tang_wii_kind_t kind, const uint8_t b[8],
                     tang_wii_sample_t *s)
{
    memset(s, 0, sizeof *s);

    if (kind == TANG_WII_KIND_CLASSIC) {
        /* Data format 3 (WII-003): left X, right X, left Y, right Y, the two
         * triggers, then the buttons, all axes 8-bit.  A is byte 7 bit 4 and
         * B bit 6, active low. */
        s->lx = b[0];
        s->rx = b[1];
        s->ly = b[2];
        s->ry = b[3];
        s->left = (b[7] & 0x10u) == 0;
        s->right = (b[7] & 0x40u) == 0;
        /* The same two bytes carry every other button, all active low: byte
         * 6 has d-pad R and D, L, minus, Home, plus and R; byte 7 has ZL, B,
         * Y, A, X, ZR, d-pad L and d-pad U (WII-004). */
        if (s->left)  s->btn |= TANG_WII_BTN_A;
        if (s->right) s->btn |= TANG_WII_BTN_B;
        if ((b[7] & 0x08u) == 0) s->btn |= TANG_WII_BTN_X;
        if ((b[7] & 0x20u) == 0) s->btn |= TANG_WII_BTN_Y;
        if ((b[7] & 0x80u) == 0) s->btn |= TANG_WII_BTN_ZL;
        if ((b[7] & 0x04u) == 0) s->btn |= TANG_WII_BTN_ZR;
        if ((b[6] & 0x04u) == 0) s->btn |= TANG_WII_BTN_PLUS;
        if ((b[6] & 0x10u) == 0) s->btn |= TANG_WII_BTN_MINUS;
        if ((b[6] & 0x08u) == 0) s->btn |= TANG_WII_BTN_HOME;
        if ((b[6] & 0x20u) == 0) s->btn |= TANG_WII_BTN_L;
        if ((b[6] & 0x02u) == 0) s->btn |= TANG_WII_BTN_R;
        if ((b[7] & 0x01u) == 0) s->btn |= TANG_WII_BTN_UP;
        if ((b[7] & 0x02u) == 0) s->btn |= TANG_WII_BTN_LEFT;
        if ((b[6] & 0x40u) == 0) s->btn |= TANG_WII_BTN_DOWN;
        if ((b[6] & 0x80u) == 0) s->btn |= TANG_WII_BTN_RIGHT;
        return;
    }

    /* The nunchuck's format: stick, three accelerometer bytes, then Z and C
     * as bits 0 and 1 of the last byte, active low (WII-002). */
    s->lx = b[0];
    s->ly = b[1];
    s->left = (b[5] & 0x01u) == 0;
    s->right = (b[5] & 0x02u) == 0;
}

tang_wii_kind_t tang_wii_identify(const uint8_t id[6])
{
    if (id[2] != 0xA4u || id[3] != 0x20u) {
        return TANG_WII_KIND_NONE;
    }
    /* The fifth byte is the model (0x00 nunchuck, 0x01 Classic) and the sixth
     * the data format, which distinguishes a Classic from a guitar or drum
     * that shares its identity prefix.  A Classic's format byte reads 0x01
     * before the host selects format 3 and 0x03 after (WII-004). */
    if (id[4] == 0x00u && id[5] == 0x00u) {
        return TANG_WII_KIND_NUNCHUCK;
    }
    if (id[5] == 0x01u && (id[4] == 0x01u || id[4] == 0x03u)) {
        return TANG_WII_KIND_CLASSIC;
    }
    return TANG_WII_KIND_NONE;
}

bool tang_wiipad_all_ff(const uint8_t *b, unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        if (b[i] != 0xFFu) {
            return false;
        }
    }
    return true;
}

/* One axis: dead zone, then a squared response so the middle of the travel
 * is fine control and the edge is fast.  Counts are positive for a larger raw
 * value. */
static int axis_counts(int v, int center, int dz, int full, int max)
{
    const int d = v - center;
    const int mag = d < 0 ? -d : d;
    if (mag <= dz) {
        return 0;
    }
    const int span = full - dz;
    int over = mag - dz;
    if (over > span) {
        over = span;
    }
    const int counts = (max * over * over) / (span * span);
    return d < 0 ? -counts : counts;
}

/* Accept a sample as a center only when it is resting near the middle of the
 * part's encoding: a stick held over during `wiipad center`, or one reporting
 * garbage, must not become the new zero. */
static bool plausible_center(int v)
{
    return v > 88 && v < 168;
}

static void emit(tang_wii_keys_t *k, const char *s, size_t len)
{
    if (k == NULL || k->n + (int)len > TANG_WIIPAD_KEYS_MAX) {
        return;
    }
    memcpy(k->keys + k->n, s, len);
    k->n += (int)len;
}

/* The d-pad's four directions as the arrow keys the terminal sends. */
static const char *dpad_seq(uint32_t dir)
{
    switch (dir) {
    case TANG_WII_BTN_UP:    return "\x1b[A";
    case TANG_WII_BTN_DOWN:  return "\x1b[B";
    case TANG_WII_BTN_RIGHT: return "\x1b[C";
    case TANG_WII_BTN_LEFT:  return "\x1b[D";
    default:                 return NULL;
    }
}

/* The Classic's spare buttons as desktop input: Tab and Shift+Tab for focus,
 * Enter, Escape, the start menu, Page Up and Down, and closing a window.  A
 * press is one key; a d-pad direction repeats while it is held.  On the
 * desktop the d-pad moves the pointer instead, one cell a step, which is what
 * the D-pad of a game controller has always done here (tang_pad.c); at the
 * prompt it is the arrow keys, because there is no pointer to move. */
static void keys_from_buttons(tang_wiipad_t *n, const tang_wii_sample_t *s,
                              uint32_t now_ms, tang_wii_keys_t *k,
                              bool console_mode, tang_mouse_t *m)
{
    const uint32_t pressed = s->btn & ~n->btn_prev;

    if (pressed & TANG_WII_BTN_X)      emit(k, "\x1b", 1);
    if (pressed & TANG_WII_BTN_Y)      emit(k, "\t", 1);
    if (pressed & TANG_WII_BTN_ZL)     emit(k, "\x1b[Z", 3);
    if (pressed & TANG_WII_BTN_ZR)     emit(k, "\x11", 1);      /* Ctrl+Q */
    if (pressed & TANG_WII_BTN_PLUS)   emit(k, "\r", 1);
    if (pressed & TANG_WII_BTN_MINUS)  emit(k, "\x1b[21~", 5);  /* F10 */
    if (pressed & TANG_WII_BTN_L)      emit(k, "\x1b[5~", 4);
    if (pressed & TANG_WII_BTN_R)      emit(k, "\x1b[6~", 4);
    if ((pressed & TANG_WII_BTN_HOME) && k != NULL) {
        k->toggle = true;
    }

    const uint32_t dpad = s->btn & (TANG_WII_BTN_UP | TANG_WII_BTN_DOWN |
                                    TANG_WII_BTN_LEFT | TANG_WII_BTN_RIGHT);
    const uint32_t fresh = dpad & ~n->btn_prev;
    const uint32_t dir = fresh != 0 ? fresh : n->dpad_dir;
    const bool due = fresh != 0 ||
                     (dpad != 0 && n->dpad_dir != 0 &&
                      (int32_t)(now_ms - n->dpad_next_ms) >= 0);
    if (dpad == 0) {
        n->dpad_dir = 0;
    } else if (due) {
        if (console_mode) {
            const char *seq = dpad_seq(dir);
            if (seq != NULL) {
                emit(k, seq, 3);
            }
        } else {
            /* One cell for the press, then an accelerating step while held, in
             * counts (tang_pad.h counts 16 to a cell). */
            int cells = 1;
            if (fresh == 0) {
                const uint32_t held = now_ms - n->dpad_start_ms;
                cells = 1 + (int)(held / TANG_WIIPAD_RAMP_MS);
                if (cells > TANG_WIIPAD_MAX_CELLS) {
                    cells = TANG_WIIPAD_MAX_CELLS;
                }
            }
            const int step = TANG_MOUSE_COUNTS_PER_CELL * cells;
            if (dir & TANG_WII_BTN_UP)    m->dy -= step;
            if (dir & TANG_WII_BTN_DOWN)  m->dy += step;
            if (dir & TANG_WII_BTN_LEFT)  m->dx -= step;
            if (dir & TANG_WII_BTN_RIGHT) m->dx += step;
        }
        if (fresh != 0) {
            n->dpad_start_ms = now_ms;
        }
        n->dpad_dir = dir;
        n->dpad_next_ms = now_ms + (fresh != 0 ? TANG_WIIPAD_REPEAT_DELAY_MS
                                               : TANG_WIIPAD_REPEAT_MS);
    }

    n->btn_prev = s->btn;
}

void tang_wii_apply(tang_wiipad_t *n, const tang_wii_sample_t *s,
                    tang_mouse_t *m, bool console_mode, uint32_t now_ms,
                    tang_wii_keys_t *k)
{
    const bool classic = n->kind == TANG_WII_KIND_CLASSIC;

    if (!n->centered || n->relearn) {
        if (plausible_center(s->lx)) {
            n->cx = s->lx;
        }
        if (plausible_center(s->ly)) {
            n->cy = s->ly;
        }
        n->centered = true;
    }
    if (classic && (!n->rcentered || n->relearn)) {
        if (plausible_center(s->rx)) {
            n->crx = s->rx;
        }
        if (plausible_center(s->ry)) {
            n->cry = s->ry;
        }
        n->rcentered = true;
    }
    n->relearn = false;

    if (console_mode) {
        /* There is no pointer to move, so the left stick's vertical axis is
         * the wheel: pushing away walks back through the history, as wheel-up
         * does.  One detent per TANG_WIIPAD_WHEEL_MS, and the gate re-arms as
         * soon as the stick comes back to the middle.  The right stick is not
         * used here, so the two cannot fight over the history. */
        const int up = s->ly - n->cy;
        if (up > TANG_WIIPAD_DZ_8 || up < -TANG_WIIPAD_DZ_8) {
            if ((int32_t)(now_ms - n->next_wheel_ms) >= 0) {
                m->wheel += up > 0 ? 1 : -1;
                n->next_wheel_ms = now_ms + TANG_WIIPAD_WHEEL_MS;
            }
        } else {
            n->next_wheel_ms = now_ms;
        }
    } else {
        /* The part reports a larger Y for a push away from the user, while
         * the mouse counts +y downward (tang_pad.h), so Y is negated.  Both
         * sticks' X grow to the right.  The right stick adds at half the
         * counts. */
        int dx = axis_counts(s->lx, n->cx, TANG_WIIPAD_DZ_8, TANG_WIIPAD_FULL_8,
                             TANG_WIIPAD_MAX_COUNTS);
        int dy = -axis_counts(s->ly, n->cy, TANG_WIIPAD_DZ_8, TANG_WIIPAD_FULL_8,
                              TANG_WIIPAD_MAX_COUNTS);
        if (classic) {
            dx += axis_counts(s->rx, n->crx, TANG_WIIPAD_DZ_8,
                              TANG_WIIPAD_FULL_8, TANG_WIIPAD_RIGHT_COUNTS);
            dy -= axis_counts(s->ry, n->cry, TANG_WIIPAD_DZ_8,
                              TANG_WIIPAD_FULL_8, TANG_WIIPAD_RIGHT_COUNTS);
        }
        m->dx += dx;
        m->dy += dy;
    }

    uint8_t b = 0;
    if (s->left) {
        b |= TANG_MOUSE_LEFT;
    }
    if (s->right) {
        b |= TANG_MOUSE_RIGHT;
    }
    m->pressed |= (uint8_t)(b & (uint8_t)~n->buttons);
    m->buttons |= b;
    n->buttons = b;

    keys_from_buttons(n, s, now_ms, k, console_mode, m);
}

/* --------------------------------------------------------------- the part */

/* Read the identity: 1 a part this driver handles, 0 no answer, -1 a part it
 * does not. */
static int id_probe(tang_wii_kind_t *kind)
{
    uint8_t id[6];
    if (!tang_wiipad_hw_read(TANG_WIIPAD_REG_ID, id, sizeof id) ||
        tang_wiipad_all_ff(id, sizeof id)) {
        return 0;
    }
    *kind = tang_wii_identify(id);
    return *kind == TANG_WII_KIND_NONE ? -1 : 1;
}

/* Put a Classic in data format 3, the eight-byte report with 8-bit sticks.
 * A genuine Classic supports it, and a third-party receiver built for the
 * NES/SNES Classic sends nothing but a fixed block without it (WII-004). */
static bool enter_format3(void)
{
    const uint8_t v = TANG_WIIPAD_FORMAT3;
    return tang_wiipad_hw_write(TANG_WIIPAD_REG_FORMAT, &v, 1);
}

static void adopt(tang_wiipad_t *n, tang_wii_kind_t kind)
{
    n->kind = kind;
    n->state = TANG_WIIPAD_READY;
    n->awake = true;
    n->identified = true;
    n->centered = false;
    n->rcentered = false;
    n->relearn = false;
    default_centers(n);
}

bool tang_wiipad_poll(tang_mouse_t *m, bool console_mode, tang_wii_keys_t *k)
{
    const uint32_t now = tang_wiipad_hw_ms();

    if (!tang_wiipad_hw_ready()) {
        /* A core without the I2C engine, or a host-owned link.  Not an
         * error: the next poll after a core load tries again. */
        LOCK();
        s_pad.state = TANG_WIIPAD_OFF;
        s_pad.awake = false;
        UNLOCK();
        return false;
    }

    tang_wii_state_t st;
    bool awake;
    uint32_t due;
    LOCK();
    st = s_pad.state;
    awake = s_pad.awake;
    due = s_pad.next_try_ms;
    UNLOCK();

    if (st == TANG_WIIPAD_UNSUPPORTED) {
        /* Look again occasionally: a controller swapped under power must be
         * noticed without a reboot. */
        if ((int32_t)(now - due) < 0) {
            return false;
        }
        tang_wii_kind_t k;
        const int r = id_probe(&k);
        if (r == 1) {
            LOCK();
            adopt(&s_pad, k);
            s_pad.next_id_ms = now + TANG_WIIPAD_ID_MS;
            UNLOCK();
            if (k == TANG_WII_KIND_CLASSIC && !enter_format3()) {
                LOCK();
                s_pad.awake = false;
                s_pad.state = TANG_WIIPAD_SLEEPING;
                s_pad.next_try_ms = now + 2000u;
                UNLOCK();
            }
        } else {
            LOCK();
            s_pad.next_try_ms = now + 2000u;
            UNLOCK();
        }
        return false;
    }

    if (!awake) {
        if ((int32_t)(now - due) < 0) {
            return false;
        }
        const uint8_t v55 = 0x55u;
        const uint8_t v00 = 0x00u;
        const bool ok = tang_wiipad_hw_write(TANG_WIIPAD_REG_INIT, &v55, 1) &&
                        tang_wiipad_hw_write(TANG_WIIPAD_REG_ENC, &v00, 1);
        LOCK();
        s_pad.awake = ok;
        s_pad.identified = false;
        s_pad.state = TANG_WIIPAD_SLEEPING;
        /* A genuine part needs a moment after the wake-up before it reports;
         * the next poll is 40 ms later, which is plenty. */
        s_pad.next_try_ms = now + (ok ? 10u : 2000u);
        UNLOCK();
        return false;
    }

    /* Eight bytes whatever the part: a nunchuck answers with its six and the
     * register block carries on past them, and a Classic in format 3 needs
     * all eight. */
    uint8_t b[8];
    if (!tang_wiipad_hw_read(TANG_WIIPAD_REG_DATA, b, sizeof b) ||
        tang_wiipad_all_ff(b, sizeof b)) {
        /* No answer, or 0xff: the part has been replugged and needs its
         * wake-up again.  Retry rather than decide it is gone. */
        LOCK();
        s_pad.awake = false;
        s_pad.state = TANG_WIIPAD_SLEEPING;
        s_pad.next_try_ms = now + 2000u;
        UNLOCK();
        return false;
    }

    bool identified;
    LOCK();
    identified = s_pad.identified;
    UNLOCK();
    if (!identified) {
        tang_wii_kind_t k;
        const int r = id_probe(&k);
        if (r == 1) {
            LOCK();
            adopt(&s_pad, k);
            s_pad.next_id_ms = now + TANG_WIIPAD_ID_MS;
            UNLOCK();
            /* The report just read may have been in the other format, so it
             * is dropped; the next poll reads the one adopted. */
            if (k == TANG_WII_KIND_CLASSIC && !enter_format3()) {
                LOCK();
                s_pad.awake = false;
                s_pad.state = TANG_WIIPAD_SLEEPING;
                s_pad.next_try_ms = now + 2000u;
                UNLOCK();
            }
            return false;
        }
        if (r < 0) {
            LOCK();
            s_pad.identified = true;
            s_pad.state = TANG_WIIPAD_UNSUPPORTED;
            s_pad.next_try_ms = now + 2000u;
            UNLOCK();
            return false;
        }
        /* It reports data but will not give its identity; that is a part in a
         * state this driver does not understand, so wake it again rather than
         * guess which packing its bytes are in. */
        LOCK();
        s_pad.awake = false;
        s_pad.state = TANG_WIIPAD_SLEEPING;
        s_pad.next_try_ms = now + 500u;
        UNLOCK();
        return false;
    }

    uint32_t check;
    LOCK();
    check = s_pad.next_id_ms;
    UNLOCK();
    if ((int32_t)(now - check) >= 0) {
        tang_wii_kind_t k;
        const int r = id_probe(&k);
        if (r != 1) {
            LOCK();
            s_pad.awake = false;
            s_pad.state = TANG_WIIPAD_SLEEPING;
            s_pad.next_try_ms = now + (r < 0 ? 2000u : 500u);
            UNLOCK();
            return false;
        }
        bool changed;
        LOCK();
        changed = k != s_pad.kind;
        if (changed) {
            adopt(&s_pad, k);
        }
        s_pad.next_id_ms = now + TANG_WIIPAD_ID_MS;
        UNLOCK();
        if (changed) {
            if (k == TANG_WII_KIND_CLASSIC && !enter_format3()) {
                LOCK();
                s_pad.awake = false;
                s_pad.state = TANG_WIIPAD_SLEEPING;
                s_pad.next_try_ms = now + 2000u;
                UNLOCK();
            }
            return false;
        }
    }

    tang_wii_sample_t s;
    tang_wii_kind_t kind;
    LOCK();
    kind = s_pad.kind;
    UNLOCK();
    tang_wii_decode(kind, b, &s);

    LOCK();
    memcpy(s_pad.last, b, sizeof b);
    s_pad.have_last = true;
    s_pad.state = TANG_WIIPAD_READY;
    tang_wii_apply(&s_pad, &s, m, console_mode, now, k);
    UNLOCK();
    return true;
}

/* ------------------------------------------------------------- the shell */

#ifndef TANG_WIIPAD_HOST

static const char *kind_name(tang_wii_kind_t k)
{
    switch (k) {
    case TANG_WII_KIND_NUNCHUCK: return "nunchuck";
    case TANG_WII_KIND_CLASSIC:  return "classic";
    default:                     return "unknown";
    }
}

static const char *state_name(tang_wii_state_t s)
{
    switch (s) {
    case TANG_WIIPAD_READY:       return "ready";
    case TANG_WIIPAD_SLEEPING:    return "sleeping";
    case TANG_WIIPAD_UNSUPPORTED: return "unsupported";
    case TANG_WIIPAD_OFF:         /* fall through */
    default:                      return "off";
    }
}

static void print_report(tang_wii_kind_t kind, const uint8_t b[8])
{
    tang_wii_sample_t s;
    tang_wii_decode(kind, b, &s);
    if (kind == TANG_WII_KIND_CLASSIC) {
        tdsh_printf("wiipad: %02x %02x %02x %02x %02x %02x %02x %02x  left %d/%d right %d/%d  A %d B %d\r\n",
                    b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                    s.lx, s.ly, s.rx, s.ry, s.left ? 1 : 0, s.right ? 1 : 0);
    } else {
        tdsh_printf("wiipad: %02x %02x %02x %02x %02x %02x  stick %d/%d  z %d c %d\r\n",
                    b[0], b[1], b[2], b[3], b[4], b[5],
                    s.lx, s.ly, s.left ? 1 : 0, s.right ? 1 : 0);
    }
}

static const char *const s_btn_names[15] = {
    "A", "B", "X", "Y", "ZL", "ZR", "+", "-", "Home",
    "L", "R", "Up", "Down", "Left", "Right",
};

static void print_buttons(uint32_t btn)
{
    bool first = true;
    for (int i = 0; i < 15; i++) {
        if ((btn & (1u << i)) != 0) {
            tdsh_printf("%s%s", first ? "" : " ", s_btn_names[i]);
            first = false;
        }
    }
    if (first) {
        tdsh_printf("none");
    }
}

/* Watch the part and print a line whenever its report changes, so one run
 * with every button and both sticks produces the whole map.  It reads the
 * registers directly rather than through the driver, so it neither disturbs
 * the desk task's polling nor feeds the desktop input. */
static int cmd_trace(int seconds)
{
    if (seconds < 1) {
        seconds = 20;
    }
    if (seconds > 120) {
        seconds = 120;
    }

    tang_wiipad_t *n = tang_wiipad_state();
    tang_wii_kind_t kind;
    LOCK();
    kind = n->kind;
    UNLOCK();
    if (kind != TANG_WII_KIND_CLASSIC) {
        tdsh_printf("wiipad: trace needs a Classic in format 3\r\n");
        return 1;
    }

    uint8_t prev[8] = {0};
    bool have = false;
    int lines = 0;
    const uint32_t end = tang_wiipad_hw_ms() + (uint32_t)seconds * 1000u;
    tdsh_printf("wiipad: tracing %d s; press each button and move both sticks\r\n",
                seconds);

    while (lines < 60) {
        if ((int32_t)(tang_wiipad_hw_ms() - end) >= 0) {
            break;
        }
        uint8_t b[8];
        if (tang_wiipad_hw_read(TANG_WIIPAD_REG_DATA, b, sizeof b) &&
            !tang_wiipad_all_ff(b, sizeof b) &&
            (!have || memcmp(b, prev, sizeof b) != 0)) {
            tang_wii_sample_t s;
            tang_wii_decode(kind, b, &s);
            tdsh_printf("wiipad: %02x %02x %02x %02x %02x %02x %02x %02x  L%d/%d R%d/%d  ",
                        b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                        s.lx, s.ly, s.rx, s.ry);
            print_buttons(s.btn);
            tdsh_printf("\r\n");
            memcpy(prev, b, sizeof b);
            have = true;
            lines++;
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
    tdsh_printf("wiipad: trace done, %d changes\r\n", lines);
    return 0;
}

static int cmd_wiipad(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    tang_wiipad_t *n = tang_wiipad_state();
    const char *sub = argc > 1 ? argv[1] : "status";

    if (strcmp(sub, "raw") == 0) {
        tang_mouse_t scratch = {0};
        tang_wii_keys_t keys;
        memset(&keys, 0, sizeof keys);
        if (!tang_wiipad_poll(&scratch, false, &keys)) {
            tang_wii_state_t st;
            tang_wii_kind_t kind;
            LOCK();
            st = n->state;
            kind = n->kind;
            UNLOCK();
            if (st == TANG_WIIPAD_UNSUPPORTED) {
                tdsh_printf("wiipad: a Wii extension part is fitted but it is not a nunchuck or a Classic\r\n");
            } else if (st == TANG_WIIPAD_OFF) {
                tdsh_printf("wiipad: no I2C bus -- load a desktop core with the bus\r\n");
            } else {
                tdsh_printf("wiipad: waking; run it again\r\n");
            }
            (void)kind;
            return 1;
        }
        LOCK();
        uint8_t b[8];
        tang_wii_kind_t kind;
        memcpy(b, n->last, sizeof b);
        kind = n->kind;
        UNLOCK();
        print_report(kind, b);
        return 0;
    }

    if (strcmp(sub, "trace") == 0) {
        return cmd_trace(argc > 2 ? atoi(argv[2]) : 20);
    }

    if (strcmp(sub, "center") == 0) {
        LOCK();
        n->relearn = true;
        n->centered = false;
        n->rcentered = false;
        UNLOCK();
        tdsh_printf("wiipad: the next reading becomes the center -- leave both sticks at rest\r\n");
        return 0;
    }

    if (strcmp(sub, "status") != 0) {
        tdsh_printf("usage: wiipad status|raw|center|trace [seconds]\r\n");
        return 1;
    }

    LOCK();
    const tang_wii_state_t st = n->state;
    const tang_wii_kind_t kind = n->kind;
    const bool centered = n->centered;
    const int cx = n->cx, cy = n->cy;
    const int crx = n->crx, cry = n->cry;
    const bool have = n->have_last;
    uint8_t b[8];
    memcpy(b, n->last, sizeof b);
    UNLOCK();

    tdsh_printf("wiipad: %s %s%s\r\n", kind_name(kind), state_name(st),
                st == TANG_WIIPAD_READY ? "" : " -- not pointing");
    if (kind == TANG_WII_KIND_CLASSIC) {
        tdsh_printf("wiipad: center left %d/%d right %d/%d%s\r\n", cx, cy, crx, cry,
                    centered ? "" : " (not learned yet)");
    } else {
        tdsh_printf("wiipad: center %d/%d%s\r\n", cx, cy,
                    centered ? "" : " (not learned yet)");
    }
    if (have) {
        print_report(kind, b);
    }
    return 0;
}

static const tdsh_command_t s_wiipad_commands[] = {
    { "wiipad", "wiipad status|raw|center|trace [s]",
      "Wii nunchuck or Classic Controller as the pointer", cmd_wiipad, 0 },
    /* The command was called `nunchuck` when that was the only part, and it
     * is kept so scripts and habits do not break. */
    { "nunchuck", "nunchuck status|raw|center",
      "Alias for wiipad", cmd_wiipad, 0 },
};

int tang_wiipad_register(void)
{
    return tdsh_register_commands(s_wiipad_commands,
                                  sizeof(s_wiipad_commands) / sizeof(s_wiipad_commands[0]));
}

#endif /* !TANG_WIIPAD_HOST */
