// TinyTang — the desktop layer, driven from the console tap.
//
// See tang_osd_desk.h for what this is and why it is shaped this way.  This
// file is the mechanism, in two modes.  For the console: a terminal emulator
// sized to the layer, a shadow of what the core has been told, a diff, and a
// task that sends it.  For the desktop: TinyDesk draws the layer itself
// (TinyDesk patch 0005, a local display) -- the task diffs TinyDesk's composed
// screen against TinyDesk's own front buffer, which records what the core has
// been told, and sends the difference.  No escape sequences, no emulator; the
// console's emulator and shadow (about 30 KB) are freed for as long as the
// desktop runs.

#include "tang_osd_desk.h"
#include "tang_osd_desk_local.h"
#include "tang_osd.h"
#include "tang_fpga_link.h"
#include "tang_pad.h"
#include "tang_key.h"
#include "tang_ble.h"
#include "tang_btpad.h"
#include "tang_wiipad.h"
#include "tdsh_bl616.h"

/* The console writer matches this string byte for byte to keep the query off
 * the wire, so its length is part of the contract, not a detail. */
_Static_assert(sizeof(TANG_DESK_SIZE_QUERY) - 1 == TANG_DESK_SIZE_QUERY_LEN,
               "TANG_DESK_SIZE_QUERY_LEN must match TANG_DESK_SIZE_QUERY");

#include <string.h>

#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "tinydesk/td.h"
#include "tinydesk/td_config.h"
#include "tinydesk/td_screen.h"
#include "tinydesk/td_vterm.h"

#include "tdsh.h"

int tdsh_printf(const char *fmt, ...);

/* How often the diff runs.  The desktop repaints its clock once a second and
 * the pointer on every move; 40 ms is well inside what a person notices and
 * keeps the link idle between changes. */
#define DESK_POLL_MS 40

/* How long the desk task will wait for the transmit lock before it gives the
 * poll up.  A frame's own transmit is well inside this; a host's pipe and a
 * load hold the lock for seconds, and the task must not go with them.  Below
 * DESK_POLL_MS, so a skipped poll cannot put the task behind. */
#define DESK_LOCK_MS 20

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

/* The Bluetooth keyboard, the only keyboard: its held key, for repeat, and
 * its F12 edge. */
typedef struct {
    tang_key_t kbd;
    bool       toggle_prev;    /* F12 was down at the last poll */
} desk_kbd_t;

static desk_kbd_t   s_ble;

/* The console's emulator, and the shadow of what the core has been told of
 * it: about 30 KB, taken from the heap while the layer shows the console and
 * given back while TinyDesk draws the layer itself (s_local). */
typedef struct {
    td_vterm_t vt;
    td_vcell_t shadow[TANG_DESK_ROWS][TANG_DESK_COLS];
} desk_console_t;
static desk_console_t *s_con;
#define s_vt     (s_con->vt)
#define s_shadow (s_con->shadow)

/* TinyDesk draws the layer: its screen and its record of what the core shows
 * are read under s_frame_lock, which TinyDesk holds while it composes. */
static volatile bool     s_local;
static SemaphoreHandle_t s_frame_lock;
static TaskHandle_t      s_task;

static void desk_invalidate(void);
static bool desk_lock(void);

/* Block glyphs.  TinyDesk draws pictures with half-block and quadrant
 * characters, and its scrollbars and lists with shades and pointers; the
 * layer's font is ASCII, so all of those used to reach it as '#', ':' or '?'.
 * Desktop ABI 1.7 puts 23 of them in font slots 0x01-0x17, which are control
 * codes the font left blank, so the 7-bit cell is unchanged.  Only that font
 * has them: a game core's desk layer, or an older desktop core, still draws
 * blanks there, so the slots are used only while tang_core_load has seen a
 * desktop core that reports ABI 1.7 or later, and everything else keeps the
 * ASCII fallback.  The table is TinyTang's; the slots match fpga/desktop/font.vh. */
static volatile bool s_blocks;

void tang_osd_desk_set_block_glyphs(bool on)
{
    if (s_blocks == on) {
        return;
    }
    /* The shadow records what each cell held in Unicode, not what was sent, so
     * a change of font would not resend a cell that did not change.  Forget
     * the shadow and the next flush repaints the grid in the new mapping. */
    s_blocks = on;
    desk_invalidate();
}

bool tang_osd_desk_block_glyphs(void)
{
    return s_blocks;
}

uint8_t tang_osd_desk_block_slot(uint32_t ch)
{
    switch (ch) {
    case 0x2580u: return 0x01u;   /* upper half */
    case 0x2584u: return 0x02u;   /* lower half */
    case 0x2588u: return 0x03u;   /* full block */
    case 0x258Cu: return 0x04u;   /* left half */
    case 0x2590u: return 0x05u;   /* right half */
    case 0x2591u: return 0x06u;   /* light shade */
    case 0x2592u: return 0x07u;   /* medium shade */
    case 0x2593u: return 0x08u;   /* dark shade */
    case 0x2596u: return 0x09u;   /* quadrant lower left */
    case 0x2597u: return 0x0Au;   /* quadrant lower right */
    case 0x2598u: return 0x0Bu;   /* quadrant upper left */
    case 0x2599u: return 0x0Cu;   /* quadrants UL, LL, LR */
    case 0x259Au: return 0x0Du;   /* quadrants UL, LR */
    case 0x259Bu: return 0x0Eu;   /* quadrants UL, UR, LL */
    case 0x259Cu: return 0x0Fu;   /* quadrants UL, UR, LR */
    case 0x259Du: return 0x10u;   /* quadrant upper right */
    case 0x259Eu: return 0x11u;   /* quadrants UR, LL */
    case 0x259Fu: return 0x12u;   /* quadrants UR, LL, LR */
    case 0x25B2u: return 0x13u;   /* up triangle */
    case 0x25BAu: return 0x14u;   /* right pointer */
    case 0x25BCu: return 0x15u;   /* down triangle */
    case 0x25C4u: return 0x16u;   /* left pointer */
    case 0x25A0u: return 0x17u;   /* black square */
    default:      return 0u;
    }
}

static bool        s_enabled;
static bool        s_task_started;
static int         s_rearm;          /* polls left to re-arm after a reload */
static unsigned    s_cells;
static unsigned    s_rows;
static unsigned    s_dropped;
static volatile bool s_pointer;      /* the desktop is running; see the header */

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

/* All or nothing: a push is one key or one batch of pointer reports, and
 * cutting it short where the ring fills would leave half an escape sequence
 * for the parser to read the rest of the input through. */
static void desk_in_push(const char *data, int len)
{
    taskENTER_CRITICAL();
    const int used = (s_in_tail - s_in_head + DESK_IN_MAX) % DESK_IN_MAX;
    if (len <= DESK_IN_MAX - 1 - used) {    /* else full: dropped, never blocked */
        for (int i = 0; i < len; i++) {
            s_in[s_in_tail] = data[i];
            s_in_tail = (s_in_tail + 1) % DESK_IN_MAX;
        }
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
    /* No pointer, no highlight.  The cell it last sat on now differs from
     * the shadow, so the next poll puts the real cell back by itself. */
    if (!s_pointer || x != s_pad.x || y != s_pad.y) {
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

/* One cell into the payload, in the core's form. */
static void put_cell(int *n, const td_vcell_t c)
{
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
        const uint8_t slot = s_blocks ? tang_osd_desk_block_slot(ch) : 0u;
        ch = slot != 0u ? slot : td_ascii_fallback(ch);
    }

    s_payload[(*n)++] = (uint8_t)(ch & 0x7Fu);
    s_payload[(*n)++] = (uint8_t)(f >> 8);
    s_payload[(*n)++] = (uint8_t)(f & 0xFFu);
    s_payload[(*n)++] = (uint8_t)(b >> 8);
    s_payload[(*n)++] = (uint8_t)(b & 0xFFu);
}

/* A TinyDesk cell as the console's emulator would have left it after reading
 * TinyDesk's escape sequences: bold brightens a system colour, reverse swaps
 * the two, and a code point past 16 bits is '?' (vterm.c put_char).  So the
 * layer looks the same in either mode. */
static td_vcell_t local_cell(const td_cell_t c)
{
    uint8_t fg = c.fg, bg = c.bg;
    if ((c.attr & TD_BOLD) && fg < 8) {
        fg = (uint8_t)(fg + 8);
    }
    if (c.attr & TD_REVERSE) {
        const uint8_t t = fg;
        fg = bg;
        bg = t;
    }
    td_vcell_t v = { (c.ch > 0xFFFFu) ? (uint16_t)'?' : (uint16_t)c.ch, fg, bg };
    return v;
}

/* Local mode: one row of TinyDesk's screen, with the pointer, against
 * TinyDesk's front buffer -- which records what the core holds, in the
 * console emulator's terms (attr 0) -- and the span that differs sent.
 *
 * The frame lock (TinyDesk composing) and the link lock are never held
 * together: the span is read under the first, sent under the second, and
 * recorded under the first again.  Holding both would deadlock against a
 * desktop app that talks to the core while it draws.  What is recorded is
 * what was sent, so a screen that changed meanwhile is simply different from
 * the record on the next pass.  -1 is a failed send (the record is then
 * unknown), 0 nothing sent for want of the link, 1 done. */
static td_vcell_t s_span[TANG_DESK_COLS];

static int flush_local_row(int y)
{
    int first = -1, last = -1;
    xSemaphoreTake(s_frame_lock, portMAX_DELAY);
    const td_buffer_t *screen = td_display_screen();
    const td_buffer_t *shown = td_display_shown();
    if (y < screen->rows && screen->cols >= TANG_DESK_COLS && shown->cols >= TANG_DESK_COLS) {
        const td_cell_t *row = &screen->cells[y * screen->cols];
        const td_cell_t *have = &shown->cells[y * shown->cols];
        for (int x = 0; x < TANG_DESK_COLS; x++) {
            const td_vcell_t c = pointer_cell(local_cell(row[x]), x, y);
            s_span[x] = c;
            if (c.ch != have[x].ch || c.fg != have[x].fg || c.bg != have[x].bg ||
                have[x].attr != 0) {
                if (first < 0) {
                    first = x;
                }
                last = x;
            }
        }
    }
    xSemaphoreGive(s_frame_lock);
    if (first < 0) {
        return 1;
    }

    int n = 0;
    for (int x = first; x <= last; x++) {
        put_cell(&n, s_span[x]);
    }
    if (!desk_lock()) {
        return 0;
    }
    const bool ok = send_cursor((uint8_t)first, (uint8_t)y) &&
                    send_cells(s_payload, (size_t)(last - first + 1));
    tang_fpga_unlock();
    if (!ok) {
        return -1;
    }
    s_rows++;

    xSemaphoreTake(s_frame_lock, portMAX_DELAY);
    td_cell_t *have = &td_display_shown()->cells[y * td_display_shown()->cols];
    for (int x = first; x <= last; x++) {
        have[x].ch = s_span[x].ch;
        have[x].fg = s_span[x].fg;
        have[x].bg = s_span[x].bg;
        have[x].attr = 0;
    }
    xSemaphoreGive(s_frame_lock);
    return 1;
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
            put_cell(&n, c);
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
    if (!send_cells(s_payload, (size_t)(last - first + 1))) {
        return false;
    }
    /* Counted here, where a row was actually sent, not per row examined: the
     * status line says "rows sent", and counting every row of every poll made
     * that read as polls times 45. */
    s_rows++;
    return true;
}

/* Every one of the desk layer's own sends takes the link this way, and this is
 * the one place it is written: nothing while a host owns the link, and a
 * bounded wait for the lock itself.  Entry 79's guard tested
 * tang_fpga_link_open(), which only says the UART is up.  The link_open() call
 * stays for its own sake, since it brings the UART up. */
static bool desk_lock(void)
{
    if (tang_fpga_link_open() != 0 || tang_fpga_pipe_open()) {
        return false;
    }
    return tang_fpga_lock_within(DESK_LOCK_MS);
}

/* Whatever the core holds is unknown: the next flush sends every cell.  The
 * console's shadow is under the link lock's protection from a flush (the
 * caller may hold it); TinyDesk's record is under the frame lock, which is
 * never taken with the link lock held. */
static void console_invalidate(void)
{
    taskENTER_CRITICAL();
    if (s_con != NULL) {
        for (int y = 0; y < TANG_DESK_ROWS; y++) {
            for (int x = 0; x < TANG_DESK_COLS; x++) {
                s_shadow[y][x].ch = 0xFFFFu;
            }
        }
    }
    taskEXIT_CRITICAL();
}

static void desk_invalidate(void)
{
    if (s_local) {
        xSemaphoreTake(s_frame_lock, portMAX_DELAY);
        td_buffer_invalidate(td_display_shown());
        xSemaphoreGive(s_frame_lock);
        return;
    }
    console_invalidate();
}

/* Local mode: every row, each taking its own locks (flush_local_row). */
static void flush_local(void)
{
    if (tang_fpga_link_open() != 0 || tang_fpga_pipe_open()) {
        return;
    }
    for (int y = 0; y < TANG_DESK_ROWS; y++) {
        const int r = flush_local_row(y);
        if (r < 0) {
            desk_invalidate();
            s_dropped++;
        }
        if (r <= 0) {
            break;
        }
    }
}

/* The mode and the console are switched only under the link lock
 * (tang_osd_desk_local_begin/end), so neither changes under a flush. */
static void flush(void)
{
    if (s_local) {
        flush_local();
        return;
    }
    if (!desk_lock()) {
        return;
    }
    if (s_local || s_con == NULL) {
        tang_fpga_unlock();
        return;
    }
    for (int y = 0; y < TANG_DESK_ROWS; y++) {
        if (!flush_row(y)) {
            /* The link refused part of a row.  The shadow for that row is
             * already updated, so invalidate everything: the next poll
             * repaints from scratch rather than leaving the core and the
             * emulator quietly disagreeing. */
            console_invalidate();
            s_dropped++;
            break;
        }
    }
    tang_fpga_unlock();
}

static void ensure_task(void);

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
    /* F12 has to work from the first core load whether or not anyone has
     * turned the layer on -- so the task that watches for it starts here at
     * the latest. */
    ensure_task();
}

/* The pad word: the dock's pad as the core relays it, with a Classic
 * Bluetooth pad's home joined to it as LB (tang_btpad.h), so it shows and
 * hides the desktop as the dock pad's home tap does.  Only LB: the rest of a
 * Bluetooth pad drives the desktop through tang_btpad_poll(), with the wired
 * Wii pad's key map, and its directions here would move the pointer as well
 * as typing arrows. */
static void desk_joypad(uint16_t *joy1, uint16_t *joy2)
{
    tang_fpga_joypad(joy1, joy2);
    *joy1 |= (uint16_t)((tang_btpad_pad_word(0) | tang_btpad_pad_word(1)) & TANG_PAD_LB);
}

/* The reserved toggle key's press, from one report.  Edge, not level: the
 * report is state, and a held F12 is one switch, not one per poll. */
static bool toggle_pressed(desk_kbd_t *src, const uint8_t rep[8])
{
    const bool down = tang_key_toggle_down(&rep[2]);
    const bool pressed = down && !src->toggle_prev;
    src->toggle_prev = down;
    return pressed;
}

/* One mouse count as the byte an input frame carries: signed, and clamped
 * rather than wrapped, since one report can hold more movement than a byte. */
static uint8_t pointer_count(int value)
{
    if (value > 127) {
        return 127u;
    }
    if (value < -128) {
        return 128u;   /* the byte the core reads back as signed -128 */
    }
    return (uint8_t)(int8_t)value;
}

/* Hand the mouse to the core as an input frame, so that a program under the
 * desktop layer can drive a camera with it.  tang_ble_mouse() gives movement
 * out once, so this and the desktop's own pointer see it between them, never
 * both.  A frame goes only when there is something to say -- and only while
 * the link is ours, because the host's pipe owns it while a program is being
 * loaded and those bytes are not ours to interleave. */
static void pointer_frame(const tang_mouse_t *mouse)
{
    static uint8_t last_buttons;
    const uint8_t payload[6] = {
        0xFFu, 0xFFu, mouse->buttons, pointer_count(mouse->dx),
        pointer_count(mouse->dy), pointer_count(mouse->wheel),
    };

    if (mouse->dx == 0 && mouse->dy == 0 && mouse->wheel == 0 &&
        mouse->buttons == last_buttons) {
        return;
    }
    last_buttons = mouse->buttons;
    if (!desk_lock()) {
        return;
    }
    (void)tang_fpga_frame(FPGA_CMD_INPUT, payload, sizeof payload);
    tang_fpga_unlock();
}

/* Hand the Bluetooth pads to the game, as the joystick frame (0x09): the
 * wired pads reach a core through its own fabric, but these end here.  While
 * the desktop is shown the pads are the desktop's, so the game is given
 * nothing then -- a release, if something was held.  A frame goes every poll
 * while a button is held, so a core can let a word go when the frames stop
 * (the link was taken, or this task stalled), and once more when all are
 * released; with nothing held nothing is sent.  The words are remembered only
 * once a frame is on the link, as for the keyboard below, so a poll whose lock
 * was refused cannot lose a release. */
static void joystick_frame(bool shown)
{
    static uint16_t last1, last2;
    const uint16_t hid1 = shown ? 0u : tang_btpad_core_word(0);
    const uint16_t hid2 = shown ? 0u : tang_btpad_core_word(1);

    if (hid1 == 0 && hid2 == 0 && last1 == 0 && last2 == 0) {
        return;
    }
    if (!desk_lock()) {
        return;
    }
    const uint8_t payload[4] = {
        (uint8_t)(hid1 >> 8), (uint8_t)hid1, (uint8_t)(hid2 >> 8), (uint8_t)hid2,
    };
    if (tang_fpga_frame(FPGA_CMD_JOYSTICK, payload, sizeof payload) == 0) {
        last1 = hid1;
        last2 = hid2;
    }
    tang_fpga_unlock();
}

/* The mouse at the shell prompt, where there is no pointer to move.
 *
 * The line editor already knows the two things this hardware can drive: Up
 * and Down walk the shell's own history, kept in ~/.tdsh_history and appended
 * for every line that runs, and Enter runs the line on screen.  So the wheel
 * becomes arrows and a click becomes Enter -- pushed into the ring the
 * keyboard types into, where the shell cannot tell them apart.  Wheel up once
 * recalls the last command and draws it, and a click runs it.
 *
 * Only at the prompt: while a command runs the ring belongs to that command,
 * and a click must not become a byte it reads.  A press that arrives then is
 * dropped, not queued for the next prompt -- the press is handed over once.
 */
static void console_mouse(const tang_mouse_t *mouse)
{
    if (!s_enabled || tdsh_bl616_console_state() != TDSH_BL616_AT_PROMPT) {
        return;
    }

    /* Away from the user is Up, the most recent command first.  Clamped per
     * poll: a fast spin would otherwise fill the ring with arrows. */
    int steps = mouse->wheel;
    if (steps > TANG_MOUSE_WHEEL_MAX) {
        steps = TANG_MOUSE_WHEEL_MAX;
    } else if (steps < -TANG_MOUSE_WHEEL_MAX) {
        steps = -TANG_MOUSE_WHEEL_MAX;
    }
    for (; steps > 0; steps--) {
        desk_in_push("\x1b[A", 3);
    }
    for (; steps < 0; steps++) {
        desk_in_push("\x1b[B", 3);
    }

    /* A click is Enter, on the line as it stands: a recalled command or one
     * typed at the prompt.  The press edge, so a held button is one Enter. */
    if (mouse->pressed & TANG_MOUSE_LEFT) {
        desk_in_push("\r", 1);
    }
}

/* Hand the Bluetooth keyboard to the core as an input frame, so that a program
 * under the desktop layer can read keys.  The payload is the HID boot report's
 * own shape: the modifier byte and then the six usage codes.  A frame goes
 * only when the report changes, because a boot keyboard reports state rather
 * than events -- a key held down sends one report and then nothing -- so a core
 * that wants auto-repeat generates it itself, the way tang_key_step() does for
 * the desktop. */
static void keyboard_frame(const uint8_t mods, const uint8_t keys[6])
{
    static uint8_t last[7];
    const uint8_t payload[7] = { mods, keys[0], keys[1], keys[2],
                                 keys[3], keys[4], keys[5] };

    if (memcmp(payload, last, sizeof payload) == 0) {
        return;
    }
    /* The report is remembered only once it has been handed to the link.  It
     * used to be remembered first, and a poll whose lock was refused then
     * dropped the keystroke for good: the next poll compared against the same
     * payload, found no change, and sent nothing -- so a press or a release
     * lost in a busy link never reached the core, and the core's idea of the
     * keyboard stayed where it was. */
    if (!desk_lock()) {
        return;
    }
    memcpy(last, payload, sizeof payload);
    (void)tang_fpga_frame(FPGA_CMD_KEYBOARD, payload, sizeof payload);
    tang_fpga_unlock();
}

static void desk_poll(void)
{
    /* Re-arm first: until the core knows the layer is on, nothing sent is
     * shown. */
    if (s_rearm > 0) {
        s_rearm--;
        const uint8_t p[1] = { 1 };
        if (desk_lock()) {
            (void)tang_fpga_frame(FPGA_CMD_DESK_CTRL, p, sizeof(p));
            tang_fpga_unlock();
        }
        /* Restore the overlay to what it was, rather than forcing it on.  The
         * re-arm runs below the shell, so during a cartridge load it is
         * starved until nesload has finished -- and nesload's last act is to
         * clear the overlay to hand the screen to the game.  Forcing it on
         * here then put it straight back over the game: the TinyDesk layer on
         * a core that has one, and on one that has not, the legacy page and
         * its logo with the game running invisibly behind (the symptom of
         * core-log cycle 4).  A reset core asserts the overlay itself
         * (PROT-005), so restoring the recorded state is what keeps the two
         * in agreement.  Read and sent under one lock (tang_osd_reassert):
         * reading first and locking second let a re-arm that had to wait for
         * nesload's lock send the state from before nesload hid the overlay,
         * which put TinyDesk back over a running game. */
        (void)tang_osd_reassert();
        /* Whatever the core holds is unknown, so everything goes again. */
        desk_invalidate();
    }

    uint16_t joy1 = 0, joy2 = 0;
    desk_joypad(&joy1, &joy2);

    /* The Bluetooth keyboard, read once per poll whatever is on screen, so
     * the toggle and the typing share one report.  It is the HID boot
     * layout: byte 0 the modifier, byte 1 reserved, bytes 2..7 the usage
     * codes.  The pointer mode is applied here: with left-alt held its
     * arrows, Enter and Esc join the pad word and leave the report.  It sends
     * only on change, so it counts as fresh for as long as it is connected
     * (tang_ble.h). */
    uint8_t brep[8];
    const bool bfresh = tang_ble_keyboard(brep);
    joy1 |= tang_key_pointer(brep[0], &brep[2]);

    /* The Bluetooth mouse: movement and wheel since the last poll, handed
     * over once, so they are taken every poll -- and used twice: the desktop
     * draws its pointer with them, and the core is given them as an input
     * frame for whatever program is running under the layer. */
    tang_mouse_t mouse;
    (void)tang_ble_mouse(&mouse);
    /* The Wii pad beside the Bluetooth mouse: one pointer, and the same
     * wheel for the shell's history.  It reads the desktop core's I2C bus, so
     * it does nothing while another core holds the fabric.  Its spare buttons
     * come back as the keys the desktop already understands. */
    tang_wii_keys_t padkeys;
    memset(&padkeys, 0, sizeof padkeys);
    (void)tang_wiipad_poll(&mouse, !s_pointer, &padkeys);
    /* A Classic Bluetooth pad, with the same key map. */
    tang_btpad_poll(&mouse, !s_pointer, &padkeys);
    pointer_frame(&mouse);
    joystick_frame(tang_osd_shown());
    if (padkeys.n > 0) {
        desk_in_push(padkeys.keys, (size_t)padkeys.n);
    }

    /* The keyboard beside it, from the same report the desktop types from, so
     * that a program under the layer sees the keys whether or not the desktop
     * is drawn.  Freshness is the desktop's, read-and-clear; this only watches
     * the report itself. */
    keyboard_frame(brep[0], &brep[2]);

    /* The session switch: L on the pad, or F12 on the keyboard, the key
     * reserved for it the way MiSTer reserves F12 for its menu.  It does not
     * stop the layer or the desktop: it flips the overlay, which is the same
     * byte the cartridge loader clears to hand the screen to the game.  So the
     * desktop keeps running and its grid stays in the core, and coming back is
     * a frame rather than a repaint.  The state is read back from the OSD
     * module rather than tracked here, so it cannot disagree with what the
     * loader did.  Both are read whatever is on screen, because switching back
     * is exactly the case where the desktop is hidden. */
    const bool pad_toggle =
        ((uint16_t)(joy1 & (uint16_t)~s_pad.prev) & TANG_PAD_LB) != 0;
    const bool key_toggle = toggle_pressed(&s_ble, brep);

    if (tang_osd_shown()) {
        if (s_pointer) {
            char seq[192];
            bool ignored = false;
            const int n = tang_pad_pointer(&s_pad, joy1, &mouse, seq, sizeof(seq),
                                           &ignored);
            if (n > 0) {
                desk_in_push(seq, n);
            }
        } else {
            /* Console mode: the pad moves nothing, and the mouse is the
             * shell's -- the wheel scrolls its history, a click runs the
             * line.  Tracking the pad keeps L's edge. */
            s_pad.prev = joy1;
            console_mouse(&mouse);
        }

        /* Then the keyboard, if the core has one.  After the pointer, so that
         * a keystroke and a pointer move arriving in the same poll keep the
         * order they happened in -- which matters for a click that opens a
         * window followed by typing into it. */
        char typed[32];
        const uint32_t now_ms =
            (uint32_t)xTaskGetTickCount() * (1000u / (uint32_t)configTICK_RATE_HZ);
        const int tn = tang_key_step(&s_ble.kbd, brep[0], &brep[2], bfresh, now_ms,
                                     typed, sizeof(typed));
        if (tn > 0) {
            desk_in_push(typed, tn);
        }
    } else {
        /* While the desktop is hidden the pad belongs to the game.  Tracking it
         * keeps L's edge from being missed, but a pointer nobody can see should
         * not wander across the screen while somebody is playing -- coming back
         * to find it somewhere else is a small thing that reads as a fault. */
        s_pad.prev = joy1;
        /* The keys belong to the game too.  Absorbing them means a key still
         * held when TinyDesk comes back -- an arrow mid-jump -- is neither a
         * fresh press nor a repeat into the shell. */
        tang_key_absorb(&s_ble.kbd, brep[0], &brep[2]);
    }

    if (pad_toggle || key_toggle || padkeys.toggle) {
        (void)tang_osd_toggle();
    }
}

/* F12 with the layer off altogether -- after `osd desk off`, or after a
 * tangload with no `osd desk on` -- brings TinyDesk up, which is what the key
 * promises.  It comes up on a fresh grid, as `osd desk on` does, because
 * nothing was mirrored while the layer was off; the next output repaints it.
 *
 * L on the pad does the same, so a pad on its own can reach the desktop: the
 * core reports the tap as LB in its pad word (TinySNES turns a HOME tap into
 * one and never reports the game's own L).  set(true) resets the pad, which
 * forgets the edge -- and LB is still down on the next poll, which would read
 * it as a fresh press and hide the desktop it had just put up.  So the word
 * that brought it up is put back as the previous one. */
static void desk_watch_toggle(void)
{
    if (tang_fpga_link_open() != 0 || tang_fpga_pipe_open()) {
        return;
    }
    uint16_t joy1 = 0, joy2 = 0;
    desk_joypad(&joy1, &joy2);
    const bool pad_toggle =
        ((uint16_t)(joy1 & (uint16_t)~s_pad.prev) & TANG_PAD_LB) != 0;
    s_pad.prev = joy1;

    uint8_t brep[8];
    (void)tang_ble_keyboard(brep);
    const bool key_toggle = toggle_pressed(&s_ble, brep);
    if (pad_toggle || key_toggle) {
        if (tang_osd_desk_set(true) == 0) {
            s_pad.prev = joy1;
        }
    }
}

/* What the desk task does while the layer is hidden -- which is where a game
 * or an emulator is.  The core wants both input frames there: the mouse to
 * drive a camera, and the keyboard so a program under the layer can read keys.
 * desk_poll() is the branch that does not run in that state, so both are read
 * and sent here.
 *
 * The mouse was moved into this branch first, for exactly this reason (entry
 * 79).  The keyboard was added inside desk_poll() only, so a key did nothing
 * while something else held the screen -- which is how Tiny89's calculator
 * came to ignore its keyboard.  A function of its own rather than a block in
 * desk_task() because the task loop is infinite and this is what a host test
 * can call (tools/tests/tb_osd_desk.c). */
static void desk_poll_hidden(void)
{
    tang_mouse_t mouse;
    (void)tang_ble_mouse(&mouse);
    (void)tang_wiipad_poll(&mouse, false, NULL);
    tang_btpad_poll(&mouse, false, NULL);
    pointer_frame(&mouse);
    joystick_frame(tang_osd_shown());

    /* After the pointer, so a keystroke and a pointer move arriving in the
     * same poll keep the order they happened in, as in desk_poll(). */
    uint8_t brep[8];
    (void)tang_ble_keyboard(brep);
    keyboard_frame(brep[0], &brep[2]);

    desk_watch_toggle();
}

static void desk_task(void *arg)
{
    (void)arg;
    s_task = xTaskGetCurrentTaskHandle();
    for (;;) {
        /* A poll every DESK_POLL_MS, or at once when TinyDesk has composed a
         * frame (local_frame), so a click is not answered 40 ms late. */
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(DESK_POLL_MS));
        if (s_enabled) {
            desk_poll();
            flush();
        } else {
            desk_poll_hidden();
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
    /* While TinyDesk draws the layer, the console's text is not on it. */
    taskENTER_CRITICAL();
    if (s_con != NULL) {
        td_vterm_write(&s_vt, (const uint8_t *)data, (int)length);
    }
    taskEXIT_CRITICAL();
}

/* The console's emulator and shadow, fresh: a blank grid that matches nothing
 * the core holds.  0 or -1 when the heap has no room for it. */
static int console_open(void)
{
    if (s_con == NULL) {
        desk_console_t *c = malloc(sizeof *c);
        if (c == NULL) {
            return -1;
        }
        taskENTER_CRITICAL();
        s_con = c;
        taskEXIT_CRITICAL();
    }
    taskENTER_CRITICAL();
    td_vterm_init(&s_vt, TANG_DESK_COLS, TANG_DESK_ROWS);
    s_vt.reply = desk_reply;
    s_vt.reply_user = NULL;
    for (int y = 0; y < TANG_DESK_ROWS; y++) {
        for (int x = 0; x < TANG_DESK_COLS; x++) {
            s_shadow[y][x].ch = 0xFFFFu;
            s_shadow[y][x].fg = 0xFFu;
            s_shadow[y][x].bg = 0xFFu;
        }
    }
    taskEXIT_CRITICAL();
    return 0;
}

static void console_close(void)
{
    taskENTER_CRITICAL();
    desk_console_t *c = s_con;
    s_con = NULL;
    taskEXIT_CRITICAL();
    free(c);
}

/* ------------------------------------------------- TinyDesk draws the layer */

static void local_lock(void *ctx)
{
    (void)ctx;
    xSemaphoreTake(s_frame_lock, portMAX_DELAY);
}

static void local_unlock(void *ctx)
{
    (void)ctx;
    xSemaphoreGive(s_frame_lock);
}

static void local_frame(void *ctx)
{
    (void)ctx;
    if (s_task != NULL) {
        xTaskNotifyGive(s_task);
    }
}

const td_local_display_t *tang_osd_desk_local_begin(void)
{
    static const td_local_display_t display = {
        TANG_DESK_COLS, TANG_DESK_ROWS, local_frame, local_lock, local_unlock, NULL,
    };
    if (!s_enabled) {
        return NULL;            /* no layer: the desktop draws to a terminal */
    }
    if (s_frame_lock == NULL) {
        s_frame_lock = xSemaphoreCreateMutex();
        if (s_frame_lock == NULL) {
            return NULL;
        }
    }
    tang_fpga_lock();           /* not under a flush (see flush) */
    s_local = true;
    console_close();
    tang_fpga_unlock();
    return &display;
}

void tang_osd_desk_local_end(void)
{
    if (!s_local) {
        return;
    }
    /* Back to the console on a blank grid; the next flush clears what the
     * desktop left on the layer.  With no room for the console, the layer
     * goes off rather than freeze on the last desktop frame. */
    tang_fpga_lock();           /* not under a flush (see flush) */
    s_local = false;
    const int rc = s_enabled ? console_open() : 0;
    tang_fpga_unlock();
    if (rc != 0) {
        (void)tang_osd_desk_set(false);
    }
}

/* ----------------------------------------------------------- the interface */

int tang_osd_desk_set(bool on)
{
    if (!on) {
        s_enabled = false;
        if (!s_local) {
            tang_fpga_lock();   /* not under a flush (see flush) */
            console_close();
            tang_fpga_unlock();
        }
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
    tang_wiipad_reset();
    tang_key_reset(&s_ble.kbd);
    /* Nothing has been sent yet, so nothing matches: a full repaint, from the
     * console's emulator or from TinyDesk's screen. */
    if (s_local) {
        desk_invalidate();
    } else if (console_open() != 0) {
        return -1;
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

void tang_osd_desk_set_pointer(bool on)
{
    s_pointer = on;
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
        /* Quiet on success, the way `osd clear` is: the layer appearing *is*
         * the feedback, and at boot this line would be the first thing the
         * layer ever mirrors -- so the confirmation would be the first thing
         * on the screen, which is exactly where it should not be.  A failure
         * still speaks. */
        if (tang_osd_desk_set(true) != 0) {
            tdsh_printf("osd desk: could not start the layer\r\n");
            return 1;
        }
        return 0;
    }
    if (strcmp(what, "off") == 0) {
        (void)tang_osd_desk_set(false);
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
