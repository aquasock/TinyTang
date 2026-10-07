// TinyTang — the Bluetooth window: the radio and the paired keyboard and mouse.
//
// TinyDesk's Settings has a Network button for its own network app, which
// this port leaves out (TDESK-008).  The carried patch
// third_party/patches/tinydesk/0001-settings-bluetooth-button.patch makes that
// button "Bluetooth..." and launches the app registered here by that name.
//
// This first version only shows status: what `ble` prints, read through
// tang_ble_info(), which never blocks and starts nothing, so opening the
// window does not start the radio.  Pairing, forgetting and turning
// reconnection off and on stay with the shell's blekbd and blemouse commands,
// which the window names.
//
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <string.h>

#include "tinydesk/td.h"
#include "tang_ble.h"
#include "tang_heap.h"

#define WIN_W    66
#define WIN_H    21   /* every row filled, the footer and Close */
#define TICK_MS  1000
#define COL      12     /* where values start, after the headings */

static td_window_t *s_win;
static tang_ble_info_t s_info;
static tang_heap_info_t s_heap;

static const char *const s_slot_names[TANG_BOND_SLOTS] = {"Keyboard", "Mouse"};
static const char *const s_slot_cmds[TANG_BOND_SLOTS] = {"blekbd", "blemouse"};

static void refresh(void)
{
    tang_ble_info(&s_info);
    tang_heap_info(&s_heap);
}

/* An address as the shell prints it: most significant byte first. */
static void format_addr(char *out, size_t n, const uint8_t a[6])
{
    snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
}

static void row(int y, int w, const char *heading, const char *value, uint8_t fg)
{
    const td_theme_t *t = td_theme();
    if (heading != NULL) {
        td_text(1, y, heading, t->win_fg, t->win_bg, TD_BOLD);
    }
    td_textn(COL, y, value, w - COL - 1, fg, t->win_bg, 0);
}

static int draw_slot(int y, int w, int i)
{
    const td_theme_t *t = td_theme();
    const tang_ble_slot_info_t *s = &s_info.slot[i];
    char line[96];
    char addr[18];

    if (s->active) {
        format_addr(addr, sizeof(addr), s->addr);
        snprintf(line, sizeof(line), "%s, %s%s%s", s->state, addr,
                 s->name[0] ? " " : "", s->name);
    } else {
        snprintf(line, sizeof(line), "%s", s->state);
    }
    row(y++, w, s_slot_names[i], line, s->ready ? t->win_fg : t->dim);

    if (s->ready) {
        snprintf(line, sizeof(line), "%s protocol, %lu report(s)",
                 s->boot_protocol ? "boot" : "report", (unsigned long)s->reports);
        row(y++, w, NULL, line, t->win_fg);
    }

    if (s->paired) {
        format_addr(addr, sizeof(addr), s->paired_addr);
        snprintf(line, sizeof(line), "paired with %s%s%s", addr,
                 s->paired_name[0] ? " " : "", s->paired_name);
        row(y++, w, NULL, line, t->win_fg);
        snprintf(line, sizeof(line), "%s",
                 s->reconnects ? "reconnects by itself"
                               : "not reconnecting");
        row(y++, w, NULL, line, s->reconnects ? t->win_fg : t->dim);
    } else {
        row(y++, w, NULL, "not paired", t->dim);
    }
    return y;
}

static void on_draw(td_window_t *win, int w, int h)
{
    (void)win;
    const td_theme_t *t = td_theme();
    char line[96];
    td_fill(td_rect(0, 0, w, h), ' ', t->win_fg, t->win_bg);

    int y = 1;
    switch (s_info.radio) {
    case TANG_BLE_RADIO_OFF:
        row(y++, w, "Radio", "off (starts at boot when a device is paired)", t->dim);
        break;
    case TANG_BLE_RADIO_STARTING:
        row(y++, w, "Radio", "starting", t->win_fg);
        break;
    case TANG_BLE_RADIO_FAILED:
        snprintf(line, sizeof(line), "failed to start (%d)", s_info.enable_error);
        row(y++, w, "Radio", line, t->win_fg);
        break;
    case TANG_BLE_RADIO_UP:
        snprintf(line, sizeof(line), "up, the stack took %lu bytes of heap at start",
                 (unsigned long)s_info.stack_heap);
        row(y++, w, "Radio", line, t->win_fg);
        break;
    }
    snprintf(line, sizeof(line), "%s (/ble/bonds.bin)", s_info.pairings);
    row(y++, w, "Pairings", line, t->win_fg);
    snprintf(line, sizeof(line), "%lu bytes free, largest block %lu",
             (unsigned long)s_heap.free, (unsigned long)s_heap.largest);
    row(y++, w, "Heap", line, t->win_fg);

    for (int i = 0; i < TANG_BOND_SLOTS; i++) {
        y = draw_slot(y + 1, w, i);
    }

    y = h - 4;
    td_text(1, y++, "Pairing and forgetting are done in the Terminal:",
            t->dim, t->win_bg, 0);
    for (int i = 0; i < TANG_BOND_SLOTS; i++) {
        snprintf(line, sizeof(line), "  %s pair | forget | off | on", s_slot_cmds[i]);
        td_text(1, y++, line, t->dim, t->win_bg, 0);
    }
}

static void on_tick(td_window_t *win)
{
    refresh();
    td_win_invalidate(win);
}

static void on_close_btn(td_widget_t *w, void *user)
{
    (void)user;
    td_win_close(w->win);
}

static void on_close(td_window_t *win)
{
    (void)win;
    s_win = NULL;
}

static void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }
    refresh();
    td_window_desc_t d = {0};
    d.title = "Bluetooth";
    d.rect = td_rect(-1, -1, WIN_W, WIN_H);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE;
    d.on_draw = on_draw;
    d.on_close = on_close;
    d.on_tick = on_tick;
    d.tick_ms = TICK_MS;
    s_win = td_win_create(&d);
    if (s_win == NULL) {
        return;
    }
    td_widget_focus(td_button(s_win, 1, -1, "Close", on_close_btn, NULL));
}

static const td_app_t s_app = {"Bluetooth", launch, "B)"};

void td_bluetooth_register(void)
{
    td_app_register(&s_app);
}
