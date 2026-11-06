// TinySNES -- the window that starts SNES cartridges.
//
// The shape of the TinyGBC app beside it, cut to the Library page: browse
// /root/media/snes on the card and start what is picked.  TinySNES has no CPU
// to read the card itself, so a launch loads /cores/console138k/tinysnes.bin
// and then streams the cartridge into it from here (tang_snes_load.c), the
// way `snesload` does.  The game then runs with the desk layer hidden; HOME
// on the pad brings TinyDesk up over it, and HOME with SELECT (Minus) stores
// the cartridge's battery save to the card and puts the desktop core back --
// the gesture TinyNES and TinyGBC use, so the three systems end a game the
// same way.  There is no Stop button: a game ends by the pad, and the save is
// written on the way out.
//
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The TinyDesk and shell headers are C, and their symbols are unmangled, so
// they are included with C linkage as the other apps do it.
extern "C" {
#include "ff.h"
#include "td_apps.h"
#include "tinydesk/td.h"
#include "tdsh.h"
#include "tang_core_load.h"
#include "tang_fpga_link.h"
#include "tang_osd.h"
#include "tang_snes_load.h"

/* The USB console writer, as tinygbc_app.cpp declares it. */
int tdsh_printf(const char *fmt, ...);
}

#include "FreeRTOS.h"
#include "task.h"

namespace {

const int TICK_MS = 400;
const int LIB_MAX_ENTRIES = 256;
const int LIB_NAME_BYTES = 56;

/* FatFS's volume-label bit, which this SDK's ff.h leaves out. */
constexpr BYTE AM_VOLUME_LABEL = 0x08;

/* The two cores a launch moves between, as FatFS mounts them. */
constexpr char CORE_REAL[] = "/sd/root/cores/console138k/tinysnes.bin";
constexpr char DESKTOP_REAL[] = "/sd/root/cores/console138k/tinytang.bin";

/* Where the library opens, as the shell shows it; the task works in /sd. */
constexpr char SNES_ROOT[] = "/root/media/snes";

/* 8 KiB: a core load and a stream, no interpreter. */
constexpr uint32_t LAUNCH_STACK_WORDS = 2048;

/* The size TinyNES and TinyGBC use, so the three windows match: the list on
 * rows 3 to 14, the buttons on row 16 and the status line on row 17. */
constexpr int WIN_W = 54;
constexpr int WIN_H = 20;
constexpr int LIB_ROWS = 12;

td_window_t *s_win;

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
char s_launch_cart[TD_PATH_MAX + 4];

/* A launch or a stop owns the core and the link until it is done, so a
 * second is refused rather than reloading the core under the first.  The
 * task leaves its outcome in s_task_say; the tick, which runs on the
 * desktop's own task, is what puts it on screen. */
volatile bool s_launching;
volatile bool s_lib_reload;
volatile bool s_playing;
char s_task_say[TD_TEXT_MAX];

/* Declared here because launch_task starts the exit watcher, which is defined
 * further down beside the rest of the task code. */
void play_task(void *);

/* ----------------------------------------------------------------- the list */

void lib_say(const char *text)
{
    if (s_lib_message != nullptr && strcmp(td_widget_text(s_lib_message), text) != 0) {
        td_widget_set_text(s_lib_message, text);
    }
}

bool lib_at_root(void) { return strcmp(s_lib_shown, SNES_ROOT) == 0; }

void lib_set_path(const char *shown)
{
    snprintf(s_lib_shown, sizeof s_lib_shown, "%s", shown);
    snprintf(s_lib_real, sizeof s_lib_real, "/sd%s", shown);
}

/* A name that can be carried unchanged into a path. */
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

/* The images the loader reads.  A .zip is not offered: the BL616 has no
 * inflater, and the core takes only the raw image. */
bool lib_is_cartridge(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot != nullptr && (strcasecmp(dot, ".sfc") == 0 ||
                              strcasecmp(dot, ".smc") == 0);
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
    return (info.fattrib & AM_DIR) != 0 || lib_is_cartridge(info.fname);
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
        lib_say(lib_at_root() ? "No /root/media/snes folder on the card"
                              : "No folder here on the card");
        return;
    }
    const bool at_root = lib_at_root();

    /* Counted before anything is allocated, so a small folder costs little. */
    const int parent = at_root ? 0 : 1;
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
    } else if (at_root && s_lib_count == 0) {
        snprintf(text, sizeof text, "No .sfc or .smc files here");
    } else {
        snprintf(text, sizeof text, "%d entr%s", s_lib_count, s_lib_count == 1 ? "y" : "ies");
    }
    lib_say(text);
}

/* ---------------------------------------------------------------- actions */

/* What the desk's re-arm after the next core load will put back, recorded
 * without sending: the core that has it now is about to be replaced. */
void overlay_record(bool shown)
{
    tang_fpga_lock();
    tang_osd_note_locked(shown);
    tang_fpga_unlock();
}

/* Load the TinySNES core, then stream the cartridge into it.  The game keeps
 * running after the task ends; nothing here waits for it.
 *
 * The overlay is recorded hidden before the core load, so the desk's re-arm
 * leaves the new core's colour bars on screen while the cartridge streams,
 * as TinyNES's and TinyGBC's loads show them.  Hiding it with a frame instead
 * would blank the desktop for the length of the core load; recorded only, the
 * desktop stays up until the FPGA is reconfigured under it.  Every way back
 * to the desktop core records it shown first, or the desktop would come back
 * hidden. */
void launch_task(void *)
{
    overlay_record(false);
    const tang_load_result_t loaded = tang_core_load(CORE_REAL);
    if (loaded != TANG_LOAD_OK) {
        overlay_record(true);
    }
    if (loaded == TANG_LOAD_OK) {
        char why[96];
        const int rc = tang_snes_load(s_launch_cart, why, sizeof why);
        tdsh_printf("snes: %s: %s\r\n", s_launch_cart, why);
        if (rc == 0) {
            s_playing = true;
            snprintf(s_task_say, sizeof s_task_say,
                     "Playing.  HOME shows the desk; HOME+SELECT saves and ends.");
            /* The exit gesture is watched from here on, so the desk's own
             * task never has to poll for it. */
            (void)xTaskCreate(play_task, "snesplay", LAUNCH_STACK_WORDS, nullptr,
                              tskIDLE_PRIORITY + 1, nullptr);
        } else {
            /* The SNES core is loaded but holds no game: put the desktop
             * core back rather than leave the bars up. */
            overlay_record(true);
            (void)tang_core_load(DESKTOP_REAL);
            s_playing = false;
            snprintf(s_task_say, sizeof s_task_say, "Not started: %s", why);
        }
    } else if (loaded == TANG_LOAD_BUSY) {
        snprintf(s_task_say, sizeof s_task_say, "Another core load is running");
    } else {
        snprintf(s_task_say, sizeof s_task_say, "The TinySNES core did not load");
    }
    s_launching = false;
    s_lib_reload = true;
    vTaskDelete(NULL);
}

/* How often the exit task looks at the pad.  The core reports a change within
 * 20 ms of it happening, so 50 ms sees a held chord on the next look. */
constexpr uint32_t PLAY_POLL_MS = 50;

/* Watch the pad while a game runs, then store the cartridge's save and put
 * the desktop core back.  One task does both: the watch is a loop and the
 * exit is what ends it, so a second task would only buy a race and another
 * 8 KiB of stack.  HOME with SELECT is the gesture, bit 15 of the pad word;
 * everything else the pad reports belongs to the game. */
void play_task(void *)
{
    for (;;) {
        uint16_t joy1 = 0, joy2 = 0;
        tang_fpga_joypad(&joy1, &joy2);
        if ((joy1 & TANG_SNES_PAD_EXIT) != 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(PLAY_POLL_MS));
    }

    /* Own the core from here until the task ends, so the desk's Launch cannot
     * start a second cartridge into the one being saved. */
    s_launching = true;
    lib_say("Saving ...");

    char sav[256];
    uint32_t size = 0;
    char why[128];

    if (tang_snes_save_for(s_launch_cart, &size, sav, sizeof sav) != 0) {
        /* No battery, so nothing to write; the game still ends. */
        tdsh_printf("snes: %s has no battery save\r\n", s_launch_cart);
    } else if (tang_snes_save_write(sav, size, why, sizeof why) != 0) {
        /* The core is held in reset from the failed readout.  Give the game
         * back rather than leave the player with a frozen picture, and say
         * what went wrong instead of pretending the save happened. */
        tang_snes_resume();
        s_launching = false;
        snprintf(s_task_say, sizeof s_task_say, "Save failed: %s", why);
        tdsh_printf("snes: save failed: %s\r\n", why);
        vTaskDelete(NULL);
        return;
    } else {
        tdsh_printf("snes: %s\r\n", why);
    }

    overlay_record(true);
    const tang_load_result_t loaded = tang_core_load(DESKTOP_REAL);
    if (loaded == TANG_LOAD_OK) {
        s_playing = false;
        snprintf(s_task_say, sizeof s_task_say, "Saved.  Back at the desk.");
    } else {
        /* The desktop did not load and the SNES core is held in reset.
         * Release it so the picture is live again, and report the failure. */
        tang_snes_resume();
        snprintf(s_task_say, sizeof s_task_say,
                 loaded == TANG_LOAD_BUSY ? "Another core load is running"
                                          : "The desktop core did not load");
    }
    s_launching = false;
    s_lib_reload = true;
    vTaskDelete(NULL);
}

bool start_task(TaskFunction_t fn, const char *name, const char *say)
{
    s_task_say[0] = '\0';
    s_launching = true;
    if (xTaskCreate(fn, name, LAUNCH_STACK_WORDS, nullptr,
                    tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
        s_launching = false;
        s_lib_reload = true;
        snprintf(s_task_say, sizeof s_task_say, "Not enough memory for that");
        return false;
    }
    lib_say(say);
    return true;
}

void lib_start(int index)
{
    if (s_launching) {
        lib_say("A core load is running");
        return;
    }
    /* The desk can be brought up over a running game, so the library is
     * reachable while one plays.  Starting a second would give the exit
     * gesture two watchers, so it is refused until the first has ended. */
    if (s_playing) {
        lib_say("A game is running; HOME+SELECT saves and ends it.");
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
    /* The heap is shared with the core load; the names come back after. */
    free(s_lib_entries);
    s_lib_entries = nullptr;
    s_lib_count = 0;
    s_lib_truncated = false;
    if (s_lib_list != nullptr) {
        td_list_set_count(s_lib_list, 0);
    }
    (void)start_task(launch_task, "sneslaunch", "Starting ...");
}

/* Put the task's outcome and the list back once it has finished; the tick
 * is where the desktop's own task can touch widgets. */
void on_tick(td_window_t *)
{
    if (s_launching || !s_lib_reload) {
        return;
    }
    s_lib_reload = false;
    if (td_win_is_open(s_win)) {
        lib_load();
        if (s_task_say[0] != '\0') {
            lib_say(s_task_say);
        }
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
        if (slash == nullptr || slash <= next + strlen(SNES_ROOT)) {
            snprintf(next, sizeof next, "%s", SNES_ROOT);
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
    s_lib_list = nullptr;
    s_lib_path = nullptr;
    s_lib_message = nullptr;
    /* The names are not worth the heap while the window is shut. */
    free(s_lib_entries);
    s_lib_entries = nullptr;
    s_lib_count = 0;
    s_lib_reload = false;
}

void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }

    td_window_desc_t d = {};
    d.title = "TinySNES";
    d.rect = td_rect(-1, -1, WIN_W, WIN_H);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE;
    d.min_w = WIN_W;
    d.min_h = WIN_H;
    d.on_close = on_close;
    d.on_tick = on_tick;
    d.tick_ms = TICK_MS;
    d.icon = "SNS";
    s_win = td_win_create(&d);
    if (s_win == nullptr) {
        return;
    }

    lib_set_path(SNES_ROOT);
    (void)td_label(s_win, 1, 0, 0, "Library");
    s_lib_path = td_label(s_win, 1, 2, 0, "");
    s_lib_list = td_list(s_win, td_rect(1, 3, -1, LIB_ROWS), lib_get_item,
                         lib_on_activate, nullptr);
    td_scrollbar(s_win, -1, 3, LIB_ROWS, s_lib_list);
    (void)td_button(s_win, 1, 16, "Launch", lib_on_launch, nullptr);
    (void)td_button(s_win, 12, 16, "Up", lib_on_up, nullptr);
    (void)td_button(s_win, 19, 16, "Refresh", lib_on_refresh, nullptr);
    s_lib_message = td_label(s_win, 1, 17, 0, "");
    lib_load();
    if (s_lib_list != nullptr) {
        td_list_select(s_lib_list, 0);
    } else {
        lib_say("Not enough memory for the list");
    }
    if (s_launching) {
        lib_say("A core load is running");
    } else if (s_playing) {
        lib_say("A game is running; HOME+SELECT saves and ends it.");
    }
}

const td_app_t s_app = { "TinySNES", launch, "SNS" };

} // namespace

// C linkage: the desktop's init file is C, and calls this by name.
extern "C" void td_tinysnes_register(void)
{
    (void)td_app_register(&s_app);
}
