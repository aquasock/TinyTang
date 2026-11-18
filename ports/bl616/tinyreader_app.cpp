// TinyTang — Reader: pictures on the desktop core's pixel plane, full screen.
//
// TinyDesk's Image Viewer draws with block characters, so on this board's
// 80x45 grid a page is about 60x82 picture pixels and text cannot be read.
// The desktop core has an RGB565 plane (tang_plane.h): 1280x720 in the dock's
// SDRAM, shown 1:1, from desktop ABI 1.9, and 640x360 shown at 2x on ABI 1.8.
// This app owns the whole screen with it, the way an app blits into a panel on
// TinyDesk's LCD ports, and draws the picture at whatever size the plane
// reports, with zoom and pan.
//
// The window is a list of pictures under /root/media/reader.  Opening one makes
// the window full screen -- so every key and click comes here -- shows the plane,
// and starts a task that streams the file row by row through TinyDesk's own
// reader (td_image_*: BMP, PPM, PGM), box-filters each row down to the plane or
// repeats pixels when zoomed past 1:1, and sends each finished row as it
// completes.  Nothing holds a whole picture: a source row and one accumulator
// row, about 22 KB for a 1280-wide page on the 1280x720 plane.  Only the
// columns that can hold picture are sent -- this picture's and the last one's
// -- so the black margins beside a page are cleared once, not on every row.
//
// While a picture is up:
//   +  -           zoom in and out (1x is the whole picture, up to 8x)
//   arrows, wheel  pan by a quarter of the screen
//   PgDn PgUp      next and previous picture in the folder (also space, n, p)
//   Home  0        back to the whole picture
//   Esc  q         back to the list
//   left click (A on the pad)   zoom in on that point
//   right click (B on the pad)  zoom out; at 1x, back to the list
//
// SPDX-License-Identifier: MIT

extern "C" {
#include "ff.h"
#include "td_apps.h"
#include "tinydesk/td.h"
#include "tinydesk/td_sysinfo.h"
#include "tang_plane.h"

int tdsh_printf(const char *fmt, ...);
}

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace {

constexpr int WIN_W = 54;
constexpr int WIN_H = 20;
constexpr int LIST_ROWS = 13;
constexpr BYTE AM_VOLUME_LABEL = 0x08;
constexpr int MAX_ENTRIES = 256;
constexpr int NAME_BYTES = 56;
constexpr char ROOT[] = "/root/media/reader";
constexpr uint32_t RENDER_STACK_WORDS = 2048;
constexpr int TICK_MS = 200;
constexpr int MAX_ZOOM = 8;

struct entry {
    char name[NAME_BYTES];
    bool is_dir;
};

td_window_t *s_win;
td_widget_t *s_list;
td_widget_t *s_path;
td_widget_t *s_status;
entry *s_entries;
int s_count;
char s_shown[TD_PATH_MAX];
char s_real[TD_PATH_MAX + 4];

/* The view.  The UI side writes the request and bumps s_gen; the render task
 * reads a snapshot, and abandons a render the moment s_gen moves on. */
bool s_viewing;
int s_view_index;
char s_view_real[TD_PATH_MAX + 64];
int s_zoom = 1;
int32_t s_cx = -1, s_cy = -1;           // the view's centre in picture pixels; -1: middle
volatile uint32_t s_gen;
volatile bool s_render_busy;
volatile bool s_plane_on;               // what the plane was last told
char s_render_say[96];
volatile bool s_render_said;

/* What the last render found, for turning clicks and pans into picture
 * coordinates.  Written by the task, read by the UI between renders. */
volatile int32_t s_iw, s_ih;            // the picture
volatile int32_t s_vw, s_vh;            // the part of it on screen
volatile int32_t s_sx0, s_sy0;          // its top-left corner
volatile int32_t s_ox, s_oy, s_dw, s_dh; // where it lies on the plane
int32_t s_pw = 640, s_ph = 360;         // the plane, from tang_plane_size at open

/* The columns of the plane that may hold anything but black: what a render
 * must cover so nothing of the last picture is left.  Unknown, so all of it,
 * whenever the plane is newly shown. */
int32_t s_lit_l = 0, s_lit_r = TANG_PLANE_MAX_WIDTH;

/* ------------------------------------------------------------ the list */

void say(const char *text)
{
    if (s_status != nullptr && strcmp(td_widget_text(s_status), text) != 0) {
        td_widget_set_text(s_status, text);
    }
}

bool at_root(void) { return strcmp(s_shown, ROOT) == 0; }

void set_path(const char *shown)
{
    snprintf(s_shown, sizeof s_shown, "%s", shown);
    snprintf(s_real, sizeof s_real, "/sd%s", shown);
}

bool is_picture(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot != nullptr && (strcasecmp(dot, ".bmp") == 0 || strcasecmp(dot, ".ppm") == 0 ||
                              strcasecmp(dot, ".pgm") == 0);
}

bool listable(const FILINFO &info)
{
    if ((info.fattrib & (AM_HID | AM_SYS | AM_VOLUME_LABEL)) != 0 || info.fname[0] == '.' ||
        strlen(info.fname) >= NAME_BYTES) {
        return false;
    }
    return (info.fattrib & AM_DIR) != 0 || is_picture(info.fname);
}

int compare(const void *a, const void *b)
{
    const entry *x = static_cast<const entry *>(a);
    const entry *y = static_cast<const entry *>(b);
    if (x->is_dir != y->is_dir) {
        return x->is_dir ? -1 : 1;
    }
    return strcasecmp(x->name, y->name);
}

void load(void)
{
    free(s_entries);
    s_entries = nullptr;
    s_count = 0;
    if (s_list != nullptr) {
        td_list_set_count(s_list, 0);
    }
    if (s_path != nullptr) {
        td_widget_set_text(s_path, s_shown);
    }
    DIR dir;
    FILINFO info;
    if (f_opendir(&dir, s_real) != FR_OK) {
        say(at_root() ? "Put BMP pictures in /root/media/reader" : "No folder here");
        return;
    }
    const int parent = at_root() ? 0 : 1;
    int capacity = parent;
    while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != '\0') {
        if (listable(info)) capacity++;
    }
    (void)f_closedir(&dir);
    if (capacity > MAX_ENTRIES) capacity = MAX_ENTRIES;
    s_entries = static_cast<entry *>(calloc(capacity > 0 ? capacity : 1, sizeof(entry)));
    if (s_entries == nullptr) {
        say("Not enough memory for the list");
        return;
    }
    int n = 0;
    if (parent) {
        snprintf(s_entries[n].name, NAME_BYTES, "..");
        s_entries[n++].is_dir = true;
    }
    if (f_opendir(&dir, s_real) == FR_OK) {
        while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != '\0') {
            if (!listable(info) || n >= capacity) continue;
            snprintf(s_entries[n].name, NAME_BYTES, "%s", info.fname);
            s_entries[n++].is_dir = (info.fattrib & AM_DIR) != 0;
        }
        (void)f_closedir(&dir);
    }
    s_count = n;
    qsort(s_entries + parent, s_count - parent, sizeof(entry), compare);
    if (s_list != nullptr) {
        td_list_set_count(s_list, s_count);
        td_list_select(s_list, 0);
    }
    say(s_count == parent ? "No pictures here" : "");
}

void descend(const char *name)
{
    char next[TD_PATH_MAX];
    if (strcmp(name, "..") == 0) {
        if (at_root()) return;
        snprintf(next, sizeof next, "%s", s_shown);
        char *slash = strrchr(next, '/');
        if (slash != nullptr && slash != next) *slash = '\0';
    } else if (snprintf(next, sizeof next, "%s/%s", s_shown, name) >= (int)sizeof next) {
        say("That path is too long");
        return;
    }
    set_path(next);
    load();
}

const char *item(td_widget_t *, int index, int *fg, void *)
{
    static char text[NAME_BYTES + 2];
    if (index < 0 || index >= s_count) return "";
    if (s_entries[index].is_dir) {
        *fg = td_theme()->accent;
        snprintf(text, sizeof text, "%s/", s_entries[index].name);
        return strcmp(s_entries[index].name, "..") == 0 ? ".." : text;
    }
    return s_entries[index].name;
}

/* ------------------------------------------------------------ the render */

inline uint16_t rgb565(uint32_t r, uint32_t g, uint32_t b)
{
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/* Draw the requested view.  The picture is fitted to the plane at 1x and
 * scaled by the zoom; s is the plane pixels per picture pixel in 16.16. */
void render(const char *path, int zoom, int32_t cx, int32_t cy, uint32_t gen)
{
    static uint16_t out[TANG_PLANE_MAX_WIDTH];
    static uint8_t sent[(TANG_PLANE_MAX_HEIGHT + 7) / 8];
    const int32_t PW = s_pw, PH = s_ph;
    td_image_t img;
    char err[64];
    if (!td_image_load(&img, td_sysinfo()->fs, path, err, (int)sizeof err)) {
        snprintf(s_render_say, sizeof s_render_say, "%s", err);
        s_render_said = true;
        return;
    }
    const int32_t iw = img.w, ih = img.h;
    const uint64_t fit = (uint64_t)65536 * PW / (uint64_t)iw <
                                 (uint64_t)65536 * PH / (uint64_t)ih
                             ? (uint64_t)65536 * PW / (uint64_t)iw
                             : (uint64_t)65536 * PH / (uint64_t)ih;
    const uint64_t s = fit * (uint64_t)zoom;
    int32_t vw = (int32_t)(((uint64_t)PW << 16) / s);
    int32_t vh = (int32_t)(((uint64_t)PH << 16) / s);
    if (vw > iw) vw = iw;
    if (vh > ih) vh = ih;
    if (vw < 1) vw = 1;
    if (vh < 1) vh = 1;
    int32_t dw = (int32_t)(((uint64_t)vw * s) >> 16);
    int32_t dh = (int32_t)(((uint64_t)vh * s) >> 16);
    if (dw > PW) dw = PW;
    if (dh > PH) dh = PH;
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    if (cx < 0) cx = iw / 2;
    if (cy < 0) cy = ih / 2;
    int32_t sx0 = cx - vw / 2, sy0 = cy - vh / 2;
    if (sx0 > iw - vw) sx0 = iw - vw;
    if (sy0 > ih - vh) sy0 = ih - vh;
    if (sx0 < 0) sx0 = 0;
    if (sy0 < 0) sy0 = 0;
    const int32_t ox = (PW - dw) / 2, oy = (PH - dh) / 2;
    s_iw = iw; s_ih = ih; s_vw = vw; s_vh = vh; s_sx0 = sx0; s_sy0 = sy0;
    s_ox = ox; s_oy = oy; s_dw = dw; s_dh = dh;
    s_cx = sx0 + vw / 2;
    s_cy = sy0 + vh / 2;

    const bool down = s <= 65536;       // 1:1 or smaller: average; larger: repeat
    uint8_t *row = static_cast<uint8_t *>(malloc((size_t)iw * 3));
    uint32_t *acc = static_cast<uint32_t *>(calloc((size_t)dw * 3, sizeof(uint32_t)));
    uint16_t *cnt = static_cast<uint16_t *>(calloc((size_t)dw, sizeof(uint16_t)));
    if (row == nullptr || acc == nullptr || cnt == nullptr) {
        free(row); free(acc); free(cnt);
        td_image_close(&img);
        snprintf(s_render_say, sizeof s_render_say, "Not enough memory for a %ld-wide picture", (long)iw);
        s_render_said = true;
        return;
    }
    memset(sent, 0, sizeof sent);
    const uint64_t inv = ((uint64_t)1 << 32) / s;     // picture pixels per plane pixel, 16.16
    int32_t cur = -1;

    /* Each row covers this picture's columns and the last one's. */
    const int32_t L = ox < s_lit_l ? ox : s_lit_l;
    const int32_t R = ox + dw > s_lit_r ? ox + dw : (s_lit_r > PW ? PW : s_lit_r);
    auto send = [&](int32_t ty, const uint16_t *px) {
        const int32_t py = oy + ty;
        for (int32_t x = L; x < R; x++) out[x] = 0;
        memcpy(out + ox, px, (size_t)dw * 2);
        (void)tang_plane_write(L, py, out + L, R - L);
        sent[py >> 3] |= (uint8_t)(1u << (py & 7));
    };
    static uint16_t line[TANG_PLANE_MAX_WIDTH];
    auto flush = [&](void) {
        if (cur < 0) return;
        for (int32_t tx = 0; tx < dw; tx++) {
            const uint32_t n = cnt[tx] ? cnt[tx] : 1;
            line[tx] = rgb565(acc[3 * tx] / n, acc[3 * tx + 1] / n, acc[3 * tx + 2] / n);
        }
        send(cur, line);
        memset(acc, 0, (size_t)dw * 3 * sizeof(uint32_t));
        memset(cnt, 0, (size_t)dw * sizeof(uint16_t));
        cur = -1;
    };

    int y = 0;
    while (td_image_row(&img, &y, row)) {
        if (s_gen != gen) break;
        if (y < sy0 || y >= sy0 + vh) continue;
        const int32_t r = y - sy0;
        int32_t lo = (int32_t)(((uint64_t)r * s) >> 16);
        int32_t hi = (int32_t)((((uint64_t)r + 1) * s) >> 16) - 1;
        if (hi < lo) hi = lo;
        if (lo >= dh) continue;
        if (hi >= dh) hi = dh - 1;
        if (down) {
            if (lo != cur) {
                flush();
                cur = lo;
            }
            for (int32_t x = sx0; x < sx0 + vw; x++) {
                int32_t tx = (int32_t)(((uint64_t)(x - sx0) * s) >> 16);
                if (tx >= dw) tx = dw - 1;
                acc[3 * tx]     += row[3 * x];
                acc[3 * tx + 1] += row[3 * x + 1];
                acc[3 * tx + 2] += row[3 * x + 2];
                cnt[tx]++;
            }
        } else {
            flush();
            for (int32_t tx = 0; tx < dw; tx++) {
                int32_t x = sx0 + (int32_t)(((uint64_t)tx * inv) >> 16);
                if (x >= iw) x = iw - 1;
                line[tx] = rgb565(row[3 * x], row[3 * x + 1], row[3 * x + 2]);
            }
            for (int32_t ty = lo; ty <= hi; ty++) {
                send(ty, line);
            }
        }
    }
    if (s_gen == gen) {
        flush();
        /* Every row the picture did not reach -- the margins above and below,
         * and any a rounding skipped -- is cleared, so nothing of the last
         * picture is left on the plane. */
        for (int32_t x = L; x < R; x++) out[x] = 0;
        for (int32_t py = 0; py < PH && s_gen == gen; py++) {
            if (!(sent[py >> 3] & (1u << (py & 7)))) {
                (void)tang_plane_write(L, py, out + L, R - L);
            }
        }
    }
    /* Finished, only this picture's columns can be lit; abandoned part way,
     * any of them can. */
    if (s_gen == gen) {
        s_lit_l = ox;
        s_lit_r = ox + dw;
    } else {
        s_lit_l = L;
        s_lit_r = R;
    }
    free(row); free(acc); free(cnt);
    td_image_close(&img);
}

void render_task(void *)
{
    for (;;) {
        const uint32_t gen = s_gen;
        char path[sizeof s_view_real];
        snprintf(path, sizeof path, "%s", s_view_real);
        const int zoom = s_zoom;
        const int32_t cx = s_cx, cy = s_cy;
        if (!s_viewing) break;
        render(path, zoom, cx, cy, gen);
        if (s_gen == gen) break;
    }
    s_render_busy = false;
    vTaskDelete(NULL);
}

/* Ask for the current view to be drawn: a running render is abandoned and
 * restarts with the new request; otherwise a task is started. */
void request(void)
{
    s_gen = s_gen + 1;
    if (s_render_busy) return;
    s_render_busy = true;
    if (xTaskCreate(render_task, "reader", RENDER_STACK_WORDS, nullptr, tskIDLE_PRIORITY + 1,
                    nullptr) != pdPASS) {
        s_render_busy = false;
        say("Not enough memory to draw");
    }
}

/* ------------------------------------------------------------ the view */

bool open_index(int index)
{
    if (index < 0 || index >= s_count || s_entries[index].is_dir) return false;
    if (snprintf(s_view_real, sizeof s_view_real, "%s/%s", s_real, s_entries[index].name) >=
        (int)sizeof s_view_real) {
        say("That path is too long");
        return false;
    }
    s_view_index = index;
    s_zoom = 1;
    s_cx = s_cy = -1;
    return true;
}

void view_enter(int index)
{
    if (!tang_plane_available()) {
        say("This desktop core has no pixel plane (needs ABI 1.8)");
        return;
    }
    if (!open_index(index)) return;
    int w = 0, h = 0;
    if (tang_plane_size(&w, &h)) {
        s_pw = w;
        s_ph = h;
    }
    s_lit_l = 0;
    s_lit_r = s_pw;                     // whatever the plane holds is unknown
    s_viewing = true;
    td_win_set_fullscreen(s_win, true);
    if (tang_plane_show(true) == 0) s_plane_on = true;
    request();
}

void view_leave(void)
{
    s_viewing = false;
    s_gen = s_gen + 1;              // the render stops at its next row
    if (s_win != nullptr) td_win_set_fullscreen(s_win, false);
    /* The plane is hidden by the tick once the render has stopped, so a row
     * still in flight cannot land after the screen was handed back. */
}

void step(int dir)
{
    for (int i = s_view_index + dir; i >= 0 && i < s_count; i += dir) {
        if (!s_entries[i].is_dir) {
            open_index(i);
            if (s_list != nullptr) td_list_select(s_list, i);
            request();
            return;
        }
    }
}

void zoom_at(int z, int32_t cx, int32_t cy)
{
    if (z < 1) z = 1;
    if (z > MAX_ZOOM) z = MAX_ZOOM;
    if (z == s_zoom && cx == s_cx && cy == s_cy) return;
    s_zoom = z;
    s_cx = cx;
    s_cy = cy;
    request();
}

void pan(int dx, int dy)
{
    const int32_t sx = s_vw / 4 > 0 ? s_vw / 4 : 1, sy = s_vh / 4 > 0 ? s_vh / 4 : 1;
    zoom_at(s_zoom, s_cx + dx * sx, s_cy + dy * sy);
}

/* The screen is 80x45 cells, so a cell is 8x8 plane pixels on the 640x360
 * plane and 16x16 on the 1280x720 one; this is the picture pixel under one. */
void cell_to_picture(int cell_x, int cell_y, int32_t *px, int32_t *py)
{
    const int32_t cw = s_pw / 80, ch = s_ph / 45;
    const int32_t x = cell_x * cw + cw / 2 - s_ox, y = cell_y * ch + ch / 2 - s_oy;
    if (x < 0 || y < 0 || x >= s_dw || y >= s_dh || s_dw <= 0 || s_dh <= 0) {
        *px = s_cx;
        *py = s_cy;
        return;
    }
    *px = s_sx0 + (int32_t)((int64_t)x * s_vw / s_dw);
    *py = s_sy0 + (int32_t)((int64_t)y * s_vh / s_dh);
}

bool on_event(td_window_t *, const td_event_t *ev)
{
    if (!s_viewing) return false;
    if (ev->type == TD_EV_KEY) {
        switch (ev->key) {
        case TD_KEY_ESC: case 'q': case TD_KEY_BACKSPACE: view_leave(); return true;
        case '+': case '=': zoom_at(s_zoom + 1, s_cx, s_cy); return true;
        case '-': case '_': zoom_at(s_zoom - 1, s_cx, s_cy); return true;
        case TD_KEY_HOME: case '0': zoom_at(1, -1, -1); return true;
        case TD_KEY_LEFT: pan(-1, 0); return true;
        case TD_KEY_RIGHT: pan(1, 0); return true;
        case TD_KEY_UP: pan(0, -1); return true;
        case TD_KEY_DOWN: pan(0, 1); return true;
        case TD_KEY_PGDN: case ' ': case 'n': step(1); return true;
        case TD_KEY_PGUP: case 'p': case 'b': step(-1); return true;
        default: return true;           // a full-screen app swallows the rest
        }
    }
    if (ev->type == TD_EV_MOUSE && ev->action == TD_MOUSE_PRESS) {
        if (ev->button == 0) {
            int32_t px, py;
            cell_to_picture(ev->x, ev->y, &px, &py);
            zoom_at(s_zoom + 1, px, py);
        } else if (ev->button == 2) {
            if (s_zoom <= 1) view_leave();
            else zoom_at(s_zoom - 1, s_cx, s_cy);
        } else if (ev->button == 4) {
            pan(0, -1);
        } else if (ev->button == 5) {
            pan(0, 1);
        }
        return true;
    }
    return ev->type == TD_EV_MOUSE;
}

/* ------------------------------------------------------------ the window */

void on_activate(td_widget_t *, void *)
{
    const int i = s_list != nullptr ? td_list_selected(s_list) : -1;
    if (i < 0 || i >= s_count) return;
    if (s_entries[i].is_dir) descend(s_entries[i].name);
    else view_enter(i);
}

void on_up(td_widget_t *, void *) { descend(".."); }
void on_refresh(td_widget_t *, void *) { load(); }

void on_tick(td_window_t *)
{
    if (!s_viewing && !s_render_busy && s_plane_on) {
        if (tang_plane_show(false) == 0) s_plane_on = false;
    }
    if (s_render_said) {
        s_render_said = false;
        say(s_render_say);
    }
}

void on_close(td_window_t *)
{
    s_viewing = false;
    s_gen = s_gen + 1;
    /* The window is going; the render stops at its next row and the plane
     * must not outlive the app that owned it.  If the link is busy for this
     * one try, the tick of a reopened Reader hides it. */
    if (s_plane_on && tang_plane_show(false) == 0) s_plane_on = false;
    s_win = nullptr;
    s_list = nullptr;
    s_path = nullptr;
    s_status = nullptr;
    free(s_entries);
    s_entries = nullptr;
    s_count = 0;
}

void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }
    td_window_desc_t d = {};
    d.title = "Reader";
    d.rect = td_rect(-1, -1, WIN_W, WIN_H);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE;
    d.min_w = WIN_W;
    d.min_h = WIN_H;
    d.on_event = on_event;
    d.on_close = on_close;
    d.on_tick = on_tick;
    d.tick_ms = TICK_MS;
    d.icon = "RD";
    s_win = td_win_create(&d);
    if (s_win == nullptr) return;

    set_path(ROOT);
    s_path = td_label(s_win, 1, 0, 0, "");
    s_list = td_list(s_win, td_rect(1, 1, -1, LIST_ROWS), item, on_activate, nullptr);
    td_scrollbar(s_win, -1, 1, LIST_ROWS, s_list);
    td_button(s_win, 1, 15, "Open", on_activate, nullptr);
    td_button(s_win, 10, 15, "Up", on_up, nullptr);
    td_button(s_win, 17, 15, "Refresh", on_refresh, nullptr);
    s_status = td_label(s_win, 1, 17, 0, "");
    load();
    if (!tang_plane_available()) {
        say("No pixel plane on this core (desktop ABI 1.8)");
    }
}

const td_app_t s_app = { "Reader", launch, "RD" };

} // namespace

extern "C" void td_reader_register(void)
{
    (void)td_app_register(&s_app);
}
