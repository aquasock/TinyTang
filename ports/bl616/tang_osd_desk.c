// TinyTang — the desktop layer, driven from the console tap.
//
// See tang_osd_desk.h for what this is and why it is shaped this way.  This
// file is the mechanism: a terminal emulator sized to the layer, a shadow of
// what the core has been told, a diff, and a task that sends it.

#include "tang_osd_desk.h"
#include "tang_osd.h"
#include "tang_fpga_link.h"
#include "tang_pad.h"

/* The console writer matches this string byte for byte to keep the query off
 * the wire, so its length is part of the contract, not a detail. */
_Static_assert(sizeof(TANG_DESK_SIZE_QUERY) - 1 == TANG_DESK_SIZE_QUERY_LEN,
               "TANG_DESK_SIZE_QUERY_LEN must match TANG_DESK_SIZE_QUERY");

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "tinydesk/td_config.h"
#include "tinydesk/td_screen.h"
#include "tinydesk/td_vterm.h"

#include "tdsh.h"

int tdsh_printf(const char *fmt, ...);

/* How often the diff runs.  The desktop repaints its clock once a second and
 * the pointer on every move; 40 ms is well inside what a person notices and
 * keeps the link idle between changes. */
#define DESK_POLL_MS 40

/* How many polls to keep re-arming the layer after the FPGA has been
 * reprogrammed.  The core needs a moment before it will answer, and nothing
 * acknowledges the enable, so it is sent a few times rather than assumed. */
#define DESK_REARM_POLLS 8

/* Bytes for the desktop, waiting for the HAL to ask for them.  Two things
 * write here: the emulator, when the desktop asks a question the terminal is
 * meant to answer, and the pad, when the pointer moves.  The desktop cannot
 * tell them apart and does not need to -- to it, it is all just input from
 * whatever terminal it thinks it is talking to. */
#define DESK_IN_MAX 192

static char         s_in[DESK_IN_MAX];
static volatile int s_in_head, s_in_tail;
static tang_pad_t   s_pad;

static td_vterm_t  s_vt;
static td_vcell_t  s_shadow[TANG_DESK_ROWS][TANG_DESK_COLS];

static bool        s_enabled;
static bool        s_task_started;
static int         s_rearm;          /* polls left to re-arm after a reload */
static unsigned    s_cells;
static unsigned    s_rows;
static unsigned    s_dropped;

static uint8_t     s_payload[TANG_DESK_COLS * 5];

/* ------------------------------------------------------------- the palette */

/* The emulator keeps the desktop's 256-colour palette indexes; the core wants
 * 15-bit BGR5, the encoding the OSD already uses.  This is the standard xterm
 * palette: sixteen system colours, a 6x6x6 cube whose levels are 0, 95, 135,
 * 175, 215 and 255, and twenty-four greys. */
static uint16_t s_pal[256];

static uint16_t bgr5(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)((((uint16_t)b >> 3) << 10) |
                      (((uint16_t)g >> 3) << 5)  |
                       ((uint16_t)r >> 3));
}

static uint8_t cube6(int i)
{
    static const uint8_t steps[6] = { 0, 95, 135, 175, 215, 255 };
    return steps[i];
}

static void palette_init(void)
{
    static const uint8_t sys[16][3] = {
        {  0,   0,   0}, {205,   0,   0}, {  0, 205,   0}, {205, 205,   0},
        {  0,   0, 238}, {205,   0, 205}, {  0, 205, 205}, {229, 229, 229},
        {127, 127, 127}, {255,   0,   0}, {  0, 255,   0}, {255, 255,   0},
        { 92,  92, 255}, {255,   0, 255}, {  0, 255, 255}, {255, 255, 255},
    };

    for (int i = 0; i < 16; i++) {
        s_pal[i] = bgr5(sys[i][0], sys[i][1], sys[i][2]);
    }
    for (int i = 0; i < 216; i++) {
        s_pal[16 + i] = bgr5(cube6(i / 36), cube6((i / 6) % 6), cube6(i % 6));
    }
    for (int i = 0; i < 24; i++) {
        const uint8_t v = (uint8_t)(8 + i * 10);
        s_pal[232 + i] = bgr5(v, v, v);
    }
}

/* ------------------------------------------------- the emulator's answers */

static void desk_in_push(const char *data, int len)
{
    taskENTER_CRITICAL();
    for (int i = 0; i < len; i++) {
        const int next = (s_in_tail + 1) % DESK_IN_MAX;
        if (next == s_in_head) {
            break;              /* full: dropped, never blocked */
        }
        s_in[s_in_tail] = data[i];
        s_in_tail = next;
    }
    taskEXIT_CRITICAL();
}

static void desk_reply(void *user, const char *data, int len)
{
    (void)user;
    desk_in_push(data, len);
}

int tang_osd_desk_read_byte(void)
{
    if (!s_enabled) {
        return -1;
    }
    int b = -1;
    taskENTER_CRITICAL();
    if (s_in_head != s_in_tail) {
        b = (int)(unsigned char)s_in[s_in_head];
        s_in_head = (s_in_head + 1) % DESK_IN_MAX;
    }
    taskEXIT_CRITICAL();
    return b;
}

/* ------------------------------------------------------------ the sending */

static bool send_cursor(uint8_t x, uint8_t y)
{
    const uint8_t p[2] = { x, y };
    return tang_fpga_frame(FPGA_CMD_DESK_CURSOR, p, sizeof(p)) == 0;
}

static bool send_cells(const uint8_t *cells, size_t count)
{
    if (tang_fpga_frame(FPGA_CMD_DESK_CELLS, cells, count * 5) != 0) {
        return false;
    }
    s_cells += (unsigned)count;
    return true;
}

/* The cell as the core should show it: the emulator's, with the pointer drawn
 * on top.
 *
 * Nothing on this board draws a mouse pointer, and tinydesk does not either --
 * because a real terminal does.  Over the OSD there is no terminal, so without
 * this the pointer would be invisible and the controller would be unusable:
 * you would be waving at a screen with no way to see where.  The layer draws
 * it, by showing the cell under the pointer reversed, which is the one
 * highlight a text screen has.
 *
 * The shadow keeps whatever was sent, pointer included, so moving off a cell
 * leaves the shadow disagreeing with the emulator and the next poll puts the
 * real cell back.  Nothing has to remember where the pointer used to be. */
static td_vcell_t pointer_cell(const td_vcell_t c, int x, int y)
{
    if (x != s_pad.x || y != s_pad.y) {
        return c;
    }
    td_vcell_t p = c;
    if (c.fg == c.bg) {
        /* Inverting an invisible cell would keep it invisible. */
        p.fg = 0;
        p.bg = 7;
    } else {
        p.fg = c.bg;
        p.bg = c.fg;
    }
    return p;
}

/* Compare one row against the shadow and send the span that differs.
 *
 * The shadow is updated while the row is copied, inside the critical section,
 * and the frame goes out after it.  That ordering is what makes a console
 * write landing during the send safe: it changes the emulator and not the
 * shadow, so the next poll finds the difference and sends it.  Updating the
 * shadow after the send instead would drop those cells silently. */
static bool flush_row(int y)
{
    int first = -1, last = -1;

    taskENTER_CRITICAL();
    for (int x = 0; x < TANG_DESK_COLS; x++) {
        const td_vcell_t c = pointer_cell(s_vt.cells[y * TD_VT_MAX_COLS + x], x, y);
        const td_vcell_t s = s_shadow[y][x];
        if (c.ch != s.ch || c.fg != s.fg || c.bg != s.bg) {
            if (first < 0) {
                first = x;
            }
            last = x;
        }
    }

    int n = 0;
    if (first >= 0) {
        for (int x = first; x <= last; x++) {
            const td_vcell_t c = pointer_cell(s_vt.cells[y * TD_VT_MAX_COLS + x], x, y);
            const uint16_t f = s_pal[c.fg];
            const uint16_t b = s_pal[c.bg];

            /* The core's font is ASCII (FONT[0:127]) and the desktop draws its
             * window frames with box-drawing glyphs.  Mapping here, rather than
             * switching the desktop into ASCII mode, keeps the console's proper
             * Unicode boxes while the layer gets the + - | # the font actually
             * has.  Anything with no ASCII form becomes '?', which is visibly
             * wrong rather than silently wrong -- masking a 16-bit code point
             * to seven bits turns U+2554 into 'T' and a frame into nonsense.
             * The mapping is tinydesk's own, so the two cannot disagree. */
            uint32_t ch = c.ch;
            if (ch >= 0x80u) {
                ch = td_ascii_fallback(ch);
            }

            s_payload[n++] = (uint8_t)(ch & 0x7Fu);
            s_payload[n++] = (uint8_t)(f >> 8);
            s_payload[n++] = (uint8_t)(f & 0xFFu);
            s_payload[n++] = (uint8_t)(b >> 8);
            s_payload[n++] = (uint8_t)(b & 0xFFu);
            s_shadow[y][x] = c;
        }
    }
    taskEXIT_CRITICAL();

    if (first < 0) {
        return true;
    }
    if (!send_cursor((uint8_t)first, (uint8_t)y)) {
        return false;
    }
    return send_cells(s_payload, (size_t)(last - first + 1));
}

static void flush(void)
{
    if (tang_fpga_link_open() != 0) {
        return;
    }
    tang_fpga_lock();
    for (int y = 0; y < TANG_DESK_ROWS; y++) {
        if (!flush_row(y)) {
            /* The link refused part of a row.  The shadow for that row is
             * already updated, so invalidate everything: the next poll
             * repaints from scratch rather than leaving the core and the
             * emulator quietly disagreeing. */
            for (int yy = 0; yy < TANG_DESK_ROWS; yy++) {
                for (int xx = 0; xx < TANG_DESK_COLS; xx++) {
                    s_shadow[yy][xx].ch = 0xFFFFu;
                }
            }
            s_dropped++;
            break;
        }
        s_rows++;
    }
    tang_fpga_unlock();
}

/* The pad, once per poll.
 *
 * L is the session switch.  It does not stop the layer or the desktop: it
 * flips the overlay, which is the same byte the cartridge loader clears to
 * hand the screen to the game.  So the desktop keeps running and its grid
 * stays in the core, and coming back is a frame rather than a repaint.  The
 * state is read back from the OSD module rather than tracked here, so it
 * cannot disagree with what the loader did. */
/* The core was reprogrammed underneath us.
 *
 * `tangload` reconfigures the FPGA, which wipes everything the layer had put
 * in it: the enable bit goes back to 0 and the cell store goes blank.  Nothing
 * else would ever notice -- the shadow still matches the emulator, so the diff
 * would send nothing at all and the desktop would stay gone until somebody ran
 * `osd desk on` again.  That is exactly what launching a cartridge from the
 * desktop does, so without this the feature destroys itself the first time it
 * is used.
 *
 * The re-arm is deferred and repeated rather than done here: tangload returns
 * as soon as configuration is done, and the core needs a moment before it will
 * answer. */
void tang_osd_desk_core_reloaded(void)
{
    if (s_enabled) {
        s_rearm = DESK_REARM_POLLS;
    }
}

static void desk_poll(void)
{
    /* Re-arm first: until the core knows the layer is on, nothing sent is
     * shown. */
    if (s_rearm > 0) {
        s_rearm--;
        const uint8_t p[1] = { 1 };
        if (tang_fpga_link_open() == 0) {
            tang_fpga_lock();
            (void)tang_fpga_frame(FPGA_CMD_DESK_CTRL, p, sizeof(p));
            tang_fpga_unlock();
        }
        (void)tang_osd_set(true);
        /* Whatever the core holds is unknown, so everything goes again. */
        for (int y = 0; y < TANG_DESK_ROWS; y++) {
            for (int x = 0; x < TANG_DESK_COLS; x++) {
                s_shadow[y][x].ch = 0xFFFFu;
            }
        }
    }

    uint16_t joy1 = 0, joy2 = 0;
    tang_fpga_joypad(&joy1, &joy2);

    /* L is read whatever is on screen, because switching back is exactly the
     * case where the desktop is hidden. */
    const bool toggle =
        ((uint16_t)(joy1 & (uint16_t)~s_pad.prev) & TANG_PAD_LB) != 0;

    if (tang_osd_shown()) {
        char seq[96];
        bool ignored = false;
        const int n = tang_pad_step(&s_pad, joy1, seq, sizeof(seq), &ignored);
        if (n > 0) {
            desk_in_push(seq, n);
        }
    } else {
        /* While the desktop is hidden the pad belongs to the game.  Tracking it
         * keeps L's edge from being missed, but a pointer nobody can see should
         * not wander across the screen while somebody is playing -- coming back
         * to find it somewhere else is a small thing that reads as a fault. */
        s_pad.prev = joy1;
    }

    if (toggle) {
        (void)tang_osd_set(!tang_osd_shown());
    }
}

static void desk_task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(DESK_POLL_MS));
        if (s_enabled) {
            desk_poll();
            flush();
        }
    }
}

static void ensure_task(void)
{
    if (s_task_started) {
        return;
    }
    /* Below the shell: a slow repaint may lag, the console may not. */
    if (xTaskCreate(desk_task, "osddesk", 1024, NULL, 2, NULL) == pdPASS) {
        s_task_started = true;
    }
}

/* ------------------------------------------------------------- the console */

void tang_osd_desk_feed(const void *data, size_t length)
{
    if (!s_enabled) {
        return;
    }
    taskENTER_CRITICAL();
    td_vterm_write(&s_vt, (const uint8_t *)data, (int)length);
    taskEXIT_CRITICAL();
}

/* ----------------------------------------------------------- the interface */

int tang_osd_desk_set(bool on)
{
    if (!on) {
        s_enabled = false;
        if (tang_fpga_link_open() != 0) {
            return -1;
        }
        const uint8_t p[1] = { 0 };
        tang_fpga_lock();
        const int rc = tang_fpga_frame(FPGA_CMD_DESK_CTRL, p, sizeof(p));
        tang_fpga_unlock();

        /* The overlay goes too, and leaving it behind is not cosmetic.
         * Turning the layer off while the overlay is still asserted falls
         * through the compositor to the *legacy* page -- which is a black
         * screen with the core's logo on it, the very symptom PROT-005
         * describes, arrived at from the other direction.  Stopping the layer
         * means going back to the game, which is why anyone stops it.  The
         * console mirror does the same thing for the same reason. */
        (void)tang_osd_set(false);
        return rc;
    }

    if (tang_fpga_link_open() != 0) {
        return -1;
    }

    palette_init();
    s_in_head = s_in_tail = 0;
    tang_pad_reset(&s_pad, TANG_DESK_COLS, TANG_DESK_ROWS);
    td_vterm_init(&s_vt, TANG_DESK_COLS, TANG_DESK_ROWS);
    s_vt.reply = desk_reply;
    s_vt.reply_user = NULL;

    /* Nothing has been sent yet, so nothing matches: force a full repaint. */
    for (int y = 0; y < TANG_DESK_ROWS; y++) {
        for (int x = 0; x < TANG_DESK_COLS; x++) {
            s_shadow[y][x].ch = 0xFFFFu;
            s_shadow[y][x].fg = 0xFFu;
            s_shadow[y][x].bg = 0xFFu;
        }
    }
    s_cells = s_rows = s_dropped = 0;

    const uint8_t p[1] = { 1 };
    tang_fpga_lock();
    const int rc = tang_fpga_frame(FPGA_CMD_DESK_CTRL, p, sizeof(p));
    tang_fpga_unlock();
    if (rc != 0) {
        return -1;
    }

    /* The overlay is what makes the layer visible, and clearing it is how the
     * cartridge path hands the screen to the game -- so putting the desktop up
     * is the same byte, and hiding it needs nothing new from the loader. */
    if (tang_osd_set(true) != 0) {
        return -1;
    }

    s_enabled = true;
    ensure_task();

    /* Start from a clean grid rather than whatever the shell last printed. */
    flush();
    return 0;
}

bool tang_osd_desk_enabled(void)
{
    return s_enabled;
}

int tang_osd_desk_command(int argc, char **argv)
{
    if (argc < 3) {
        tdsh_printf("usage: osd desk on | osd desk off | osd desk status\r\n");
        tdsh_printf("The desktop layer is %d columns by %d rows, drawn over the whole\r\n",
                    TANG_DESK_COLS, TANG_DESK_ROWS);
        tdsh_printf("HDMI output while the overlay is on.\r\n");
        return 1;
    }

    const char *what = argv[2];
    if (strcmp(what, "on") == 0) {
        if (tang_osd_desk_set(true) != 0) {
            tdsh_printf("osd desk: could not start the layer\r\n");
            return 1;
        }
        tdsh_printf("osd desk: on\r\n");
        return 0;
    }
    if (strcmp(what, "off") == 0) {
        (void)tang_osd_desk_set(false);
        tdsh_printf("osd desk: off\r\n");
        return 0;
    }
    if (strcmp(what, "status") == 0) {
        tdsh_printf("osd desk: %s, %u cells in %u rows sent, %u refused\r\n",
                    s_enabled ? "on" : "off", s_cells, s_rows, s_dropped);
        tdsh_printf("osd desk: pointer at (%d, %d) of %dx%d\r\n",
                    s_pad.x, s_pad.y, s_pad.cols, s_pad.rows);
        return 0;
    }

    tdsh_printf("osd desk: expected on, off or status\r\n");
    return 1;
}
