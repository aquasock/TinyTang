// TinyNES Launcher -- the window that browses the NES library and starts the
// cartridge chosen.
//
// Opened from the start menu or the desktop before a game rather than over
// one: it reads /root/media/nes, shows its folders and then the cartridges
// in them, and starts the selection through TinyNES's own open handler, so
// the path taken is the one a file opened in the Files app already takes --
// a `.nes`, or a `.zip`, which the player unpacks itself.
//
// That last one is why this window exists.  A `.zip` opened in the Files app
// deliberately starts nothing (there is no `zip.tdsh`, by the user's
// decision), so a zipped cartridge had no way in from the desktop; here it
// has one, and the rule the Files app keeps is untouched.
//
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// The TinyDesk and shell headers are C, and their symbols are unmangled, so
// they are included with C linkage as the Control Deck and Phosphor apps do.
extern "C" {
#include "ff.h"
#include "td_apps.h"
}

namespace {

/* The library, as the shell names it and as FatFS does. */
constexpr char ROOT_SHOWN[] = "/root/media/nes";
constexpr char ROOT_REAL[] = "/sd/root/media/nes";

/* The library's largest folder holds 63 files, which is what the split into
 * letter folders was for (TDESK-017); this ceiling is the window's own, and
 * a folder past it says so rather than quietly leaving files out. */
constexpr int MAX_ENTRIES = 256;

/* A name is kept whole.  The library's longest is 47 bytes; the room above
 * that is for a folder that does not obey that, not for truncation. */
constexpr int NAME_BYTES = 56;

/* This SDK's FatFS declares AM_RDO, AM_HID, AM_SYS, AM_DIR and AM_ARC but
 * not the volume label's bit, which FatFS gives as 0x08; a card's own label
 * entry is skipped with the other attributes a browser has no use for. */
constexpr BYTE AM_VOLUME_LABEL = 0x08;

/* The shell gives a variable 128 bytes (TDSH-008) and the path this window
 * hands over rides in FILE, so it must fit that and must survive being
 * quoted into the command typed at the Terminal's prompt. */
constexpr int SHELL_VALUE_MAX = 128;
constexpr int CMD_BYTES = 256;

/* The handler the launch runs, so that what starts a game here is exactly
 * what starts one from the Files app. */
constexpr char HANDLER[] = "tdsh run /root/scripts/open/nes.tdsh";

struct entry
{
    char name[NAME_BYTES];
    bool is_dir;
};

td_window_t *s_win;
td_widget_t *s_list;
td_widget_t *s_path;
td_widget_t *s_message;

entry *s_entries;
int s_count;
bool s_truncated;

char s_shown[TD_PATH_MAX];    /* the shell's path: /root/media/nes/A */
char s_real[TD_PATH_MAX + 4]; /* the same for FatFS, under /sd   */

/* ---------------------------------------------------------------- helpers */

bool at_root(void) { return strcmp(s_shown, ROOT_SHOWN) == 0; }

void set_path(const char *shown)
{
    snprintf(s_shown, sizeof s_shown, "%s", shown);
    snprintf(s_real, sizeof s_real, "/sd%s", shown);
}

void say(const char *text)
{
    if (s_message != nullptr && strcmp(td_widget_text(s_message), text) != 0) {
        td_widget_set_text(s_message, text);
    }
}

/* True for a name that can be handed to the shell unchanged.  A quote would
 * end FILE's own quoting and a '$' or a backtick could be read as one, and a
 * name that did would start a game at the wrong path -- silently, which is
 * how a mangled path read once already.  The library carries none of these;
 * one that did is left out of the list rather than listed and then refused. */
bool handable(const char *name)
{
    if (name[0] == '\0' || name[0] == '.') {
        return false;
    }
    if (strpbrk(name, "\"$`") != nullptr) {
        return false;
    }
    for (const char *p = name; *p != '\0'; ++p) {
        if (static_cast<unsigned char>(*p) < 0x20) {
            return false;
        }
    }
    return true;
}

bool is_cartridge(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot != nullptr && (strcasecmp(dot, ".nes") == 0 || strcasecmp(dot, ".zip") == 0);
}

bool listable(const FILINFO &info)
{
    if ((info.fattrib & (AM_HID | AM_SYS | AM_VOLUME_LABEL)) != 0) {
        return false;
    }
    if (!handable(info.fname)) {
        return false;
    }
    if (strlen(info.fname) >= NAME_BYTES) {
        s_truncated = true;
        return false;
    }
    if ((info.fattrib & AM_DIR) != 0) {
        return true;
    }
    return is_cartridge(info.fname);
}

/* Folders first, then cartridges, each by name without case. */
int compare(const void *a, const void *b)
{
    const entry *x = static_cast<const entry *>(a);
    const entry *y = static_cast<const entry *>(b);
    if (x->is_dir != y->is_dir) {
        return x->is_dir ? -1 : 1;
    }
    return strcasecmp(x->name, y->name);
}

/* Read the current folder.  The folder is counted before anything is
 * allocated, so a folder of a few files costs a few hundred bytes and not
 * the ceiling: the script worker a launch starts wants 16 KB of stack on the
 * same heap (BL6-010), and reserving the ceiling for every folder is what
 * the Phosphor catalog was rewritten to stop doing. */
void load(void)
{
    free(s_entries);
    s_entries = nullptr;
    s_count = 0;
    s_truncated = false;
    if (s_list != nullptr) {
        td_list_set_count(s_list, 0);
    }
    if (s_path != nullptr) {
        td_widget_set_text(s_path, s_shown);
    }

    DIR dir;
    FILINFO info;
    if (f_opendir(&dir, s_real) != FR_OK) {
        say("No folder here on the card");
        return;
    }

    const int parent = at_root() ? 0 : 1;
    int capacity = parent;
    FRESULT result;
    while ((result = f_readdir(&dir, &info)) == FR_OK && info.fname[0] != '\0') {
        if (!listable(info)) {
            continue;
        }
        if (capacity < MAX_ENTRIES) {
            ++capacity;
        } else {
            s_truncated = true;
        }
    }
    if (result != FR_OK || f_rewinddir(&dir) != FR_OK) {
        (void)f_closedir(&dir);
        say("The card refused that folder");
        return;
    }

    if (capacity > 0) {
        s_entries = static_cast<entry *>(calloc(static_cast<size_t>(capacity), sizeof(entry)));
        if (s_entries == nullptr) {
            (void)f_closedir(&dir);
            say("Not enough memory for the list");
            return;
        }
    }

    int n = 0;
    if (parent != 0) {
        snprintf(s_entries[n].name, NAME_BYTES, "..");
        s_entries[n].is_dir = true;
        ++n;
    }
    while (n < capacity && (result = f_readdir(&dir, &info)) == FR_OK && info.fname[0] != '\0') {
        if (!listable(info)) {
            continue;
        }
        snprintf(s_entries[n].name, NAME_BYTES, "%s", info.fname);
        s_entries[n].is_dir = (info.fattrib & AM_DIR) != 0;
        ++n;
    }
    (void)f_closedir(&dir);
    s_count = n;
    if (s_count > 1) {
        qsort(s_entries, static_cast<size_t>(s_count), sizeof(entry), compare);
    }
    if (s_list != nullptr) {
        td_list_set_count(s_list, s_count);
    }

    char text[TD_TEXT_MAX];
    if (s_truncated) {
        snprintf(text, sizeof text, "%d shown, more not listed", s_count);
    } else {
        snprintf(text, sizeof text, "%d entr%s", s_count, s_count == 1 ? "y" : "ies");
    }
    say(text);
}

/* ---------------------------------------------------------------- actions */

void descend_to(const char *name);

/* Hand the chosen cartridge to the handler the way the desktop would, by
 * typing the same line into the Terminal: the path is set as FILE and the
 * script runs, waits for the game to end and puts the desktop core back. */
void start(int index)
{
    if (index < 0 || index >= s_count) {
        return;
    }
    char shown[TD_PATH_MAX];
    const int written = snprintf(shown, sizeof shown, "%s/%s", s_shown, s_entries[index].name);
    if (written <= 0 || written >= static_cast<int>(sizeof shown)) {
        say("That path is too long");
        return;
    }
    if (written >= SHELL_VALUE_MAX) {
        say("That path is too long for the shell");
        return;
    }
    char cmd[CMD_BYTES];
    snprintf(cmd, sizeof cmd, "FILE=\"%s\"; %s", shown, HANDLER);
    say("Starting ...");
    if (!td_terminal_run(cmd)) {
        say("This build has no shell");
    }
}

void descend_to(const char *name)
{
    char next[TD_PATH_MAX];
    if (strcmp(name, "..") == 0) {
        if (at_root()) {
            say("This is the top of the library");
            return;
        }
        snprintf(next, sizeof next, "%s", s_shown);
        char *slash = strrchr(next, '/');
        if (slash == nullptr || slash <= next + strlen(ROOT_SHOWN)) {
            snprintf(next, sizeof next, "%s", ROOT_SHOWN);
        } else {
            *slash = '\0';
        }
    } else {
        const int written = snprintf(next, sizeof next, "%s/%s", s_shown, name);
        if (written <= 0 || written >= static_cast<int>(sizeof next)) {
            say("That folder's path is too long");
            return;
        }
    }
    set_path(next);
    load();
    if (s_list != nullptr) {
        td_list_select(s_list, 0);
        td_widget_focus(s_list);
    }
}

void on_activate(td_widget_t *, void *)
{
    const int index = s_list != nullptr ? td_list_selected(s_list) : -1;
    if (index < 0 || index >= s_count) {
        return;
    }
    if (s_entries[index].is_dir) {
        descend_to(s_entries[index].name);
    } else {
        start(index);
    }
}

/* The button activates the row the way Enter and a double-click do, so a
 * folder opens and a cartridge starts from the same key or click. */
void on_launch(td_widget_t *, void *) { on_activate(nullptr, nullptr); }

void on_up(td_widget_t *, void *) { descend_to(".."); }

void on_refresh(td_widget_t *, void *) { load(); }

void on_close_btn(td_widget_t *, void *)
{
    if (s_win != nullptr) {
        td_win_close(s_win);
    }
}

/* Free the list on close: the heap is shared with the shell workers and the
 * Bluetooth stack, and nothing here is worth keeping while the window is not
 * open. */
void on_close(td_window_t *)
{
    s_win = nullptr;
    s_list = nullptr;
    s_path = nullptr;
    s_message = nullptr;
    free(s_entries);
    s_entries = nullptr;
    s_count = 0;
}

const char *get_item(td_widget_t *, int index, int *fg, void *)
{
    static char text[NAME_BYTES + 2];
    if (index < 0 || index >= s_count) {
        return "";
    }
    const entry &e = s_entries[index];
    if (strcmp(e.name, "..") == 0) {
        *fg = td_theme()->accent;
        return "..";
    }
    if (e.is_dir) {
        *fg = td_theme()->accent;
        snprintf(text, sizeof text, "%s/", e.name);
    } else {
        snprintf(text, sizeof text, "%s", e.name);
    }
    return text;
}

void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }
    td_window_desc_t d = {};
    d.title = "TinyNES Launcher";
    d.rect = td_rect(-1, -1, 66, 20);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE | TD_WIN_RESIZABLE;
    d.min_w = 46;
    d.min_h = 12;
    d.on_close = on_close;
    s_win = td_win_create(&d);
    if (s_win == nullptr) {
        return;
    }

    set_path(ROOT_SHOWN);
    s_path = td_label(s_win, 0, 0, 0, "");
    s_list = td_list(s_win, td_rect(0, 1, -1, -5), get_item, on_activate, nullptr);
    (void)td_scrollbar(s_win, -1, 1, -5, s_list);
    (void)td_button(s_win, 0, -3, "Launch", on_launch, nullptr);
    (void)td_button(s_win, 11, -3, "Up", on_up, nullptr);
    (void)td_button(s_win, 18, -3, "Refresh", on_refresh, nullptr);
    (void)td_button(s_win, 30, -3, "Close", on_close_btn, nullptr);
    s_message = td_label(s_win, 0, -2, 0, "");
    td_widget_set_color(s_message, td_theme()->dim, TD_COLOR_DEFAULT);

    load();
    if (s_list != nullptr) {
        td_list_select(s_list, 0);
        td_widget_focus(s_list);
    }
}

const td_app_t s_app = { "TinyNES Launcher", launch, ">|" };

} // namespace

// C linkage: the desktop's init file is C and calls this by name.
extern "C" void td_tinynes_launcher_register(void)
{
    (void)td_app_register(&s_app);
}
