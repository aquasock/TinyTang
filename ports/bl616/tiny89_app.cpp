// TinyTang: Tiny89, the window that starts calculator sessions from the card.
//
// The calculator's core is loaded with tangload and the AE350 program is run
// with ae350run, which serves it the OS image the same way it serves a game
// its cartridge; the program converts the .89u itself (Tiny89's
// firmware/common/t89_bios.c), so the card carries the file the user has.
// Ctrl+Alt+Q in the program ends the session and this window's task then puts
// the desktop core back, exactly as the two game windows do when their player
// returns.
//
// The library browses /root/media/ti89: folders can be entered and ".." climbs
// back, the way the TinyNES, TinyGBC and TinyPhosphor windows browse theirs.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// The TinyDesk and shell headers are C, and their symbols are unmangled, so
// they are included with C linkage as the other apps do it.
extern "C" {
#include "ff.h"
#include "td_apps.h"
#include "tinydesk/td.h"
#include "tdsh.h"
#include "tang_ae350_run.h"
#include "tang_core_load.h"
#include "td_saves_log.h"

/* The USB console writer, declared for the shell's own sources in
 * ports/bl616/tdsh_stdio_redirect.h.  Only the function is wanted. */
int tdsh_printf(const char *fmt, ...);
}

#include "FreeRTOS.h"
#include "task.h"

namespace {

const int TICK_MS = 400;
const int LIB_MAX_ENTRIES = 256;
const int LIB_NAME_BYTES = 96;

/* This SDK's FatFS declares AM_RDO, AM_HID, AM_SYS, AM_DIR and AM_ARC but not
 * the volume label's bit, which FatFS gives as 0x08. */
constexpr BYTE AM_VOLUME_LABEL = 0x08;

/* The three files a session needs, as FatFS mounts them. */
constexpr char CORE_REAL[] = "/sd/root/cores/console138k/tiny89.bin";
constexpr char PLAYER_REAL[] = "/sd/root/ae350/tiny89-play.elf";
constexpr char DESKTOP_REAL[] = "/sd/root/cores/console138k/tinytang.bin";

/* The library's root: the user's own OS images, one folder per card. */
constexpr char MEDIA_ROOT[] = "/root/media/ti89";

/* The window: 50 columns and 20 rows, the size the game windows use, which
 * gives the list twelve rows above the buttons and the status line. */
constexpr int WIN_W = 50;
constexpr int WIN_H = 20;
constexpr int LIB_ROWS = 12;
constexpr uint32_t LAUNCH_STACK_WORDS = 2048;
constexpr int TABS = 2;
constexpr int PAGE_WIDGETS = 10;
const int ACCENT = 14;           /* the palette's accent, as the other apps use */

struct lib_entry {
    char name[LIB_NAME_BYTES];
    bool is_dir;
};

td_window_t *s_win;
td_widget_t *s_path;
td_widget_t *s_list;
td_widget_t *s_status;

// Two pages, the TinyNES arrangement: the library, and what the card holds for
// the image selected there.
td_widget_t *s_tab[TABS];
td_widget_t *s_page[TABS][PAGE_WIDGETS];
int s_page_count[TABS];
int s_tab_shown = -1;
td_widget_t *s_saves_path, *s_saves_state, *s_saves_ram, *s_saves_flash,
    *s_saves_state_log, *s_saves_ram_log, *s_saves_flash_log, *s_saves_hint;

lib_entry *s_entries;
int s_count;
bool s_truncated;
bool s_launching;
bool s_reload;

char s_shown[TD_PATH_MAX];
char s_real[TD_PATH_MAX];
char s_cart[TD_PATH_MAX + 4];

/* ---------------------------------------------------------------- the list */

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

void say(const char *text)
{
    if (s_status != nullptr && strcmp(td_widget_text(s_status), text) != 0) {
        td_widget_set_text(s_status, text);
    }
}

bool at_root(void) { return strcmp(s_shown, MEDIA_ROOT) == 0; }

void set_path(const char *shown)
{
    snprintf(s_shown, sizeof s_shown, "%s", shown);
    snprintf(s_real, sizeof s_real, "/sd%s", shown);
}

/* A name that can be carried unchanged: a leading dot would hide it and a
 * control character would confuse the listing. */
bool handable(const char *name)
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

/* The images this window starts: the TI operating-system file as shipped, or
 * a raw image a converter produced from one. */
bool is_image(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot != nullptr && (strcasecmp(dot, ".89u") == 0 ||
                              strcasecmp(dot, ".89t") == 0 ||
                              strcasecmp(dot, ".raw") == 0 ||
                              strcasecmp(dot, ".rom") == 0);
}

bool listable(const FILINFO &info)
{
    if ((info.fattrib & (AM_HID | AM_SYS | AM_VOLUME_LABEL)) != 0) {
        return false;
    }
    if (!handable(info.fname)) {
        return false;
    }
    if (strlen(info.fname) >= LIB_NAME_BYTES) {
        s_truncated = true;
        return false;
    }
    if ((info.fattrib & AM_DIR) != 0) {
        return true;
    }
    return is_image(info.fname);
}

/* Folders first, then images, each by name without case. */
int compare(const void *a, const void *b)
{
    const lib_entry *x = static_cast<const lib_entry *>(a);
    const lib_entry *y = static_cast<const lib_entry *>(b);
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
        say("No /root/media/ti89 folder on the card");
        return;
    }

    /* Counted before anything is allocated, so a small folder costs a few
     * hundred bytes rather than the ceiling. */
    const bool root = at_root();
    int capacity = root ? 0 : 1;    // ".." below the root
    FRESULT result;
    while ((result = f_readdir(&dir, &info)) == FR_OK && info.fname[0] != '\0') {
        if (!listable(info)) {
            continue;
        }
        if (capacity < LIB_MAX_ENTRIES) {
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
        s_entries = static_cast<lib_entry *>(calloc(static_cast<size_t>(capacity),
                                                    sizeof(lib_entry)));
        if (s_entries == nullptr) {
            (void)f_closedir(&dir);
            say("Not enough memory for the list");
            return;
        }
    }
    int n = 0;
    if (!root) {
        snprintf(s_entries[n].name, LIB_NAME_BYTES, "..");
        s_entries[n].is_dir = true;
        ++n;
    }
    while (n < capacity && (result = f_readdir(&dir, &info)) == FR_OK &&
           info.fname[0] != '\0') {
        if (!listable(info)) {
            continue;
        }
        snprintf(s_entries[n].name, LIB_NAME_BYTES, "%s", info.fname);
        s_entries[n].is_dir = (info.fattrib & AM_DIR) != 0;
        ++n;
    }
    (void)f_closedir(&dir);
    s_count = n;
    if (s_count > 1) {
        qsort(s_entries, static_cast<size_t>(s_count), sizeof(lib_entry), compare);
    }
    if (s_list != nullptr) {
        td_list_set_count(s_list, s_count);
    }

    char text[TD_TEXT_MAX];
    if (s_truncated) {
        snprintf(text, sizeof text, "%d shown, more not listed", s_count);
    } else if (root && s_count == 0) {
        snprintf(text, sizeof text, "No images in /root/media/ti89");
    } else {
        snprintf(text, sizeof text, "%d entr%s", s_count, s_count == 1 ? "y" : "ies");
    }
    say(text);
}

/* ---------------------------------------------------------------- actions */

void descend_to(const char *name);

/* Start the chosen image in a task of this app's own: load the calculator
 * core, run the player with the image, then put the desktop core back. */
void launch_task(void *)
{
    const tang_load_result_t loaded = tang_core_load(CORE_REAL);
    int code = 1;
    if (loaded == TANG_LOAD_OK) {
        code = tang_ae350_run(PLAYER_REAL, s_cart);
        /* Back to the desktop core whatever the run did: its desk layer is
         * what the screen returns to. */
        (void)tang_core_load(DESKTOP_REAL);
    } else if (loaded == TANG_LOAD_BUSY) {
        tdsh_printf("ti89: another core load is already running\r\n");
    } else {
        tdsh_printf("ti89: the core did not load\r\n");
    }
    tdsh_printf("ti89: done (%d), %u bytes of stack left\r\n", code,
                (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
    s_launching = false;
    s_reload = true;
    vTaskDelete(nullptr);
}

void start(int index)
{
    if (s_launching) {
        say("A session is already starting");
        return;
    }
    if (index < 0 || index >= s_count || s_entries[index].is_dir) {
        return;
    }
    /* The task has no shell session, so nothing maps /root onto /sd for it. */
    const int written = snprintf(s_cart, sizeof s_cart, "%s/%s", s_real,
                                 s_entries[index].name);
    if (written <= 0 || written >= static_cast<int>(sizeof s_cart)) {
        say("That path is too long");
        return;
    }
    /* The names are not needed while the calculator runs, and the task that
     * loads the core and serves the AE350 wants the heap more than this page
     * does. */
    free(s_entries);
    s_entries = nullptr;
    s_count = 0;
    s_truncated = false;
    if (s_list != nullptr) {
        td_list_set_count(s_list, 0);
    }
    say("Starting ...");
    s_launching = true;
    if (xTaskCreate(launch_task, "ti89launch", LAUNCH_STACK_WORDS, nullptr,
                    tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
        s_launching = false;
        s_reload = true;
        say("Not enough memory to start a session");
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
        if (slash == nullptr || slash <= next + strlen(MEDIA_ROOT)) {
            snprintf(next, sizeof next, "%s", MEDIA_ROOT);
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

void on_launch(td_widget_t *, void *) { on_activate(nullptr, nullptr); }
void on_up(td_widget_t *, void *) { descend_to(".."); }
void on_refresh(td_widget_t *, void *) { load(); }

const char *get_item(td_widget_t *, int index, int *fg, void *)
{
    static char text[LIB_NAME_BYTES + 4];
    if (index < 0 || index >= s_count) {
        return "";
    }
    if (s_entries[index].is_dir) {
        *fg = td_theme()->accent;
    }
    snprintf(text, sizeof text, "%s%s", s_entries[index].name,
             s_entries[index].is_dir ? "/" : "");
    return text;
}

void on_close(td_window_t *)
{
    free(s_entries);
    s_entries = nullptr;
    s_count = 0;
}

void on_tick(td_window_t *)
{
    if (s_reload && !s_launching) {
        s_reload = false;
        load();
    }
}

/* ----------------------------------------------------------------- saves */

/* What the page shows for one channel: the verdict word the board wrote, when
 * it wrote it and the size it carried.  The log's parsing is the shared one in
 * td_saves_log.h, which TinyNES's Saves page reads by the same rules. */
void describe(const td_saves_attempt_t &rec, const char *label, char *out,
              size_t cap)
{
    char when[32] = "unknown";

    if (rec.stamp[0] != '\0') {
        td_saves_when(rec.stamp, when, sizeof when);
    }
    if (rec.verdict[0] != '\0') {
        snprintf(out, cap, "%s %-6s %s %lu B", label, rec.verdict, when, rec.want);
    } else if (rec.armed) {
        snprintf(out, cap, "%s armed  %s, no finish", label, when);
    } else {
        snprintf(out, cap, "%s no attempt in the log", label);
    }
}

/* What the card holds for the image selected in the Library: the two files a
 * session writes, and the board's own word on the last attempt.  Every line of
 * last.log is stamped with the RTC's time, so this says when as well as what:
 * a save that did not verify is visible here without opening the console, and
 * the answer is to press Ctrl+S in the calculator and Refresh. */
void saves_refresh(void)
{
    const int index = s_list != nullptr ? td_list_selected(s_list) : -1;
    char base[LIB_NAME_BYTES];
    char path[TD_PATH_MAX + 16];
    char text[TD_TEXT_MAX];
    FILINFO info;

    td_widget_set_text(s_saves_state_log, "");
    td_widget_set_text(s_saves_ram_log, "");
    td_widget_set_text(s_saves_flash_log, "");
    td_widget_set_text(s_saves_hint, "");
    if (index < 0 || index >= s_count || s_entries[index].is_dir) {
        td_widget_set_text(s_saves_path, "Select an image in the Library page");
        td_widget_set_text(s_saves_state, "");
        td_widget_set_text(s_saves_ram, "");
        td_widget_set_text(s_saves_flash, "");
        return;
    }
    snprintf(base, sizeof base, "%s", s_entries[index].name);
    char *dot = strrchr(base, '.');
    if (dot != nullptr && dot != base) {
        *dot = '\0';
    }
    td_widget_set_text(s_saves_path, s_entries[index].name);

    /* The state is the save that matters: it is what a launch resumes. */
    snprintf(path, sizeof path, "/sd/root/states/ti89/%s.state", base);
    if (f_stat(path, &info) == FR_OK) {
        snprintf(text, sizeof text, "State  present, %lu bytes",
                 (unsigned long)info.fsize);
    } else {
        snprintf(text, sizeof text, "State  none -- a launch will boot");
    }
    td_widget_set_text(s_saves_state, text);

    snprintf(path, sizeof path, "/sd/root/saves/ti89/%s.ram", base);
    if (f_stat(path, &info) == FR_OK) {
        snprintf(text, sizeof text, "RAM    present, %lu bytes",
                 (unsigned long)info.fsize);
    } else {
        snprintf(text, sizeof text, "RAM    none -- the last save did not verify");
    }
    td_widget_set_text(s_saves_ram, text);

    snprintf(path, sizeof path, "/sd/root/saves/ti89/%s.flash", base);
    if (f_stat(path, &info) == FR_OK) {
        snprintf(text, sizeof text, "Flash  present, %lu bytes",
                 (unsigned long)info.fsize);
    } else {
        snprintf(text, sizeof text, "Flash  none yet");
    }
    td_widget_set_text(s_saves_flash, text);

    /* The board's own word, from the log the firmware appends to.  Lines name
     * the file they are about, so only lines for this image count: the log
     * spans sessions and images, and its newest line is often another one. */
    FIL file;
    char needle[LIB_NAME_BYTES + 8];
    char needle_flash[LIB_NAME_BYTES + 8];
    char needle_state[LIB_NAME_BYTES + 8];
    td_saves_attempt_t ram = {};
    td_saves_attempt_t flash = {};
    td_saves_attempt_t state = {};

    snprintf(needle, sizeof needle, "/%s.ram", base);
    snprintf(needle_flash, sizeof needle_flash, "/%s.flash", base);
    snprintf(needle_state, sizeof needle_state, "/%s.state", base);
    if (f_open(&file, "/sd/root/saves/last.log", FA_READ) == FR_OK) {
        char buf[256];
        UINT got = 0;
        /* One reader per channel: a line can only belong to one of them, and
         * each keeps its half-read line across chunk boundaries. */
        td_saves_reader_t reader_state;
        td_saves_reader_t reader_ram;
        td_saves_reader_t reader_flash;
        td_saves_reader_init(&reader_state);
        td_saves_reader_init(&reader_ram);
        td_saves_reader_init(&reader_flash);

        while (f_read(&file, buf, sizeof buf, &got) == FR_OK && got > 0) {
            td_saves_reader_feed(&reader_state, buf, got, needle_state, &state);
            td_saves_reader_feed(&reader_ram, buf, got, needle, &ram);
            td_saves_reader_feed(&reader_flash, buf, got, needle_flash, &flash);
        }
        (void)f_close(&file);
    }

    describe(state, "State", text, sizeof text);
    td_widget_set_text(s_saves_state_log, text);
    describe(ram, "RAM", text, sizeof text);
    td_widget_set_text(s_saves_ram_log, text);
    describe(flash, "Flash", text, sizeof text);
    td_widget_set_text(s_saves_flash_log, text);
    if (state.armed && state.verdict[0] == '\0') {
        td_widget_set_text(s_saves_hint,
                           "The run ended before the save finished");
    } else if (state.verdict[0] != '\0' && strcmp(state.verdict, "whole") != 0) {
        td_widget_set_text(s_saves_hint,
                           "Did not verify -- Ctrl+S again, then Refresh");
    } else if (state.verdict[0] == '\0') {
        td_widget_set_text(s_saves_hint, "No save yet -- Ctrl+S in the app");
    }
}

void on_tab(td_widget_t *, void *ctx) { show_tab(static_cast<int>(reinterpret_cast<intptr_t>(ctx))); }
void on_saves_refresh(td_widget_t *, void *) { saves_refresh(); }

/* ----------------------------------------------------------------- window */

void launch(void)
{
    if (s_win != nullptr && td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }

    td_window_desc_t d = {};
    d.title = "Tiny89";
    d.rect = td_rect(-1, -1, WIN_W, WIN_H);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE;
    d.min_w = WIN_W;
    d.min_h = WIN_H;
    d.on_close = on_close;
    d.on_tick = on_tick;
    d.tick_ms = TICK_MS;
    d.icon = "TI89";
    s_win = td_win_create(&d);
    if (s_win == nullptr) {
        return;
    }

    memset(s_page_count, 0, sizeof s_page_count);

    /* The tab row, spaced by caption so the labels keep a column between them. */
    int tab_x = 1;
    s_tab[0] = td_button(s_win, tab_x, 0, "Library", on_tab, (void *)0);
    tab_x += static_cast<int>(strlen("Library")) + 5;
    s_tab[1] = td_button(s_win, tab_x, 0, "Saves", on_tab, (void *)1);

    set_path(MEDIA_ROOT);
    s_path = td_label(s_win, 1, 2, 0, "");
    s_list = td_list(s_win, td_rect(1, 3, -1, LIB_ROWS), get_item, on_activate,
                     nullptr);
    td_scrollbar(s_win, -1, 3, LIB_ROWS, s_list);
    s_status = td_label(s_win, 1, 17, 0, "");
    td_widget_set_color(s_status, td_theme()->dim, TD_COLOR_DEFAULT);

    page_add(0, s_path);
    page_add(0, s_list);
    page_add(0, td_button(s_win, 1, 16, "Launch", on_launch, nullptr));
    page_add(0, td_button(s_win, 10, 16, "Up", on_up, nullptr));
    page_add(0, td_button(s_win, 15, 16, "Refresh", on_refresh, nullptr));
    page_add(0, s_status);

    /* What the card holds, and what the board's log says about the last
     * attempt on each channel, with the RTC's time on it. */
    s_saves_path = td_label(s_win, 1, 2, 0, "");
    s_saves_state = td_label(s_win, 1, 3, 0, "");
    s_saves_ram = td_label(s_win, 1, 4, 0, "");
    s_saves_flash = td_label(s_win, 1, 5, 0, "");
    s_saves_state_log = td_label(s_win, 1, 7, 0, "");
    s_saves_ram_log = td_label(s_win, 1, 8, 0, "");
    s_saves_flash_log = td_label(s_win, 1, 9, 0, "");
    s_saves_hint = td_label(s_win, 1, 11, 0, "");
    td_widget_set_color(s_saves_hint, td_theme()->dim, TD_COLOR_DEFAULT);
    page_add(1, s_saves_path);
    page_add(1, s_saves_state);
    page_add(1, s_saves_ram);
    page_add(1, s_saves_flash);
    page_add(1, s_saves_state_log);
    page_add(1, s_saves_ram_log);
    page_add(1, s_saves_flash_log);
    page_add(1, s_saves_hint);
    page_add(1, td_button(s_win, 1, 16, "Refresh", on_saves_refresh, nullptr));

    s_launching = false;
    s_reload = false;
    s_tab_shown = -1;
    show_tab(0);
    load();
    saves_refresh();
}

const td_app_t s_app = { "Tiny89", launch, "TI89" };

} // namespace

// C linkage: the desktop's init file is C, and calls this by name.
extern "C" void td_tiny89_register(void)
{
    (void)td_app_register(&s_app);
}
