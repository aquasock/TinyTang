// TinyTang — the console, mirrored onto the core's text page.
//
// See tang_osd_term.h for what this is and the exact set of control sequences
// it understands.  This file is the emulator: a 32x25 character grid, a
// cursor, a dirty-row bitmap, and a small task that blinks the cursor.

#include "tang_osd_term.h"
#include "tang_osd.h"
#include "tang_fpga_link.h"

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#define TERM_COLS TANG_TERM_COLS
#define TERM_ROWS TANG_TERM_ROWS

/* The blink period.  A real terminal's cursor is usually on for half a second
 * and off for half a second; the phase is toggled here and the cell is redrawn
 * on each edge. */
#define TERM_BLINK_MS 500

static char     s_grid[TERM_ROWS][TERM_COLS];
static uint32_t s_dirty;            /* one bit per row awaiting a redraw */
static int      s_cx, s_cy;         /* cursor cell */
static int      s_line_top;         /* first row of the logical line */
static int      s_line_end;         /* last row it has been written across */

static bool     s_enabled;
static bool     s_cursor_on;        /* blink phase */
static unsigned s_dropped;          /* bytes or rows the link refused */
static bool     s_task_started;

/* ---------------------------------------------------------------- the grid */

static void mark(int row)
{
    if (row >= 0 && row < TERM_ROWS) {
        s_dirty |= (1u << row);
    }
}

static void mark_all(void)
{
    s_dirty = (1u << TERM_ROWS) - 1u;
}

static void blank_row(int row)
{
    memset(s_grid[row], ' ', TERM_COLS);
    mark(row);
}

/* Everything moves up one row; the cursor stays on the bottom row, which is
 * where the caller is about to write. */
static void scroll(void)
{
    memmove(s_grid[0], s_grid[1], (size_t)TERM_COLS * (TERM_ROWS - 1));
    memset(s_grid[TERM_ROWS - 1], ' ', TERM_COLS);
    mark_all();

    if (s_line_top > 0) s_line_top--;
    if (s_line_end > 0) s_line_end--;
    s_cy = TERM_ROWS - 1;
}

static void down_one(void)
{
    if (s_cy + 1 >= TERM_ROWS) {
        scroll();
    } else {
        s_cy++;
    }
}

static void erase_all(void)
{
    for (int y = 0; y < TERM_ROWS; y++) {
        memset(s_grid[y], ' ', TERM_COLS);
    }
    mark_all();
    s_cx = s_cy = 0;
    s_line_top = s_line_end = 0;
}

/* From the cursor to the end of the line it is on.  Not just to the end of the
 * row: on this page a command is usually wider than one row, and the line
 * editor's redraw depends on the whole of its line being cleared. */
static void erase_to_line_end(void)
{
    for (int x = s_cx; x < TERM_COLS; x++) {
        s_grid[s_cy][x] = ' ';
    }
    mark(s_cy);
    for (int y = s_cy + 1; y <= s_line_end; y++) {
        blank_row(y);
    }
    s_line_end = s_cy;
}

static void put_char(char c)
{
    s_grid[s_cy][s_cx] = c;
    mark(s_cy);
    if (s_cy > s_line_end) {
        s_line_end = s_cy;
    }

    if (s_cx + 1 >= TERM_COLS) {
        s_cx = 0;
        down_one();
        if (s_cy > s_line_end) {
            s_line_end = s_cy;
        }
    } else {
        s_cx++;
    }
}

static void line_break(void)
{
    s_cx = 0;
    down_one();
    s_line_top = s_cy;
    s_line_end = s_cy;
}

/* The one deliberate deviation from a real terminal: `\r` goes back to the
 * start of the logical line, not the start of the row.  See TANG_TERM_ROWS in
 * the header, and the redraw sequence `\r\033[2K` it exists to make clean. */
static void carriage_return(void)
{
    s_cx = 0;
    s_cy = s_line_top;
}

/* ------------------------------------------------------------- the screen */

static void flush(void)
{
    if (s_dirty == 0) {
        return;
    }
    if (tang_fpga_link_open() != 0) {
        s_dropped++;
        return;
    }

    for (int y = 0; y < TERM_ROWS; y++) {
        if (!(s_dirty & (1u << y))) {
            continue;
        }
        if (tang_osd_put(0, (uint8_t)y, s_grid[y], TERM_COLS) != 0) {
            s_dropped++;
        } else {
            s_dirty &= ~(1u << y);
        }
    }
}

/* ------------------------------------------------------------- the parser */

enum { S_TEXT, S_ESC, S_CSI };

static int  s_state = S_TEXT;
static int  s_param;
static bool s_have_param;

static void csi(char final)
{
    const int n = (s_have_param && s_param > 0) ? s_param : 1;

    switch (final) {
    case 'J':
        if (!s_have_param || s_param == 2) {
            erase_all();
        }
        break;
    case 'K':
        /* Both forms erase to the end of the line; the page has no way to
         * erase backwards, and the shell only uses the whole-line form. */
        erase_to_line_end();
        break;
    case 'H':
        s_cx = 0;
        s_cy = 0;
        s_line_top = 0;
        s_line_end = 0;
        break;
    case 'C':
        for (int i = 0; i < n && s_cx + 1 < TERM_COLS; i++) {
            s_cx++;
        }
        break;
    case 'D':
        for (int i = 0; i < n && s_cx > 0; i++) {
            s_cx--;
        }
        break;
    default:
        /* SGR and anything else: the page draws one text colour and one
         * cursor colour, so there is nothing to do. */
        break;
    }
}

static void feed_byte(char c)
{
    switch (s_state) {
    case S_TEXT:
        if (c == '\033') {
            s_state = S_ESC;
        } else if (c == '\r') {
            carriage_return();
        } else if (c == '\n') {
            line_break();
        } else if (c == '\b') {
            if (s_cx > 0) s_cx--;
        } else if ((unsigned char)c >= 0x20 && c != 0x7F) {
            put_char(c);
        }
        /* Other control characters are dropped. */
        return;

    case S_ESC:
        if (c == '[') {
            s_state = S_CSI;
            s_param = 0;
            s_have_param = false;
        } else {
            s_state = S_TEXT;
        }
        return;

    default:    /* S_CSI */
        if (c >= '0' && c <= '9') {
            if (s_param < 100) {
                s_param = s_param * 10 + (c - '0');
            }
            s_have_param = true;
            return;
        }
        if (c == ';') {
            s_have_param = false;   /* only the first parameter is used */
            return;
        }
        csi(c);
        s_state = S_TEXT;
        return;
    }
}

/* ----------------------------------------------------------- the interface */

void tang_osd_term_feed(const void *data, size_t length)
{
    if (!s_enabled) {
        return;
    }
    const char *p = (const char *)data;
    for (size_t i = 0; i < length; i++) {
        feed_byte(p[i]);
    }
    flush();
}

bool tang_osd_term_enabled(void)
{
    return s_enabled;
}

unsigned tang_osd_term_dropped(void)
{
    return s_dropped;
}

/* --------------------------------------------------------------- blinking */

static void blink_task(void *arg)
{
    (void)arg;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(TERM_BLINK_MS));

        char cell = 0;
        uint8_t x = 0, y = 0;
        bool draw = false;

        /* The cursor cell and the grid are shared with the task that feeds
         * the emulator.  Read them under one critical section, then write
         * outside it: the write goes out over the UART and must not be holding
         * interrupts off. */
        taskENTER_CRITICAL();
        if (s_enabled) {
            s_cursor_on = !s_cursor_on;
            x = (uint8_t)s_cx;
            y = (uint8_t)s_cy;
            cell = s_cursor_on ? '_' : s_grid[s_cy][s_cx];
            draw = true;
        }
        taskEXIT_CRITICAL();

        if (draw) {
            (void)tang_osd_put(x, y, &cell, 1);
        }
    }
}

static void ensure_task(void)
{
    if (s_task_started) {
        return;
    }
    /* Below the shell: a blinking cursor may lag, the console may not. */
    if (xTaskCreate(blink_task, "osdblink", 512, NULL, 2, NULL) == pdPASS) {
        s_task_started = true;
    }
}

int tang_osd_term_set(bool on)
{
    if (!on) {
        s_enabled = false;
        /* The page goes too: stopping the terminal means going back to the
         * game, which is the whole reason to stop. */
        return tang_osd_set(false);
    }

    if (tang_fpga_link_open() != 0) {
        return -1;
    }

    for (int y = 0; y < TERM_ROWS; y++) {
        memset(s_grid[y], ' ', TERM_COLS);
    }
    s_cx = s_cy = 0;
    s_line_top = s_line_end = 0;
    s_state = S_TEXT;
    s_dropped = 0;
    s_cursor_on = false;
    mark_all();

    if (tang_osd_set(true) != 0) {
        return -1;
    }
    s_enabled = true;
    flush();

    ensure_task();
    return 0;
}
