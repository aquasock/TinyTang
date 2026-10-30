// TinyTang — the TinyNode window: four relay outputs and the node's sensors.
//
// SPDX-License-Identifier: MIT
//
// The node answers `relay <0000>` to set its four channels and `status` with a
// block that carries every sensor it has -- GPS fix, magnetometer vector and
// heading, temperature and humidity, the PPS measure, the radio's counters and
// the relay states.  Both travel as application messages over the RYLR998 link
// (tang_proto.h), so this window is only a control surface: the digits and the
// block are what the node already speaks.
//
// Every request is a radio round trip and the link loses requests, so one runs
// in a task of its own while the desktop keeps drawing, and a click made while
// one is out is refused rather than queued.  The window's budget is three
// attempts of four seconds, shorter than the console's patient ten, because a
// control surface that locks up for a minute is worse than one that says it
// failed.
//
// The relay digits follow the cable, not the header: 1 is VBUS, 2 is D-, 3 is
// D+ and 4 is GND, which is the order the node's relay command uses.

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

extern "C" {
#include "tinydesk/td.h"
#include "tang_node.h"
#include "tang_proto.h"
}

namespace {

constexpr int WIN_W = 70;
constexpr int WIN_H = 26;
constexpr int TICK_MS = 250;

constexpr int      ATTEMPTS = 3;
constexpr uint32_t REPLY_MS = 4000;
constexpr int      STACK_WORDS = 1024;
constexpr int      RELAYS = 4;

constexpr int LINES = 14;        // status rows the list can show
constexpr int LINE_CHARS = 72;   // the status reply's longest line, cut to fit

enum { REQ_NONE = 0, REQ_STATUS, REQ_RELAY };

td_window_t *s_win;
td_widget_t *s_box[RELAYS];
td_widget_t *s_update;
td_widget_t *s_note;
td_widget_t *s_list;

char s_lines[LINES][LINE_CHARS];
int  s_line_count;
char s_note_text[72];

uint8_t s_known_mask;   // the mask the node last confirmed
bool    s_busy;         // a request is on the link
bool    s_applying;     // the window is setting the boxes, not the user
bool    s_dirty;        // the worker has a result for the next tick

int     s_req;
int     s_done_kind;    // what the last finished request was
uint8_t s_req_mask;
char    s_reply[TANG_PROTO_MAX_BODY + 1];
bool    s_reply_ok;

const char *const s_box_fmt[RELAYS] = {
    "Relay 1 - VBUS", "Relay 2 - D-", "Relay 3 - D+", "Relay 4 - GND",
};

/* ------------------------------------------------------------------ helpers */

void digits(uint8_t mask, char out[RELAYS + 1])
{
    for (int i = 0; i < RELAYS; ++i) {
        out[i] = ((mask >> i) & 1u) != 0 ? '1' : '0';
    }
    out[RELAYS] = '\0';
}

void note(const char *text)
{
    snprintf(s_note_text, sizeof(s_note_text), "%s", text != NULL ? text : "");
    if (s_note != NULL) {
        td_widget_set_text(s_note, s_note_text);
    }
}

void apply_boxes(uint8_t mask)
{
    s_applying = true;
    for (int i = 0; i < RELAYS; ++i) {
        if (s_box[i] != NULL) {
            td_checkbox_set(s_box[i], ((mask >> i) & 1u) != 0);
        }
    }
    s_applying = false;
}

/* The reply's lines, cut to what a row can show. */
void split_reply(const char *text)
{
    s_line_count = 0;
    const char *p = text;
    while (*p != '\0' && s_line_count < LINES) {
        const char *nl = strchr(p, '\n');
        size_t len = (nl != NULL) ? (size_t)(nl - p) : strlen(p);
        if (len >= (size_t)LINE_CHARS) {
            len = (size_t)LINE_CHARS - 1;
        }
        memcpy(s_lines[s_line_count], p, len);
        s_lines[s_line_count][len] = '\0';
        s_line_count++;
        if (nl == NULL) {
            break;
        }
        p = nl + 1;
    }
    if (s_list != NULL) {
        td_list_set_count(s_list, s_line_count);
    }
}

/* "relay 1010" among the status lines: the states the node reports. */
bool relay_mask_from_reply(uint8_t *mask)
{
    for (int i = 0; i < s_line_count; ++i) {
        if (strncmp(s_lines[i], "relay ", 6) != 0) {
            continue;
        }
        const char *d = s_lines[i] + 6;
        uint8_t m = 0;
        int n = 0;
        for (; n < RELAYS && (d[n] == '0' || d[n] == '1'); ++n) {
            if (d[n] == '1') {
                m = (uint8_t)(m | (1u << n));
            }
        }
        if (n == RELAYS) {
            *mask = m;
            return true;
        }
    }
    return false;
}

/* --------------------------------------------------------------- the worker */

/* One request, then the task ends.  The reply is left in s_reply for the next
 * tick, which is where the window reads it -- this task never touches a
 * widget, because the desktop's drawing happens on another thread. */
void request_task(void *arg)
{
    (void)arg;
    char body[16];
    int len = 0;
    uint16_t used = 0;

    if (s_req == REQ_RELAY) {
        char d[RELAYS + 1];
        digits(s_req_mask, d);
        len = snprintf(body, sizeof(body), "relay %s", d);
        if (len > 0) {
            used = (uint16_t)len;
        }
    }

    const int got = tang_node_ask(s_req == REQ_STATUS ? TANG_REQ_STATUS : TANG_REQ_COMMAND,
                                  s_req == REQ_RELAY ? (const uint8_t *)body : NULL,
                                  used, s_reply, sizeof(s_reply),
                                  ATTEMPTS, REPLY_MS);

    s_reply_ok = got >= 0;
    if (s_reply_ok && s_req == REQ_RELAY) {
        /* Anything but the node's confirmation means the relays did not take
         * the states that were asked for. */
        s_reply_ok = strncmp(s_reply, "relay states updated", 20) == 0;
    }
    s_done_kind = s_req;
    s_req = REQ_NONE;
    s_dirty = true;
    vTaskDelete(NULL);
}

void start(int kind, uint8_t mask)
{
    if (s_busy) {
        apply_boxes(s_known_mask);   /* the box the user moved goes back */
        note("still waiting for the node");
        return;
    }
    s_req = kind;
    s_req_mask = mask;
    s_busy = true;
    note(kind == REQ_STATUS ? "updating ..." : "setting the relays ...");
    if (xTaskCreate(request_task, "tinynode", STACK_WORDS, NULL,
                    tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        s_busy = false;
        s_req = REQ_NONE;
        note("not enough memory to ask the node");
    }
}

/* ---------------------------------------------------------------- the window */

void on_tick(td_window_t *win)
{
    if (!s_dirty) {
        return;
    }
    s_dirty = false;
    s_busy = false;

    if (!s_reply_ok) {
        /* A refused set leaves the boxes showing states the node does not
         * have, so they go back to the last ones it confirmed. */
        if (s_done_kind == REQ_RELAY) {
            apply_boxes(s_known_mask);
        }
        char line[LINE_CHARS];
        snprintf(line, sizeof(line), "%s", s_reply[0] != '\0' ? s_reply : "no reply from the node");
        char *nl = strchr(line, '\n');
        if (nl != NULL) {
            *nl = '\0';
        }
        note(line);
        td_win_invalidate(win);
        return;
    }

    if (s_done_kind == REQ_RELAY) {
        note("relay states updated");
    } else {
        split_reply(s_reply);
        uint8_t mask = 0;
        if (relay_mask_from_reply(&mask)) {
            s_known_mask = mask;
            apply_boxes(mask);
        }
        note("updated");
    }
    td_win_invalidate(win);
}

void on_box(td_widget_t *w, void *user)
{
    (void)w;
    if (s_applying) {
        return;                      /* our own doing, not a click */
    }
    (void)user;
    uint8_t mask = 0;
    for (int i = 0; i < RELAYS; ++i) {
        if (s_box[i] != NULL && td_checkbox_get(s_box[i])) {
            mask = (uint8_t)(mask | (1u << i));
        }
    }
    if (mask == s_known_mask && !s_busy) {
        return;
    }
    start(REQ_RELAY, mask);
}

void on_update(td_widget_t *w, void *user)
{
    (void)w;
    (void)user;
    start(REQ_STATUS, 0);
}

void on_close(td_window_t *win)
{
    (void)win;
    s_win = NULL;
    for (int i = 0; i < RELAYS; ++i) {
        s_box[i] = NULL;
    }
    s_update = NULL;
    s_note = NULL;
    s_list = NULL;
    s_line_count = 0;
}

const char *get_line(td_widget_t *w, int index, int *fg, void *user)
{
    (void)w;
    (void)fg;
    (void)user;
    if (index < 0 || index >= s_line_count) {
        return "";
    }
    return s_lines[index];
}

void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }

    td_window_desc_t d = {0};
    d.title = "TinyNode";
    d.rect = td_rect(-1, -1, WIN_W, WIN_H);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE;
    d.on_tick = on_tick;
    d.tick_ms = TICK_MS;
    d.on_close = on_close;
    s_win = td_win_create(&d);
    if (s_win == NULL) {
        return;
    }

    td_label(s_win, 1, 1, 0, "Relays - the digits follow the cable:");
    for (int i = 0; i < RELAYS; ++i) {
        s_box[i] = td_checkbox(s_win, 3, 2 + i, s_box_fmt[i], false, on_box, NULL);
    }
    s_update = td_button(s_win, 3, 7, "Update", on_update, NULL);
    s_note = td_label(s_win, 13, 7, 0, "");
    td_widget_set_color(s_note, td_theme()->dim, TD_COLOR_DEFAULT);

    td_label(s_win, 1, 9, 0, "Sensors, from the node's status:");
    s_list = td_list(s_win, td_rect(0, 10, -1, LINES), get_line, NULL, NULL);
    td_list_set_count(s_list, 0);

    s_known_mask = 0;
    s_busy = false;
    s_dirty = false;
    s_req = REQ_NONE;
    s_done_kind = REQ_NONE;
    s_line_count = 0;
    note("press Update to read the node");
    apply_boxes(s_known_mask);
}

/* The desktop's start menu lists this by name; patch TDESK-015 style entries
 * are not needed for it, since it is this port's own app. */
const td_app_t s_app = {"TinyNode", launch, "TN"};

}  // namespace

extern "C" void td_tinynode_register(void)
{
    (void)td_app_register(&s_app);
}
