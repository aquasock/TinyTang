// TinyNES Controls -- the window that changes how a game behaves.
//
// Opened from the start menu or the desktop, over a running game, and driven
// with F12 the way the desktop is: the game keeps running behind it.  The
// settings live in /nes.ini at the card root (ports/bl616/tang_nes_ini.c) and
// the reader parses them the same way for this app and for a launch, so what
// is shown here is what a launch will use.
//
// This is the skeleton: the tabs, and every value displayed.  Nothing here
// changes a setting yet -- button remapping is the first that will.
//
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The TinyDesk and shell headers are C, and their symbols are unmangled, so
// they are included with C linkage as the Phosphor app does it.
extern "C" {
#include "td_apps.h"
#include "tinydesk/td.h"
#include "tdsh.h"
}

#include "tang_nes_ini.h"

namespace {

const int TICK_MS = 400;
const int TABS = 4;
const int PAGE_WIDGETS = 10;

/* There is no named accent, only TD_COLOR_DEFAULT and raw palette indices. */
const int ACCENT = 14;

const char *const TAB_NAMES[TABS] = { "Input", "Video", "Audio", "Saves" };

td_window_t *s_win;
td_widget_t *s_tab[TABS];
td_widget_t *s_page[TABS][PAGE_WIDGETS];
int s_page_count[TABS];
int s_tab_shown = -1;

td_widget_t *s_list;          /* the Input page's rows */
int s_player;                 /* which player the Input page is showing */
tang_nes_settings_t s_set;

/* Register a widget as part of a page, so the tab switch can show or hide
 * the whole page at once. */
void page_add(int tab, td_widget_t *w)
{
    if (w != nullptr && s_page_count[tab] < PAGE_WIDGETS) {
        s_page[tab][s_page_count[tab]++] = w;
    }
}

void show_tab(int tab)
{
    if (tab == s_tab_shown) {
        return;
    }
    for (int t = 0; t < TABS; t++) {
        for (int i = 0; i < s_page_count[t]; i++) {
            td_widget_set_visible(s_page[t][i], t == tab);
        }
        td_widget_set_color(s_tab[t], TD_COLOR_DEFAULT,
                            t == tab ? ACCENT : TD_COLOR_DEFAULT);
    }
    s_tab_shown = tab;
}

void on_tab(td_widget_t *, void *user)
{
    show_tab((int)(uintptr_t)user);
}

void on_player(td_widget_t *, void *)
{
    s_player = s_player ? 0 : 1;
    if (s_list != nullptr) {
        td_widget_set_text(s_list, s_player ? "Player 2" : "Player 1");
    }
}

/* One row of the Input page: the NES button and what it answers to. */
const char *input_item(td_widget_t *, int index, int *fg, void *)
{
    static char text[48];
    const uint8_t *map;

    (void)fg;
    if (index < 0 || index >= TANG_NES_BUTTONS) {
        return "";
    }
    map = s_player ? s_set.p2 : s_set.p1;
    snprintf(text, sizeof text, "%-8s %s", tang_nes_button_name(index),
             tang_nes_pad_name(map[index]));
    return text;
}

void on_close(td_window_t *)
{
    s_win = nullptr;
    s_list = nullptr;
    s_tab_shown = -1;
}

void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }
    if (!s_set.found) {
        (void)tang_nes_load(&s_set, TANG_NES_INI_REAL);
    }

    td_window_desc_t d = {};
    d.title = "TinyNES Controls";
    d.rect = td_rect(-1, -1, 66, 22);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE | TD_WIN_RESIZABLE;
    d.min_w = 50;
    d.min_h = 14;
    d.on_close = on_close;
    d.tick_ms = TICK_MS;
    d.icon = "NES";
    s_win = td_win_create(&d);
    if (s_win == nullptr) {
        return;
    }
    memset(s_page_count, 0, sizeof s_page_count);

    /* The tab headers. */
    for (int t = 0; t < TABS; t++) {
        s_tab[t] = td_button(s_win, 1 + t * 9, 0, TAB_NAMES[t], on_tab,
                             (void *)(uintptr_t)t);
    }

    /* Input: whose mapping is shown, the eight rows, and the actions. */
    page_add(0, td_button(s_win, 1, 2, "Player 1", on_player, nullptr));
    s_list = td_list(s_win, td_rect(1, 4, 34, TANG_NES_BUTTONS), input_item,
                     nullptr, nullptr);
    if (s_list != nullptr) {
        td_list_set_count(s_list, TANG_NES_BUTTONS);
    }
    page_add(0, s_list);
    page_add(0, td_button(s_win, 38, 4, "Remap", nullptr, nullptr));
    page_add(0, td_button(s_win, 38, 6, "Restore defaults", nullptr, nullptr));
    page_add(0, td_label(s_win, 38, 8, 0, "Remapping is next;"));
    page_add(0, td_label(s_win, 38, 9, 0, "nothing here yet."));

    /* Video and audio: what /nes.ini asked for. */
    char text[64];
    snprintf(text, sizeof text, "Aspect      %s",
             tang_nes_aspect_name(s_set.aspect));
    page_add(1, td_label(s_win, 1, 2, 0, text));
    snprintf(text, sizeof text, "Scale       %ux", (unsigned)s_set.scale);
    page_add(1, td_label(s_win, 1, 3, 0, text));
    snprintf(text, sizeof text, "Scanlines   %s", s_set.scanlines ? "on" : "off");
    page_add(1, td_label(s_win, 1, 4, 0, text));
    snprintf(text, sizeof text, "Overscan    %s", s_set.overscan ? "hide" : "show");
    page_add(1, td_label(s_win, 1, 5, 0, text));
    page_add(1, td_label(s_win, 1, 7, 0, "Nothing here reaches the video"));
    page_add(1, td_label(s_win, 1, 8, 0, "unit yet."));

    snprintf(text, sizeof text, "Volume      %u%%", (unsigned)s_set.volume);
    page_add(2, td_label(s_win, 1, 2, 0, text));
    page_add(2, td_label(s_win, 1, 3, 0,
                         s_set.mute ? "Mute        yes" : "Mute        no"));
    page_add(2, td_label(s_win, 1, 4, 0,
                         s_set.channels == TANG_CHANNELS_MONO
                             ? "Channels    mono" : "Channels    stereo"));
    page_add(2, td_label(s_win, 1, 6, 0, "Output      48,909 Hz to the"));
    page_add(2, td_label(s_win, 1, 7, 0, "            Pmod I2S2"));

    page_add(3, td_label(s_win, 1, 2, 0, "Battery saves are in /root/saves/nes"));
    page_add(3, td_label(s_win, 1, 3, 0, "and written when the game ends."));
    page_add(3, td_label(s_win, 1, 5, 0, "Save states are not implemented,"));
    page_add(3, td_label(s_win, 1, 6, 0, "so this page is a placeholder."));

    /* The file this all comes from, and whether it was there. */
    snprintf(text, sizeof text, "%s %s, %u key(s)",
             TANG_NES_INI_SHOWN, s_set.found ? "read" : "absent",
             (unsigned)s_set.keys);
    page_add(0, td_label(s_win, 1, 14, 0, text));
    page_add(1, td_label(s_win, 1, 14, 0, text));
    page_add(2, td_label(s_win, 1, 14, 0, text));
    page_add(3, td_label(s_win, 1, 14, 0, text));

    show_tab(0);
}

const td_app_t s_app = { "TinyNES Controls", launch, "NES" };

} // namespace

// C linkage: the desktop's init file is C, and calls this by name.
extern "C" void td_tinynes_register(void)
{
    (void)td_app_register(&s_app);
}
