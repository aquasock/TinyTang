/* TinyTang — Wii extension controllers as the desktop's pointer.
 * See tang_wiipad.h for what it is and why it is shaped this way.
 *
 * SPDX-License-Identifier: MIT
 */
#include "tang_wiipad.h"

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
    if (n->kind == TANG_WII_KIND_CLASSIC) {
        n->cx = 32;
        n->cy = 32;
        n->crx = 16;
        n->cry = 16;
    } else {
        n->cx = 128;
        n->cy = 128;
        n->crx = 16;
        n->cry = 16;
    }
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

void tang_wii_decode(tang_wii_kind_t kind, const uint8_t b[6],
                     tang_wii_sample_t *s)
{
    memset(s, 0, sizeof *s);

    if (kind == TANG_WII_KIND_CLASSIC) {
        /* Format 0x01 (WII-003): the left stick's two 6-bit axes are the low
         * six bits of bytes 0 and 1; the right stick's five bits are split
         * across the top two bits of those bytes and the top bit of byte 2;
         * the analog triggers occupy the rest of bytes 2 and 3. */
        const int lt = ((b[2] >> 5) & 0x03) << 3 | ((b[3] >> 5) & 0x07);
        const int rt = b[3] & 0x1F;
        s->lx = b[0] & 0x3F;
        s->ly = b[1] & 0x3F;
        s->rx = ((b[0] >> 6) & 0x03) << 3 | ((b[1] >> 6) & 0x03) << 1 |
                ((b[2] >> 7) & 0x01);
        s->ry = b[2] & 0x1F;
        /* Byte 4 carries the two shoulders as their triggers' end-of-travel
         * click; a firm pull counts even when that click has not closed. */
        s->left = (b[4] & 0x20u) == 0 || lt >= TANG_WIIPAD_TRIGGER_CLICK;
        s->right = (b[4] & 0x02u) == 0 || rt >= TANG_WIIPAD_TRIGGER_CLICK;
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
     * that shares its identity prefix. */
    if (id[4] == 0x00u && id[5] == 0x00u) {
        return TANG_WII_KIND_NUNCHUCK;
    }
    if (id[4] == 0x01u && id[5] == 0x01u) {
        return TANG_WII_KIND_CLASSIC;
    }
    return TANG_WII_KIND_NONE;
}

bool tang_wiipad_all_ff(const uint8_t b[6])
{
    for (int i = 0; i < 6; i++) {
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
static bool plausible_center(int v, tang_wii_kind_t kind)
{
    if (kind == TANG_WII_KIND_CLASSIC) {
        return v >= 16 && v <= 48;
    }
    return v > 88 && v < 168;
}

static bool plausible_right_center(int v)
{
    return v >= 8 && v <= 24;
}

void tang_wii_apply(tang_wiipad_t *n, const tang_wii_sample_t *s,
                    tang_mouse_t *m, bool console_mode, uint32_t now_ms)
{
    const bool classic = n->kind == TANG_WII_KIND_CLASSIC;
    const int dz = classic ? TANG_WIIPAD_DZ_6 : TANG_WIIPAD_DZ_8;
    const int full = classic ? TANG_WIIPAD_FULL_6 : TANG_WIIPAD_FULL_8;

    if (!n->centered || n->relearn) {
        if (plausible_center(s->lx, n->kind)) {
            n->cx = s->lx;
        }
        if (plausible_center(s->ly, n->kind)) {
            n->cy = s->ly;
        }
        n->centered = true;
    }
    if (classic && (!n->rcentered || n->relearn)) {
        if (plausible_right_center(s->rx)) {
            n->crx = s->rx;
        }
        if (plausible_right_center(s->ry)) {
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
        if (up > dz || up < -dz) {
            if ((int32_t)(now_ms - n->next_wheel_ms) >= 0) {
                m->wheel += up > 0 ? 1 : -1;
                n->next_wheel_ms = now_ms + TANG_WIIPAD_WHEEL_MS;
            }
        } else {
            n->next_wheel_ms = now_ms;
        }
    } else {
        /* The part reports a larger Y for a push away from the user, while
         * the mouse counts +y downward (tang_pad.h), so Y is negated.  The
         * right stick adds at half the counts, and its X grows to the left,
         * so that one is negated too. */
        int dx = axis_counts(s->lx, n->cx, dz, full, TANG_WIIPAD_MAX_COUNTS);
        int dy = -axis_counts(s->ly, n->cy, dz, full, TANG_WIIPAD_MAX_COUNTS);
        if (classic) {
            dx -= axis_counts(s->rx, n->crx, TANG_WIIPAD_DZ_5,
                              TANG_WIIPAD_FULL_5, TANG_WIIPAD_RIGHT_COUNTS);
            dy -= axis_counts(s->ry, n->cry, TANG_WIIPAD_DZ_5,
                              TANG_WIIPAD_FULL_5, TANG_WIIPAD_RIGHT_COUNTS);
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
}

/* --------------------------------------------------------------- the part */

/* Read the identity: 1 a part this driver handles, 0 no answer, -1 a part it
 * does not. */
static int id_probe(tang_wii_kind_t *kind)
{
    uint8_t id[6];
    if (!tang_wiipad_hw_read(TANG_WIIPAD_REG_ID, id, sizeof id) ||
        tang_wiipad_all_ff(id)) {
        return 0;
    }
    *kind = tang_wii_identify(id);
    return *kind == TANG_WII_KIND_NONE ? -1 : 1;
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

bool tang_wiipad_poll(tang_mouse_t *m, bool console_mode)
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

    uint8_t b[6];
    if (!tang_wiipad_hw_read(TANG_WIIPAD_REG_DATA, b, sizeof b) ||
        tang_wiipad_all_ff(b)) {
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
        } else if (r < 0) {
            LOCK();
            s_pad.identified = true;
            s_pad.state = TANG_WIIPAD_UNSUPPORTED;
            s_pad.next_try_ms = now + 2000u;
            UNLOCK();
            return false;
        } else {
            /* It reports data but will not give its identity; that is a part
             * in a state this driver does not understand, so wake it again
             * rather than guess which packing its bytes are in. */
            LOCK();
            s_pad.awake = false;
            s_pad.state = TANG_WIIPAD_SLEEPING;
            s_pad.next_try_ms = now + 500u;
            UNLOCK();
            return false;
        }
    } else {
        uint32_t check;
        LOCK();
        check = s_pad.next_id_ms;
        UNLOCK();
        if ((int32_t)(now - check) >= 0) {
            tang_wii_kind_t k;
            const int r = id_probe(&k);
            if (r == 1) {
                LOCK();
                if (k != s_pad.kind) {
                    adopt(&s_pad, k);
                }
                s_pad.next_id_ms = now + TANG_WIIPAD_ID_MS;
                UNLOCK();
            } else {
                LOCK();
                s_pad.awake = false;
                s_pad.state = TANG_WIIPAD_SLEEPING;
                s_pad.next_try_ms = now + (r < 0 ? 2000u : 500u);
                UNLOCK();
                return false;
            }
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
    tang_wii_apply(&s_pad, &s, m, console_mode, now);
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

static void print_report(tang_wii_kind_t kind, const uint8_t b[6])
{
    tang_wii_sample_t s;
    tang_wii_decode(kind, b, &s);
    if (kind == TANG_WII_KIND_CLASSIC) {
        tdsh_printf("wiipad: %02x %02x %02x %02x %02x %02x  left %d/%d right %d/%d  L %d R %d\r\n",
                    b[0], b[1], b[2], b[3], b[4], b[5],
                    s.lx, s.ly, s.rx, s.ry, s.left ? 1 : 0, s.right ? 1 : 0);
    } else {
        tdsh_printf("wiipad: %02x %02x %02x %02x %02x %02x  stick %d/%d  z %d c %d\r\n",
                    b[0], b[1], b[2], b[3], b[4], b[5],
                    s.lx, s.ly, s.left ? 1 : 0, s.right ? 1 : 0);
    }
}

static int cmd_wiipad(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    tang_wiipad_t *n = tang_wiipad_state();
    const char *sub = argc > 1 ? argv[1] : "status";

    if (strcmp(sub, "raw") == 0) {
        tang_mouse_t scratch = {0};
        if (!tang_wiipad_poll(&scratch, false)) {
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
        uint8_t b[6];
        tang_wii_kind_t kind;
        memcpy(b, n->last, sizeof b);
        kind = n->kind;
        UNLOCK();
        print_report(kind, b);
        return 0;
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
        tdsh_printf("usage: wiipad status|raw|center\r\n");
        return 1;
    }

    LOCK();
    const tang_wii_state_t st = n->state;
    const tang_wii_kind_t kind = n->kind;
    const bool centered = n->centered;
    const int cx = n->cx, cy = n->cy;
    const int crx = n->crx, cry = n->cry;
    const bool have = n->have_last;
    uint8_t b[6];
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
    { "wiipad", "wiipad status|raw|center",
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
