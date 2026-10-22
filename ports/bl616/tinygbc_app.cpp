// TinyGBC Control Deck -- the window that starts Game Boy cartridges.
//
// The same shape as the TinyNES app beside it: an app window with a Library
// page that browses the card and starts what is picked, over a core loaded
// from the card.  A launch loads /cores/console138k/tinygbc.bin, runs the
// AE350 player with the cartridge as the file it asks for, then puts the
// desktop core back.
//
// Only the Library page does anything yet.  Input, Video and Saves are
// placeholders: they name what the player will grow, so the window does not
// have to change shape when they arrive.
//
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The TinyDesk and shell headers are C, and their symbols are unmangled, so
// they are included with C linkage as the TinyNES app does it.
extern "C" {
#include "ff.h"
#include "td_apps.h"
#include "tinydesk/td.h"
#include "tdsh.h"
#include "tang_ae350_run.h"
#include "tang_core_load.h"

/* The USB console writer.  It is declared for the shell's own sources in
 * ports/bl616/tdsh_stdio_redirect.h, which also renames printf here; only the
 * function is wanted, as ports/bl616/tang_jtag_glue.h declares it. */
int tdsh_printf(const char *fmt, ...);
}

#include "FreeRTOS.h"
#include "task.h"

namespace {

const int TICK_MS = 400;
const int LIB_MAX_ENTRIES = 256;
const int LIB_NAME_BYTES = 56;

/* This SDK's FatFS declares AM_RDO, AM_HID, AM_SYS, AM_DIR and AM_ARC but not
 * the volume label's bit, which FatFS gives as 0x08; a card's own label entry
 * is skipped with the other attributes a browser has no use for. */
constexpr BYTE AM_VOLUME_LABEL = 0x08;

/* The three files a launch needs, as FatFS mounts them.  The core is
 * tools/deploy_tinygbc.sh's own card path, and the player is what
 * tools/deploy_tinygbc.sh puts beside TinyNES's. */
constexpr char CORE_REAL[] = "/sd/root/cores/console138k/tinygbc.bin";
constexpr char PLAYER_REAL[] = "/sd/root/ae350/tinygbc-play.elf";
constexpr char DESKTOP_REAL[] = "/sd/root/cores/console138k/desktop.bin";

/* Where the library opens: the first of these that the card has, so a GB
 * folder beside TinyNES's own is found and a card with only a media root
 * still shows one.  Listed as the shell shows them; the task works in /sd. */
const char *const LIB_ROOTS[] = {
    "/root/media/gb", "/root/media/gbc", "/root/media/gbcolor", "/root/media",
};

/* 8 KiB is what the Phosphor player gives its task, and half what a shell
 * worker gave this run: no interpreter, no script, no line table. */
constexpr uint32_t LAUNCH_STACK_WORDS = 2048;
const int TABS = 4;
const int PAGE_WIDGETS = 16;

/* There is no named accent, only TD_COLOR_DEFAULT and raw palette indices. */
const int ACCENT = 14;

/* The tabs exist from the start so the window keeps its shape; only the first
 * one is built. */
const char *const TAB_NAMES[TABS] = { "Library", "Input", "Video", "Saves" };

td_window_t *s_win;
td_widget_t *s_tab[TABS];
td_widget_t *s_page[TABS][PAGE_WIDGETS];
int s_page_count[TABS];
int s_tab_shown = -1;

struct lib_entry
{
    char name[LIB_NAME_BYTES];
    bool is_dir;
};

td_widget_t *s_lib_list;
td_widget_t *s_lib_path;
td_widget_t *s_lib_message;
lib_entry *s_lib_entries;
int s_lib_count;
bool s_lib_truncated;
char s_lib_shown[TD_PATH_MAX];
char s_lib_real[TD_PATH_MAX + 4];
char s_root[TD_PATH_MAX];        /* where the library starts, shown form */
char s_launch_cart[TD_PATH_MAX + 4];

td_widget_t *s_status[TABS];

/* A launch owns the core and the link until the game ends, so a second one is
 * refused rather than reloading the core under the first. */
volatile bool s_launching;
volatile bool s_lib_reload;

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

/* ----------------------------------------------------------------- the list */

void lib_say(const char *text)
{
    if (s_lib_message != nullptr && strcmp(td_widget_text(s_lib_message), text) != 0) {
        td_widget_set_text(s_lib_message, text);
    }
}

bool lib_at_root(void) { return strcmp(s_lib_shown, s_root) == 0; }

void lib_set_path(const char *shown)
{
    snprintf(s_lib_shown, sizeof s_lib_shown, "%s", shown);
    snprintf(s_lib_real, sizeof s_lib_real, "/sd%s", shown);
}

/* A name that can be carried unchanged: a control character or a trailing
 * separator would confuse the listing or the path built from it. */
bool lib_handable(const char *name)
{
    if (name[0] == '\0' || name[0] == '.') {
        return false;
    }
    for (const char *p = name; *p != '\0'; ++p) {
        if (static_cast<unsigned char>(*p) < 0x20) {
            return false;
        }
    }
    return true;
}

/* The cartridges this core reads: a Game Boy image as it comes, or a .zip
 * holding one.  gambatte has no archive reader, so the player inflates the
 * archive itself once the bytes reach it; the deck only has to send the file.
 * Nothing is unzipped here: the BL616's heap is not the place for a megabyte
 * of ROM, and the AE350 has megabytes to spare. */
bool lib_is_cartridge(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot != nullptr && (strcasecmp(dot, ".gbc") == 0 ||
                              strcasecmp(dot, ".gb") == 0 ||
                              strcasecmp(dot, ".zip") == 0);
}

bool lib_listable(const FILINFO &info)
{
    if ((info.fattrib & (AM_HID | AM_SYS | AM_VOLUME_LABEL)) != 0) {
        return false;
    }
    if (!lib_handable(info.fname)) {
        return false;
    }
    if (strlen(info.fname) >= LIB_NAME_BYTES) {
        s_lib_truncated = true;
        return false;
    }
    if ((info.fattrib & AM_DIR) != 0) {
        return true;
    }
    return lib_is_cartridge(info.fname);
}

/* Folders first, then cartridges, each by name without case. */
int lib_compare(const void *a, const void *b)
{
    const lib_entry *x = static_cast<const lib_entry *>(a);
    const lib_entry *y = static_cast<const lib_entry *>(b);
    if (x->is_dir != y->is_dir) {
        return x->is_dir ? -1 : 1;
    }
    return strcasecmp(x->name, y->name);
}

void lib_load(void)
{
    free(s_lib_entries);
    s_lib_entries = nullptr;
    s_lib_count = 0;
    s_lib_truncated = false;
    if (s_lib_list != nullptr) {
        td_list_set_count(s_lib_list, 0);
    }
    if (s_lib_path != nullptr) {
        td_widget_set_text(s_lib_path, s_lib_shown);
    }

    DIR dir;
    FILINFO info;
    if (f_opendir(&dir, s_lib_real) != FR_OK) {
        lib_say("No folder here on the card");
        return;
    }

    /* Counted before anything is allocated, so a folder of a few files costs
     * a few hundred bytes and not the ceiling. */
    const int parent = lib_at_root() ? 0 : 1;
    int capacity = parent;
    FRESULT result;
    while ((result = f_readdir(&dir, &info)) == FR_OK && info.fname[0] != '\0') {
        if (!lib_listable(info)) {
            continue;
        }
        if (capacity < LIB_MAX_ENTRIES) {
            ++capacity;
        } else {
            s_lib_truncated = true;
        }
    }
    if (result != FR_OK || f_rewinddir(&dir) != FR_OK) {
        (void)f_closedir(&dir);
        lib_say("The card refused that folder");
        return;
    }

    if (capacity > 0) {
        s_lib_entries = static_cast<lib_entry *>(calloc(static_cast<size_t>(capacity), sizeof(lib_entry)));
        if (s_lib_entries == nullptr) {
            (void)f_closedir(&dir);
            lib_say("Not enough memory for the list");
            return;
        }
    }

    int n = 0;
    if (parent != 0) {
        snprintf(s_lib_entries[n].name, LIB_NAME_BYTES, "..");
        s_lib_entries[n].is_dir = true;
        ++n;
    }
    while (n < capacity && (result = f_readdir(&dir, &info)) == FR_OK && info.fname[0] != '\0') {
        if (!lib_listable(info)) {
            continue;
        }
        snprintf(s_lib_entries[n].name, LIB_NAME_BYTES, "%s", info.fname);
        s_lib_entries[n].is_dir = (info.fattrib & AM_DIR) != 0;
        ++n;
    }
    (void)f_closedir(&dir);
    s_lib_count = n;
    if (s_lib_count > 1) {
        qsort(s_lib_entries, static_cast<size_t>(s_lib_count), sizeof(lib_entry), lib_compare);
    }
    if (s_lib_list != nullptr) {
        td_list_set_count(s_lib_list, s_lib_count);
    }

    char text[TD_TEXT_MAX];
    if (s_lib_truncated) {
        snprintf(text, sizeof text, "%d shown, more not listed", s_lib_count);
    } else {
        snprintf(text, sizeof text, "%d entr%s", s_lib_count, s_lib_count == 1 ? "y" : "ies");
    }
    lib_say(text);
}

/* The first root the card has.  A card with none of them keeps the last, so
 * the page says so instead of showing nothing at all. */
void lib_choose_root(void)
{
    DIR dir;
    char real[TD_PATH_MAX + 4];

    for (size_t i = 0; i < sizeof LIB_ROOTS / sizeof LIB_ROOTS[0]; i++) {
        snprintf(real, sizeof real, "/sd%s", LIB_ROOTS[i]);
        if (f_opendir(&dir, real) == FR_OK) {
            (void)f_closedir(&dir);
            snprintf(s_root, sizeof s_root, "%s", LIB_ROOTS[i]);
            return;
        }
    }
    snprintf(s_root, sizeof s_root, "%s", LIB_ROOTS[sizeof LIB_ROOTS / sizeof LIB_ROOTS[0] - 1]);
}

/* ---------------------------------------------------------------- actions */

void lib_descend_to(const char *name);

/* Start the chosen cartridge in a task of this app's own: load the TinyGBC
 * core, run the player with the cartridge, then put the desktop core back. */
void launch_task(void *)
{
    const tang_load_result_t loaded = tang_core_load(CORE_REAL);
    int code = 1;
    if (loaded == TANG_LOAD_OK) {
        code = tang_ae350_run(PLAYER_REAL, s_launch_cart);
        /* Back to the desktop core whatever the run did: its desk layer is
         * what the screen returns to. */
        (void)tang_core_load(DESKTOP_REAL);
    } else if (loaded == TANG_LOAD_BUSY) {
        tdsh_printf("gbc: another core load is already running\r\n");
    } else {
        tdsh_printf("gbc: the core did not load\r\n");
    }
    tdsh_printf("gbc: done (%d), %u bytes of stack left\r\n", code,
                (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
    s_launching = false;
    s_lib_reload = true;
    vTaskDelete(NULL);
}

void lib_start(int index)
{
    if (s_launching) {
        lib_say("A game is already starting");
        return;
    }
    if (index < 0 || index >= s_lib_count) {
        return;
    }
    /* The task has no shell session, so nothing maps /root onto /sd for it. */
    const int written = snprintf(s_launch_cart, sizeof s_launch_cart, "%s/%s",
                                 s_lib_real, s_lib_entries[index].name);
    if (written <= 0 || written >= static_cast<int>(sizeof s_launch_cart)) {
        lib_say("That path is too long");
        return;
    }
    /* The names are not needed while a game runs, and the task that loads the
     * core and serves the AE350 wants the heap more than this page does. */
    free(s_lib_entries);
    s_lib_entries = nullptr;
    s_lib_count = 0;
    s_lib_truncated = false;
    if (s_lib_list != nullptr) {
        td_list_set_count(s_lib_list, 0);
    }
    lib_say("Starting ...");
    s_launching = true;
    if (xTaskCreate(launch_task, "gbclaunch", LAUNCH_STACK_WORDS, nullptr,
                    tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
        s_launching = false;
        s_lib_reload = true;
        lib_say("Not enough memory to start the game");
    }
}

/* The launch empties the page before it starts, so put the list back once the
 * game has ended; the tick is where the desktop's own task can touch widgets. */
void on_tick(td_window_t *)
{
    if (s_launching || !s_lib_reload) {
        return;
    }
    s_lib_reload = false;
    if (td_win_is_open(s_win)) {
        lib_load();
    }
}

void lib_descend_to(const char *name)
{
    char next[TD_PATH_MAX];
    if (strcmp(name, "..") == 0) {
        if (lib_at_root()) {
            lib_say("This is the top of the library");
            return;
        }
        snprintf(next, sizeof next, "%s", s_lib_shown);
        char *slash = strrchr(next, '/');
        if (slash == nullptr || slash <= next + strlen(s_root)) {
            snprintf(next, sizeof next, "%s", s_root);
        } else {
            *slash = '\0';
        }
    } else {
        const int written = snprintf(next, sizeof next, "%s/%s", s_lib_shown, name);
        if (written <= 0 || written >= static_cast<int>(sizeof next)) {
            lib_say("That folder's path is too long");
            return;
        }
    }
    lib_set_path(next);
    lib_load();
    if (s_lib_list != nullptr) {
        td_list_select(s_lib_list, 0);
        td_widget_focus(s_lib_list);
    }
}

void lib_on_activate(td_widget_t *, void *)
{
    const int index = s_lib_list != nullptr ? td_list_selected(s_lib_list) : -1;
    if (index < 0 || index >= s_lib_count) {
        return;
    }
    if (s_lib_entries[index].is_dir) {
        lib_descend_to(s_lib_entries[index].name);
    } else {
        lib_start(index);
    }
}

/* The button activates the row the way Enter and a double-click do, so a
 * folder opens and a cartridge starts from the same key or click. */
void lib_on_launch(td_widget_t *, void *) { lib_on_activate(nullptr, nullptr); }

void lib_on_up(td_widget_t *, void *) { lib_descend_to(".."); }

void lib_on_refresh(td_widget_t *, void *) { lib_load(); }

const char *lib_get_item(td_widget_t *, int index, int *fg, void *)
{
    static char text[LIB_NAME_BYTES + 2];
    if (index < 0 || index >= s_lib_count) {
        return "";
    }
    const lib_entry &e = s_lib_entries[index];
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

void on_close(td_window_t *)
{
    s_win = nullptr;
    memset(s_status, 0, sizeof s_status);
    s_lib_list = nullptr;
    s_lib_path = nullptr;
    s_lib_message = nullptr;
    /* The library list is freed on close: the heap is shared with the shell
     * workers and the Bluetooth stack, and a folder's names are not worth
     * keeping while the window is not open. */
    free(s_lib_entries);
    s_lib_entries = nullptr;
    s_lib_count = 0;
    s_lib_reload = false;
    s_tab_shown = -1;
}

void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }

    td_window_desc_t d = {};
    d.title = "TinyGBC";
    d.rect = td_rect(-1, -1, 66, 26);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE | TD_WIN_RESIZABLE;
    d.min_w = 50;
    d.min_h = 18;
    d.on_close = on_close;
    d.on_tick = on_tick;
    d.tick_ms = TICK_MS;
    d.icon = "GBC";
    s_win = td_win_create(&d);
    if (s_win == nullptr) {
        return;
    }
    memset(s_page_count, 0, sizeof s_page_count);

    /* The tab row is spaced by caption, because "Library" is wider than the
     * three that follow it and a fixed pitch would run the labels together. */
    int tab_x = 1;
    for (int t = 0; t < TABS; t++) {
        s_tab[t] = td_button(s_win, tab_x, 0, TAB_NAMES[t], on_tab,
                             (void *)(uintptr_t)t);
        tab_x += static_cast<int>(strlen(TAB_NAMES[t])) + 5;
    }

    /* Library: the cartridge browser.  A launch loads the core and runs the
     * player in a task of its own, so this window survives it -- it lives on
     * the BL616, not on the core. */
    lib_choose_root();
    lib_set_path(s_root);
    s_lib_path = td_label(s_win, 1, 2, 0, "");
    s_lib_list = td_list(s_win, td_rect(1, 3, -1, -11), lib_get_item, lib_on_activate,
                         nullptr);
    td_scrollbar(s_win, -1, 3, -11, s_lib_list);
    page_add(0, s_lib_path);
    page_add(0, s_lib_list);
    page_add(0, td_button(s_win, 1, 16, "Launch", lib_on_launch, nullptr));
    page_add(0, td_button(s_win, 12, 16, "Up", lib_on_up, nullptr));
    page_add(0, td_button(s_win, 19, 16, "Refresh", lib_on_refresh, nullptr));
    lib_load();
    if (s_lib_list != nullptr) {
        td_list_select(s_lib_list, 0);
    } else {
        lib_say("Not enough memory for the list");
    }

    /* The other three pages: named like TinyNES's, and saying plainly that
     * nothing is behind them yet. */
    page_add(1, td_label(s_win, 1, 2, 0, "Input is not implemented yet."));
    page_add(1, td_label(s_win, 1, 4, 0, "The pad is read by the player"));
    page_add(1, td_label(s_win, 1, 5, 0, "with its own fixed mapping."));

    page_add(2, td_label(s_win, 1, 2, 0, "Video is not implemented yet."));
    page_add(2, td_label(s_win, 1, 4, 0, "The picture is five columns a"));
    page_add(2, td_label(s_win, 1, 5, 0, "pixel and five rows a line."));

    page_add(3, td_label(s_win, 1, 2, 0, "Saves are not implemented yet."));
    page_add(3, td_label(s_win, 1, 4, 0, "The player keeps no battery save"));
    page_add(3, td_label(s_win, 1, 5, 0, "and no save state on the card."));

    /* One status label per page, because a widget shared between pages takes
     * the visibility of whichever page is shown last. */
    for (int t = 0; t < TABS; t++) {
        s_status[t] = td_label(s_win, 1, 17, 0, "");
        page_add(t, s_status[t]);
    }
    s_lib_message = s_status[0];

    show_tab(0);
}

const td_app_t s_app = { "TinyGBC Control Deck", launch, "GBC" };

} // namespace

// C linkage: the desktop's init file is C, and calls this by name.
extern "C" void td_tinygbc_register(void)
{
    (void)td_app_register(&s_app);
}
