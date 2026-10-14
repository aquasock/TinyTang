// TinyNES Controls -- the window that changes how a game behaves.
//
// Opened from the start menu or the desktop, over a running game, and driven
// with F12 the way the desktop is: the game keeps running behind it.  The
// settings live in /nes.ini at the card root (ports/bl616/tang_nes_ini.c), and
// a launch reads the same file the same way, so what is saved here is what the
// next game start uses.
//
// Only the Input tab does anything yet: it rebinds which controller button
// answers to each NES button.  Video and Audio show what the file says and
// reach nothing.
//
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The TinyDesk and shell headers are C, and their symbols are unmangled, so
// they are included with C linkage as the Phosphor app does it.
extern "C" {
#include "ff.h"
#include "td_apps.h"
#include "tinydesk/td.h"
#include "tdsh.h"
}

#include "tang_nes_ini.h"

namespace {

const int TICK_MS = 400;
const int TABS = 4;
const int PAGE_WIDGETS = 12;
const int PLAYERS = 2;

/* There is no named accent, only TD_COLOR_DEFAULT and raw palette indices. */
const int ACCENT = 14;

/* Where a save is built before it replaces the real file, and where the file
 * being replaced is set aside while that happens.  FatFS's f_rename refuses a
 * name that is already in use (FR_EXIST -- FAT has no atomic replace), so a
 * save cannot be one rename over the settings file and goes through the
 * backup name instead (tools/tests/test_fat_rename.sh). */
const char TMP_PATH[] = "/sd/nes.ini.tmp";
const char BAK_PATH[] = "/sd/nes.ini.bak";

const char *const TAB_NAMES[TABS] = { "Input", "Video", "Audio", "Saves" };

td_window_t *s_win;
td_widget_t *s_tab[TABS];
td_widget_t *s_page[TABS][PAGE_WIDGETS];
int s_page_count[TABS];
int s_tab_shown = -1;

td_widget_t *s_list;             /* the Input page's rows */
td_widget_t *s_player_button;    /* which player the rows are showing */
td_widget_t *s_status[TABS];     /* one per page: a shared widget cannot be
                                  * shown on one page and hidden on another */
int s_player;
bool s_dirty;                    /* edited here and not yet saved */

/* What the app is editing, which starts as the file's and is what Save writes. */
uint8_t s_map[PLAYERS][TANG_NES_BUTTONS];
tang_nes_settings_t s_set;

/* ------------------------------------------------------------------ pages */

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

/* --------------------------------------------------------------- the file */

void set_status(const char *text)
{
    for (int t = 0; t < TABS; t++) {
        if (s_status[t] != nullptr) {
            td_widget_set_text(s_status[t], text);
        }
    }
    if (s_win != nullptr) {
        td_win_invalidate(s_win);
    }
}

void status_from_file(void)
{
    static char text[80];

    snprintf(text, sizeof text, "%s %s, %u key(s)%s", TANG_NES_INI_SHOWN,
             s_set.found ? "read" : "absent", (unsigned)s_set.keys,
             s_dirty ? "  edited" : "");
    set_status(text);
}

/* Read the file, and take its mapping as the one being edited.  Called every
 * time the window opens, so a hand edit shows without a power cycle. */
void load_settings(void)
{
    (void)tang_nes_load(&s_set, TANG_NES_INI_REAL);
    for (int p = 0; p < PLAYERS; p++) {
        memcpy(s_map[p], p == 0 ? s_set.p1 : s_set.p2, TANG_NES_BUTTONS);
    }
    s_dirty = false;
}

bool put(FIL *file, const char *text)
{
    const size_t length = strlen(text);
    UINT wrote = 0;

    return f_write(file, text, (UINT)length, &wrote) == FR_OK && wrote == length;
}

/* Our eight keys per player, one to a line, which the parser accepts as
 * readily as the several-to-a-line the shipped file uses. */
bool put_mapping(FIL *file)
{
    char line[80];

    for (int p = 0; p < PLAYERS; p++) {
        for (int i = 0; i < TANG_NES_BUTTONS; i++) {
            snprintf(line, sizeof line, "p%d_%s = %s\n", p + 1,
                     tang_nes_button_name(i), tang_nes_pad_name(s_map[p][i]));
            if (!put(file, line)) {
                return false;
            }
        }
    }
    return true;
}

bool is_key_line(const char *line)
{
    return line[0] == 'p' && (line[1] == '1' || line[1] == '2') && line[2] == '_';
}

/* Write the mapping where the file's [input] section is, keeping every other
 * byte of it -- a hand-written comment survives a save from here, and a key
 * this build does not know is not thrown away.  Built beside the real file and
 * moved into place through a backup name, so a full card cannot leave half a
 * settings file behind.
 *
 * NULL when it wrote, else a short reason, because "the save did not work" is
 * not enough to act on. */
const char *save_settings(void)
{
    FIL in, out;
    char chunk[64];
    char line[200];
    size_t len = 0;
    bool in_input = false;
    bool done = false;          /* the mapping has been put out */
    bool ok = true;
    UINT got = 0;
    const char *reason = NULL;

    if (f_open(&in, TANG_NES_INI_REAL, FA_READ) != FR_OK) {
        return "cannot read the settings file";
    }
    if (f_open(&out, TMP_PATH, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        (void)f_close(&in);
        return "cannot create the temporary";
    }

    while (ok && f_read(&in, chunk, sizeof chunk, &got) == FR_OK && got != 0) {
        for (UINT i = 0; i < got && ok; i++) {
            if (chunk[i] != '\n') {
                if (len + 1 < sizeof line) {
                    line[len++] = chunk[i];
                }
                continue;
            }
            line[len] = '\0';
            len = 0;
            if (line[0] == '[') {
                /* Leaving one section for another: if the mapping never went
                 * out, the end of [input] is where it goes. */
                if (in_input && !done) {
                    ok = put_mapping(&out);
                    done = true;
                }
                in_input = strncmp(line, "[input]", 7) == 0;
                ok = ok && put(&out, line) && put(&out, "\n");
                if (in_input) {
                    ok = ok && put_mapping(&out);
                    done = true;
                }
                continue;
            }
            if (in_input && is_key_line(line)) {
                continue;       /* ours, and already out under the header */
            }
            ok = ok && put(&out, line) && put(&out, "\n");
        }
    }
    /* A last line with no newline after it. */
    if (ok && len != 0) {
        line[len] = '\0';
        if (!(in_input && is_key_line(line))) {
            ok = put(&out, line) && put(&out, "\n");
        }
    }
    /* A file with no [input] section at all, as the first save on a fresh card
     * would be. */
    if (ok && !done) {
        ok = put(&out, "\n[input]\n") && put_mapping(&out);
    }
    if (reason == NULL && !ok) {
        reason = "a write to the temporary failed";
    }
    if (f_close(&out) != FR_OK && reason == NULL) {
        reason = "the temporary did not close";
    }
    (void)f_close(&in);

    if (reason != NULL) {
        (void)f_unlink(TMP_PATH);   /* nothing has been replaced yet */
        return reason;
    }

    /* Through the backup name, because FatFS's f_rename refuses a name that is
     * already in use (FR_EXIST -- FAT has no atomic replace), so a single
     * rename over the settings file cannot work.  The file is set aside, the
     * temporary takes its place, the backup is dropped.  A whole file sits
     * under one of those three names at every point, so an interruption costs
     * a stray name and never half a settings file. */
    (void)f_unlink(BAK_PATH);       /* one left by an interrupted save */
    if (f_rename(TANG_NES_INI_REAL, BAK_PATH) != FR_OK) {
        (void)f_unlink(TMP_PATH);
        return "cannot set the settings file aside";
    }
    if (f_rename(TMP_PATH, TANG_NES_INI_REAL) != FR_OK) {
        /* The temporary holds the mapping that would not go in, and the file
         * it was to replace is under the backup name, so neither is dropped. */
        (void)f_rename(BAK_PATH, TANG_NES_INI_REAL);
        return "cannot rename over the settings file";
    }
    (void)f_unlink(BAK_PATH);
    return NULL;
}

/* ------------------------------------------------------------- the input */

/* One row: the NES button and what it answers to.  Activating it steps the
 * mapping to the next controller button -- a capture would need the pad, and
 * the only pad an app can see is the desk layer's seven-bit word. */
const char *input_item(td_widget_t *, int index, int *fg, void *)
{
    static char text[48];

    (void)fg;
    if (index < 0 || index >= TANG_NES_BUTTONS) {
        return "";
    }
    snprintf(text, sizeof text, "%-8s %s%s", tang_nes_button_name(index),
             tang_nes_pad_name(s_map[s_player][index]), s_dirty ? " *" : "");
    return text;
}

void on_row(td_widget_t *, void *)
{
    const int row = s_list != nullptr ? td_list_selected(s_list) : -1;

    if (row < 0 || row >= TANG_NES_BUTTONS) {
        return;
    }
    s_map[s_player][row] = (uint8_t)((s_map[s_player][row] + 1) % NES_CTL_COUNT);
    s_dirty = true;
    status_from_file();
    if (s_win != nullptr) {
        td_win_invalidate(s_win);
    }
}

void on_player(td_widget_t *, void *)
{
    s_player = (s_player + 1) % PLAYERS;
    if (s_player_button != nullptr) {
        td_widget_set_text(s_player_button, s_player ? "Player 2" : "Player 1");
    }
    if (s_win != nullptr) {
        td_win_invalidate(s_win);
    }
}

void on_restore(td_widget_t *, void *)
{
    tang_nes_settings_t fresh;

    tang_nes_defaults(&fresh);
    for (int p = 0; p < PLAYERS; p++) {
        memcpy(s_map[p], p == 0 ? fresh.p1 : fresh.p2, TANG_NES_BUTTONS);
    }
    s_dirty = true;
    status_from_file();
    if (s_win != nullptr) {
        td_win_invalidate(s_win);
    }
}

void on_save(td_widget_t *, void *)
{
    if (!s_dirty) {
        set_status("Nothing to save.");
        return;
    }
    const char *why = save_settings();
    if (why == NULL) {
        s_dirty = false;
        (void)tang_nes_load(&s_set, TANG_NES_INI_REAL);
        status_from_file();
        set_status("Saved.  It applies from the next game start.");
    } else {
        set_status(why);
    }
}

void on_close(td_window_t *)
{
    s_win = nullptr;
    s_list = nullptr;
    s_player_button = nullptr;
    memset(s_status, 0, sizeof s_status);
    s_tab_shown = -1;
}

void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }
    load_settings();

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

    for (int t = 0; t < TABS; t++) {
        s_tab[t] = td_button(s_win, 1 + t * 9, 0, TAB_NAMES[t], on_tab,
                             (void *)(uintptr_t)t);
    }

    /* Input: whose mapping is shown, the eight rows, and the actions. */
    s_player_button = td_button(s_win, 1, 2, "Player 1", on_player, nullptr);
    page_add(0, s_player_button);
    s_list = td_list(s_win, td_rect(1, 4, 34, TANG_NES_BUTTONS), input_item,
                     on_row, nullptr);
    if (s_list != nullptr) {
        td_list_set_count(s_list, TANG_NES_BUTTONS);
    }
    page_add(0, s_list);
    page_add(0, td_button(s_win, 38, 4, "Change", on_row, nullptr));
    page_add(0, td_button(s_win, 38, 6, "Restore defaults", on_restore, nullptr));
    page_add(0, td_button(s_win, 38, 8, "Save", on_save, nullptr));
    page_add(0, td_label(s_win, 38, 10, 0, "Change steps the"));
    page_add(0, td_label(s_win, 38, 11, 0, "selected row."));

    char text[64];
    snprintf(text, sizeof text, "Aspect      %s", tang_nes_aspect_name(s_set.aspect));
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

    /* What the file is, and what a save said.  One label per page, because a
     * widget shared between pages takes the visibility of whichever page is
     * shown last -- which hid this on the Input tab, where Save is. */
    for (int t = 0; t < TABS; t++) {
        s_status[t] = td_label(s_win, 1, 16, 0, "");
        page_add(t, s_status[t]);
    }
    status_from_file();

    show_tab(0);
}

const td_app_t s_app = { "TinyNES Controls", launch, "NES" };

} // namespace

// C linkage: the desktop's init file is C, and calls this by name.
extern "C" void td_tinynes_register(void)
{
    (void)td_app_register(&s_app);
}
