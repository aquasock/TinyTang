// TinyTang — the TinyGBA app: the cartridge library, page one.
//
// This is the TinyGBA counterpart of tinysnes_app.cpp and is deliberately the
// same shape, because the desktop's habit is that every console app reads the
// same way: a window with the cartridge list on rows 3 to 14, the buttons on
// row 16 and a status line on row 17.  A player who has booted a SNES cart from
// the desktop should not have to learn a second arrangement for a GBA one.
//
// What it does, and what it does not.  This is the Library page only.  Choosing
// a cartridge loads the TinyGBA core from the card and streams the image into
// it with `tang_gba_load()`, the same function the `gbaload` shell command
// calls, so the two cannot drift apart.  The game then runs with the overlay
// hidden; HOME brings the desktop back over it, which the core implements by
// reporting a tap of HOME as the pad word's desk-toggle bit.  The Input, Video,
// Audio and Saves tabs TinySNES carries are not here yet -- they need core
// frames TinyGBA does not implement (a pad map, a video-shape command, a
// volume), so building the pages before the frames would produce controls that
// do nothing.  The battery-save page is the first one that would work, since
// the save path is proven, and it is the natural next page.
//
// SPDX-License-Identifier: MIT

// The TinyDesk and shell headers are C, and their symbols are unmangled, so
// they are included with C linkage as the other apps do it.
extern "C" {
#include "ff.h"
#include "td_apps.h"
#include "tinydesk/td.h"
#include "tdsh.h"
#include "tang_core_load.h"
#include "tang_fpga_link.h"
#include "tang_gba_load.h"
#include "tang_osd.h"

/* The USB console writer, as tinygbc_app.cpp declares it. */
int tdsh_printf(const char *fmt, ...);
}

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace {

/* The size TinyNES, TinyGBC and TinySNES use, so the four windows match: the
 * list on rows 3 to 14, the buttons on row 16 and the status line on row 17. */
constexpr int WIN_W = 54;
constexpr int WIN_H = 20;
constexpr int LIB_ROWS = 12;

/* FatFS's volume-label attribute, spelled out as tinysnes_app.cpp does: the
 * header this build sees does not export the name. */
constexpr BYTE AM_VOLUME_LABEL = 0x08;

constexpr int LIB_MAX_ENTRIES = 256;
constexpr int LIB_NAME_BYTES  = 56;

/* Where the cartridges are.  The real path is what FatFS sees; the shown one is
 * what the window says, because the desktop's root is the card and a player
 * should not have to read `/sd/root` to know where their games are. */
constexpr char GBA_ROOT[] = "/root/media/gba";
constexpr int  GBA_ROOT_LEN = sizeof GBA_ROOT - 1;

/* The core image the launcher programs before streaming a cartridge.  It is the
 * same name `tangload` uses, so flashing a new build to that path is all a
 * rebuild needs. */
constexpr char CORE_REAL[]    = "/sd/root/cores/console138k/tinygba.bin";
constexpr char DESKTOP_REAL[] = "/sd/root/cores/console138k/tinytang.bin";

/* The core load and the stream both block in one task, and the cartridge is up
 * to 32 MiB, so the stack is the same one the SNES app found sufficient. */
constexpr uint32_t LAUNCH_STACK_WORDS = 2048;

/* How often the window looks for a finished launch.  The task cannot touch
 * widgets itself; the desktop's own task does that here. */
constexpr int TICK_MS = 400;

struct lib_entry {
    char name[LIB_NAME_BYTES];
    bool is_dir;
};

td_window_t *s_win;
td_widget_t *s_lib_list;
td_widget_t *s_lib_path;
td_widget_t *s_lib_message;

lib_entry *s_lib_entries;
int        s_lib_count;
bool       s_lib_truncated;
char       s_lib_shown[TD_PATH_MAX];
char       s_lib_real[TD_PATH_MAX + 4];

/* What the launch task carries out of the window and back into it. */
char s_launch_cart[TD_PATH_MAX + 8];
char s_task_say[96];
bool s_launching;
bool s_playing;
bool s_lib_reload;

/* ----------------------------------------------------------------- the list */

void lib_say(const char *text)
{
    if (s_lib_message != nullptr && strcmp(td_widget_text(s_lib_message), text) != 0) {
        td_widget_set_text(s_lib_message, text);
    }
}

bool lib_at_root(void) { return strcmp(s_lib_shown, GBA_ROOT) == 0; }

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

/* The images the loader reads.  Only .gba: a .zip is not offered because the
 * BL616 has no inflater and the core takes the raw image, and the other GBA
 * extensions are not cartridge images at all. */
bool lib_is_cartridge(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot != nullptr && strcasecmp(dot, ".gba") == 0;
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
        lib_say(lib_at_root() ? "No /root/media/gba folder on the card"
                              : "No folder here on the card");
        return;
    }
    const bool at_root = lib_at_root();

    /* The parent entry is only offered below the root, so a player cannot walk
     * out of the cartridge tree into the rest of the card. */
    const int parent = at_root ? 0 : 1;
    int capacity = parent;
    FRESULT result;
    while ((result = f_readdir(&dir, &info)) == FR_OK && info.fname[0] != '\0') {
        if (!lib_listable(info)) {
            continue;
        }
        capacity++;
    }
    (void)f_closedir(&dir);

    if (capacity > LIB_MAX_ENTRIES) {
        capacity = LIB_MAX_ENTRIES;
        s_lib_truncated = true;
    }
    s_lib_entries = static_cast<lib_entry *>(calloc(capacity > 0 ? capacity : 1,
                                                    sizeof(lib_entry)));
    if (s_lib_entries == nullptr) {
        lib_say("Not enough memory for the list");
        return;
    }

    int n = 0;
    if (parent != 0) {
        snprintf(s_lib_entries[n].name, LIB_NAME_BYTES, "%s", "..");
        s_lib_entries[n].is_dir = true;
        n++;
    }
    if (f_opendir(&dir, s_lib_real) == FR_OK) {
        while ((result = f_readdir(&dir, &info)) == FR_OK && info.fname[0] != '\0') {
            if (!lib_listable(info) || n >= capacity) {
                continue;
            }
            snprintf(s_lib_entries[n].name, LIB_NAME_BYTES, "%s", info.fname);
            s_lib_entries[n].is_dir = (info.fattrib & AM_DIR) != 0;
            n++;
        }
        (void)f_closedir(&dir);
    }
    s_lib_count = n;
    qsort(s_lib_entries, s_lib_count, sizeof(lib_entry), lib_compare);

    if (s_lib_list != nullptr) {
        td_list_set_count(s_lib_list, s_lib_count);
    }
    if (s_lib_count == 0) {
        lib_say("No cartridges here");
    } else if (s_lib_truncated) {
        lib_say("Only the first 256 entries are listed");
    } else {
        lib_say("");
    }
}

/* Walk into a folder, or back out of one. */
void lib_descend_to(const char *name)
{
    char next[TD_PATH_MAX];
    if (strcmp(name, "..") == 0) {
        if (lib_at_root()) {
            return;
        }
        snprintf(next, sizeof next, "%s", s_lib_shown);
        char *slash = strrchr(next, '/');
        if (slash != nullptr && slash != next) {
            *slash = '\0';
        } else {
            snprintf(next, sizeof next, "%s", GBA_ROOT);
        }
    } else {
        const int n = snprintf(next, sizeof next, "%s/%s", s_lib_shown, name);
        if (n <= 0 || n >= static_cast<int>(sizeof next)) {
            lib_say("That path is too long");
            return;
        }
    }
    lib_set_path(next);
    lib_load();
    if (s_lib_list != nullptr && s_lib_count > 0) {
        td_list_select(s_lib_list, 0);
    }
}

/* ------------------------------------------------------------ the launch */

/* Tell the OSD module the overlay's new state without sending a frame: the core
 * that has it now is about to be replaced. */
void overlay_record(bool shown)
{
    tang_fpga_lock();
    tang_osd_note_locked(shown);
    tang_fpga_unlock();
}

/* Declared here because launch_task starts the exit watcher, which is defined
 * below it; the watcher needs the same statics, so one order has to be chosen. */
void play_task(void *);

/* Program the TinyGBA core, then stream the cartridge into it.  The game keeps
 * running after the task ends; nothing here waits for it.
 *
 * The overlay is recorded hidden before the core load, so the new core's own
 * picture is what appears while the cartridge streams.  Hiding it with a frame
 * instead would blank the desktop for the whole core load; recorded only, the
 * desktop stays up until the FPGA is reconfigured under it. */
void launch_task(void *)
{
    overlay_record(false);
    const tang_load_result_t loaded = tang_core_load(CORE_REAL);
    bool ok = false;
    if (loaded == TANG_LOAD_OK) {
        char why[160] = "";
        const int rc = tang_gba_load(s_launch_cart, s_launch_cart, why, sizeof why);
        if (rc == 0) {
            ok = true;
            s_playing = true;
            /* The game is up, so hand the pad to the watcher: from here HOME
             * shows the desk and HOME+SELECT ends the game. */
            if (xTaskCreate(play_task, "gbplay", LAUNCH_STACK_WORDS, nullptr,
                            tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
                snprintf(s_task_say, sizeof s_task_say,
                         "Playing, but the exit key will not work");
                tdsh_printf("tinygba: no memory for the exit watcher\r\n");
                vTaskDelete(NULL);
                return;
            }
            snprintf(s_task_say, sizeof s_task_say,
                     "Playing.  HOME shows the desk, minus+HOME ends.");
        } else {
            snprintf(s_task_say, sizeof s_task_say, "Not started: %s", why);
            tdsh_printf("tinygba: %s: %s\r\n", s_launch_cart, why);
        }
    } else if (loaded == TANG_LOAD_BUSY) {
        snprintf(s_task_say, sizeof s_task_say, "Another core load is running");
    } else {
        snprintf(s_task_say, sizeof s_task_say, "The TinyGBA core did not load");
    }

    if (!ok) {
        /* Either the core never loaded or it loaded with no cartridge in it.
         * Put the desktop core back rather than leave an empty machine up. */
        overlay_record(true);
        (void)tang_core_load(DESKTOP_REAL);
        s_playing = false;
    }
    s_launching = false;
    s_lib_reload = true;
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------- the exit --
 *
 * How often the watcher looks at the pad.  The core reports a change within
 * 20 ms, and the exit is a held level rather than a tap, so 50 ms cannot miss a
 * chord the player is still holding. */
constexpr uint32_t PLAY_POLL_MS = 50;

/* Watch the pad while a game runs, then store the cartridge's save and put the
 * desktop core back.  Same shape as tinysnes_app.cpp's play_task for the same
 * reason: one task whose loop is ended by the thing it watches, rather than a
 * watcher and a finisher racing each other.
 *
 * The save is best-effort and the exit is not.  TCL-064 records that every
 * console death this project has seen happened during an SD write -- twice in
 * this very command -- so a failed or hung save must not be allowed to strand a
 * player inside a game with no way out.  If the save fails, the desktop comes
 * back anyway and the status line says so. */
void play_task(void *)
{
    for (;;) {
        uint16_t joy1 = 0, joy2 = 0;
        tang_fpga_joypad(&joy1, &joy2);
        if ((joy1 & TANG_GBA_PAD_EXIT) != 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(PLAY_POLL_MS));
    }

    /* Own the core from here until this task ends, so a Launch from the desk
     * cannot stream a second cartridge into the one being saved. */
    s_launching = true;
    s_task_say[0] = '\0';

    char why[160] = "";
    const int saved = tang_gba_save(s_launch_cart, s_launch_cart, why, sizeof why);

    overlay_record(true);
    if (tang_core_load(DESKTOP_REAL) != TANG_LOAD_OK) {
        snprintf(s_task_say, sizeof s_task_say, "Save stored, but the desktop did not come back");
    } else if (saved != 0) {
        snprintf(s_task_say, sizeof s_task_say, "Left the game, save not kept: %s", why);
    } else {
        snprintf(s_task_say, sizeof s_task_say, "Save stored.");
    }
    s_playing = false;
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
    (void)start_task(launch_task, "gblaunch", "Starting ...");
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

/* Put the task's outcome and the list back once it has finished; the tick is
 * where the desktop's own task can touch widgets. */
void on_tick(td_window_t *)
{
    if (s_launching || !s_lib_reload) {
        return;
    }
    s_lib_reload = false;
    if (s_lib_entries == nullptr) {
        lib_set_path(lib_at_root() ? GBA_ROOT : s_lib_shown);
        lib_load();
    }
    if (s_task_say[0] != '\0') {
        lib_say(s_task_say);
    }
}

void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }

    td_window_desc_t d = {};
    d.title   = "TinyGBA";
    d.rect    = td_rect(-1, -1, WIN_W, WIN_H);
    d.flags   = TD_WIN_MOVABLE | TD_WIN_CLOSABLE;
    d.min_w   = WIN_W;
    d.min_h   = WIN_H;
    d.on_close = on_close;
    d.on_tick  = on_tick;
    d.tick_ms  = TICK_MS;
    d.icon     = "GB";
    s_win = td_win_create(&d);
    if (s_win == nullptr) {
        tdsh_printf("tinygba: could not open the window\r\n");
        return;
    }

    lib_set_path(GBA_ROOT);
    s_lib_path = td_label(s_win, 1, 2, 0, "");
    s_lib_list = td_list(s_win, td_rect(1, 3, -1, LIB_ROWS), lib_get_item,
                         lib_on_activate, nullptr);
    td_scrollbar(s_win, -1, 3, LIB_ROWS, s_lib_list);
    td_button(s_win, 1, 16, "Launch", lib_on_launch, nullptr);
    td_button(s_win, 12, 16, "Up", lib_on_up, nullptr);
    td_button(s_win, 19, 16, "Refresh", lib_on_refresh, nullptr);
    s_lib_message = td_label(s_win, 1, 17, 0, "");

    lib_load();
    if (s_lib_list != nullptr) {
        td_list_select(s_lib_list, 0);
    }
    if (s_launching) {
        lib_say("A core load is running");
    } else if (s_playing) {
        lib_say("A game is running; HOME shows the desk.");
    }
}

const td_app_t s_app = { "TinyGBA", launch, "GB" };

} // namespace

// C linkage: the desktop's init file is C, and calls this by name.
extern "C" void td_tinygba_register(void)
{
    (void)td_app_register(&s_app);
}
