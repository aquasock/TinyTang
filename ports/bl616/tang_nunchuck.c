/* TinyTang — a Wii nunchuck as the desktop's pointer.
 * See tang_nunchuck.h for what it is and why it is shaped this way.
 *
 * SPDX-License-Identifier: MIT
 */
#include "tang_nunchuck.h"

#include <string.h>

#ifdef TANG_NUNCHUCK_HOST
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

static tang_nunchuck_t s_nunchuck = {
    .state = TANG_NUNCHUCK_OFF,
    .cx = 128,
    .cy = 128,
};

void tang_nunchuck_reset(void)
{
    LOCK();
    const int cx = s_nunchuck.cx;
    const int cy = s_nunchuck.cy;
    memset(&s_nunchuck, 0, sizeof s_nunchuck);
    s_nunchuck.state = TANG_NUNCHUCK_OFF;
    /* The center survives: it describes the part, not the session. */
    s_nunchuck.cx = cx;
    s_nunchuck.cy = cy;
    UNLOCK();
}

tang_nunchuck_t *tang_nunchuck_state(void)
{
    return &s_nunchuck;
}

/* ------------------------------------------------------------- the pieces */

void tang_nunchuck_decode(const uint8_t b[6], tang_nunchuck_raw_t *r)
{
    r->sx = b[0];
    r->sy = b[1];
    /* Both buttons are active low, and they are the only two bits of the
     * last byte that are not accelerometer LSBs. */
    r->z = (b[5] & 0x01u) == 0;
    r->c = (b[5] & 0x02u) == 0;
}

/* The identity block is 00 00 A4 20 00 00 for a nunchuck; the fifth byte is
 * the model, 0x01 being a Classic Controller and its Pro (WII-001). */
bool tang_nunchuck_is_nunchuck(const uint8_t id[6])
{
    return id[2] == 0xA4u && id[3] == 0x20u && id[4] == 0x00u;
}

bool tang_nunchuck_all_ff(const uint8_t b[6])
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
static int axis_counts(int v, int center)
{
    int d = v - center;
    const int mag = d < 0 ? -d : d;
    if (mag <= TANG_NUNCHUCK_DEADZONE) {
        return 0;
    }
    const int span = TANG_NUNCHUCK_FULL - TANG_NUNCHUCK_DEADZONE;
    int over = mag - TANG_NUNCHUCK_DEADZONE;
    if (over > span) {
        over = span;
    }
    int counts = (TANG_NUNCHUCK_MAX_COUNTS * over * over) / (span * span);
    return d < 0 ? -counts : counts;
}

/* Accept a sample as the stick's center only when it is resting somewhere
 * near the middle: a stick held over during `nunchuck center`, or one
 * reporting garbage, must not become the new zero. */
static bool plausible_center(int v)
{
    return v > 88 && v < 168;
}

void tang_nunchuck_apply(tang_nunchuck_t *n, const tang_nunchuck_raw_t *r,
                         tang_mouse_t *m, bool console_mode, uint32_t now_ms)
{
    if (!n->centered || n->relearn) {
        if (plausible_center(r->sx)) {
            n->cx = r->sx;
        }
        if (plausible_center(r->sy)) {
            n->cy = r->sy;
        }
        n->centered = true;
        n->relearn = false;
    }

    if (console_mode) {
        /* There is no pointer to move, so the vertical axis is the wheel:
         * pushing away walks back through the history, as wheel-up does.  One
         * detent per TANG_NUNCHUCK_WHEEL_MS, and the gate re-arms as soon as
         * the stick comes back to the middle, so separate pushes each give a
         * detent immediately. */
        const int up = r->sy - n->cy;
        if (up > TANG_NUNCHUCK_DEADZONE || up < -TANG_NUNCHUCK_DEADZONE) {
            if ((int32_t)(now_ms - n->next_wheel_ms) >= 0) {
                m->wheel += up > 0 ? 1 : -1;
                n->next_wheel_ms = now_ms + TANG_NUNCHUCK_WHEEL_MS;
            }
        } else {
            n->next_wheel_ms = now_ms;
        }
    } else {
        m->dx += axis_counts(r->sx, n->cx);
        /* The part reports a larger Y for a push away from the user, while
         * the mouse counts +y downward (tang_pad.h). */
        m->dy -= axis_counts(r->sy, n->cy);
    }

    uint8_t b = 0;
    if (r->z) {
        b |= TANG_MOUSE_LEFT;
    }
    if (r->c) {
        b |= TANG_MOUSE_RIGHT;
    }
    m->pressed |= (uint8_t)(b & (uint8_t)~n->buttons);
    m->buttons |= b;
    n->buttons = b;
}

/* --------------------------------------------------------------- the part */

bool tang_nunchuck_poll(tang_mouse_t *m, bool console_mode)
{
    const uint32_t now = tang_nunchuck_hw_ms();

    if (!tang_nunchuck_hw_ready()) {
        /* A core without the I2C engine, or a host-owned link.  Not an
         * error: the next poll after a core load tries again. */
        LOCK();
        s_nunchuck.state = TANG_NUNCHUCK_OFF;
        s_nunchuck.awake = false;
        UNLOCK();
        return false;
    }

    tang_nunchuck_state_t st;
    bool awake;
    uint32_t due;
    LOCK();
    st = s_nunchuck.state;
    awake = s_nunchuck.awake;
    due = s_nunchuck.next_try_ms;
    UNLOCK();

    if (st == TANG_NUNCHUCK_UNSUPPORTED) {
        return false;
    }

    if (!awake) {
        if ((int32_t)(now - due) < 0) {
            return false;
        }
        const uint8_t v55 = 0x55u;
        const uint8_t v00 = 0x00u;
        const bool ok = tang_nunchuck_hw_write(TANG_NUNCHUCK_REG_INIT, &v55, 1) &&
                        tang_nunchuck_hw_write(TANG_NUNCHUCK_REG_ENC, &v00, 1);
        LOCK();
        s_nunchuck.awake = ok;
        s_nunchuck.identified = false;
        s_nunchuck.state = TANG_NUNCHUCK_SLEEPING;
        /* A genuine part needs a moment after the wake-up before it reports;
         * the next poll is 40 ms later, which is plenty. */
        s_nunchuck.next_try_ms = now + (ok ? 10u : 2000u);
        UNLOCK();
        return false;
    }

    uint8_t b[6];
    if (!tang_nunchuck_hw_read(TANG_NUNCHUCK_REG_DATA, b, sizeof b) ||
        tang_nunchuck_all_ff(b)) {
        /* No answer, or 0xff: the part has been replugged and needs its
         * wake-up again.  Retry rather than decide it is gone. */
        LOCK();
        s_nunchuck.awake = false;
        s_nunchuck.state = TANG_NUNCHUCK_SLEEPING;
        s_nunchuck.next_try_ms = now + 2000u;
        UNLOCK();
        return false;
    }

    bool identified;
    LOCK();
    identified = s_nunchuck.identified;
    UNLOCK();
    if (!identified) {
        uint8_t id[6];
        const bool has_id = tang_nunchuck_hw_read(TANG_NUNCHUCK_REG_ID, id, sizeof id) &&
                            !tang_nunchuck_all_ff(id);
        if (has_id && !tang_nunchuck_is_nunchuck(id)) {
            /* A Classic Controller and its Pro report a different format
             * (WII-003): the stick would read as hard over and the d-pad as
             * the buttons.  Refuse rather than point with garbage. */
            LOCK();
            s_nunchuck.identified = true;
            s_nunchuck.state = TANG_NUNCHUCK_UNSUPPORTED;
            UNLOCK();
            return false;
        }
        /* Without a readable identity the data format is still the nunchuck's,
         * so the part is accepted; only a positive foreign identity refuses. */
        LOCK();
        s_nunchuck.identified = true;
        s_nunchuck.state = TANG_NUNCHUCK_READY;
        UNLOCK();
    }

    tang_nunchuck_raw_t r;
    tang_nunchuck_decode(b, &r);

    LOCK();
    memcpy(s_nunchuck.last, b, sizeof b);
    s_nunchuck.have_last = true;
    s_nunchuck.state = TANG_NUNCHUCK_READY;
    tang_nunchuck_apply(&s_nunchuck, &r, m, console_mode, now);
    UNLOCK();
    return true;
}

/* ------------------------------------------------------------- the shell */

#ifndef TANG_NUNCHUCK_HOST

static const char *state_name(tang_nunchuck_state_t s)
{
    switch (s) {
    case TANG_NUNCHUCK_READY:       return "ready";
    case TANG_NUNCHUCK_SLEEPING:    return "sleeping";
    case TANG_NUNCHUCK_UNSUPPORTED: return "unsupported";
    case TANG_NUNCHUCK_OFF:         /* fall through */
    default:                        return "off";
    }
}

static void print_report(const uint8_t b[6])
{
    tang_nunchuck_raw_t r;
    tang_nunchuck_decode(b, &r);
    tdsh_printf("nunchuck: %02x %02x %02x %02x %02x %02x  stick %d/%d  z %d c %d\r\n",
                b[0], b[1], b[2], b[3], b[4], b[5],
                r.sx, r.sy, r.z ? 1 : 0, r.c ? 1 : 0);
}

static int cmd_nunchuck(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    tang_nunchuck_t *n = tang_nunchuck_state();
    const char *sub = argc > 1 ? argv[1] : "status";

    if (strcmp(sub, "raw") == 0) {
        tang_mouse_t scratch = {0};
        if (!tang_nunchuck_poll(&scratch, false)) {
            tang_nunchuck_state_t st;
            LOCK();
            st = n->state;
            UNLOCK();
            if (st == TANG_NUNCHUCK_UNSUPPORTED) {
                tdsh_printf("nunchuck: a Wii extension part is fitted but it is not a nunchuck\r\n");
            } else if (st == TANG_NUNCHUCK_OFF) {
                tdsh_printf("nunchuck: no I2C bus -- load a desktop core with the bus\r\n");
            } else {
                tdsh_printf("nunchuck: waking; run it again\r\n");
            }
            return 1;
        }
        LOCK();
        uint8_t b[6];
        memcpy(b, n->last, sizeof b);
        UNLOCK();
        print_report(b);
        return 0;
    }

    if (strcmp(sub, "center") == 0) {
        LOCK();
        n->relearn = true;
        n->centered = false;
        UNLOCK();
        tdsh_printf("nunchuck: the next reading becomes the center -- leave the stick at rest\r\n");
        return 0;
    }

    if (strcmp(sub, "status") != 0) {
        tdsh_printf("usage: nunchuck status|raw|center\r\n");
        return 1;
    }

    LOCK();
    const tang_nunchuck_state_t st = n->state;
    const bool centered = n->centered;
    const int cx = n->cx, cy = n->cy;
    const bool have = n->have_last;
    uint8_t b[6];
    memcpy(b, n->last, sizeof b);
    UNLOCK();

    tdsh_printf("nunchuck: %s%s\r\n", state_name(st),
                st == TANG_NUNCHUCK_READY ? "" : " -- not pointing");
    tdsh_printf("nunchuck: center %d/%d%s\r\n", cx, cy,
                centered ? "" : " (not learned yet)");
    if (have) {
        print_report(b);
    }
    return 0;
}

static const tdsh_command_t s_nunchuck_commands[] = {
    { "nunchuck", "nunchuck status|raw|center",
      "Wii nunchuck on the USB-A port as the pointer", cmd_nunchuck, 0 },
};

int tang_nunchuck_register(void)
{
    return tdsh_register_commands(s_nunchuck_commands,
                                  sizeof(s_nunchuck_commands) / sizeof(s_nunchuck_commands[0]));
}

#endif /* !TANG_NUNCHUCK_HOST */
