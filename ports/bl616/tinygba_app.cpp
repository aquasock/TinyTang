// TinyTang — the TinyGBA app: the cartridge library, page one.
//
// This is the TinyGBA counterpart of tinysnes_app.cpp and is deliberately the
// same shape, because the desktop's habit is that every console app reads the
// same way: a window with the cartridge list on rows 3 to 14, the buttons on
// row 16 and a status line on row 17.  A player who has booted a SNES cart from
// the desktop should not have to learn a second arrangement for a GBA one.
//
// What it does, and what it does not.  The window carries TinySNES's five tabs.
// Library: choosing a cartridge loads the TinyGBA core from the card and
// streams the image into it with `tang_gba_load()`, the same function the
// `gbaload` shell command calls, so the two cannot drift apart.  The game then
// runs with the overlay hidden; HOME brings the desktop back over it, which the
// core implements by reporting a tap of HOME as the pad word's desk-toggle bit,
// and minus+HOME saves and ends it.  Saves: the selected cartridge's `.sav`, its
// save type and size, and when it was last written, from the board's save log.
// Input: the pad map, /tinygba.ini's [input], which tang_gba_load() sends to
// the core as frame 0x1A at every launch (tang_gba_ini.h).  Each controller
// button names the GBA key it presses -- the other way round from TinySNES's
// page, because the GBA's default puts two buttons on one key.  Video and Audio
// are laid out as TinySNES's are but hold placeholder values and change
// nothing: TinyGBA implements no frame yet for a picture shape or a volume, so
// each says so on its status line rather than pretending to save.
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
#include "tang_gba_ini.h"
#include "tang_gba_load.h"
#include "tang_osd.h"
#include "td_saves_log.h"

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
/* The Input and Video pages, laid out as TinyNES's: a list in the 8-column
 * name, and two clear of its right edge the column its actions stand in.  A
 * row is drawn from the list's second column.  Input's widest row,
 * "Right    SELECT *", is 17, so its list is TinyNES's 18; Video's,
 * "Aspect  3:2, 1080x720 in a 1280 frame", is 37, so its list is 38. */
constexpr int INPUT_LIST_W = 18;
constexpr int INPUT_ACTION_X = 1 + INPUT_LIST_W + 2;
constexpr int VIDEO_LIST_W = 38;
constexpr int VIDEO_ACTION_X = 1 + VIDEO_LIST_W + 2;
/* The Audio page is one row: the slider, its reading, at most "100%", then
 * the mute and the Save, each two clear of the one before. */
constexpr int VOLUME_SLIDER_W = 20;
constexpr int VOLUME_LABEL_X = 1 + VOLUME_SLIDER_W + 2;
constexpr int VOLUME_LABEL_W = 4;
constexpr int MUTE_X = VOLUME_LABEL_X + VOLUME_LABEL_W + 2;
constexpr int AUDIO_SAVE_X = MUTE_X + 8 + 2;   /* "Mute" and its box are 8 */

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

/* --------------------------------------------------------------- the tabs */

/* TinySNES's tab set, so the consoles' windows read the same. */
const int TABS = 5;
const int PAGE_WIDGETS = 16;
const char *const TAB_NAMES[TABS] = { "Library", "Input", "Video", "Audio", "Saves" };
const int INPUT_TAB = 1;
const int VIDEO_TAB = 2;
const int AUDIO_TAB = 3;
const int SAVES_TAB = 4;

/* There is no named accent, only TD_COLOR_DEFAULT and raw palette indices. */
const int ACCENT = 14;

td_widget_t *s_tab[TABS];
td_widget_t *s_page[TABS][PAGE_WIDGETS];
int s_page_count[TABS];
int s_tab_shown = -1;
td_widget_t *s_status[TABS];   /* one per page: a shared widget cannot be shown
                                * on one page and hidden on another */

/* The Input page: the file as read when the window opened, the map being
 * edited, and whether it differs from what was last saved. */
tang_gba_settings_t s_set;
uint8_t s_map[TANG_GBA_SOURCES];
bool s_dirty;
const char INPUT_HELP[] = "Double-click or Enter steps a row; Save keeps it.";

/* Video and Audio are placeholders: the values are what a default would look
 * like, and no frame carries them to the core. */
const int VIDEO_ROWS = 2;
const char PLACEHOLDER[] = "Placeholder: not connected to the core yet.";
td_widget_t *s_input_list;
td_widget_t *s_video_list;
td_widget_t *s_volume;
td_widget_t *s_volume_label;

/* The Saves page.  A GBA cartridge's save type is a marker somewhere in its
 * image, not a header field, so finding it reads the image -- seconds for a
 * large cartridge -- and that runs in a task of its own.  The answer is kept
 * for the cartridge it was found for, so the page is instant the second time. */
/* The page, as TinyNES's: the cartridge, rows for its save and its save type
 * in the 8-column name, the folder, and under them, dimmed, what the file
 * holds -- a heading and two lines. */
const int DETAIL_LINES = 3;
td_widget_t *s_save_name;
td_widget_t *s_save_line;   /* when the save was last written */
td_widget_t *s_type_line;   /* the save type the image's marker names */
td_widget_t *s_folder_line;
td_widget_t *s_detail[DETAIL_LINES];
char s_scan_cart[TD_PATH_MAX + 8];      /* what the last scan was for */
volatile uint8_t s_scan_type;
volatile bool s_scan_known;             /* s_scan_type answers s_scan_cart */
volatile bool s_scan_running;
volatile bool s_scan_done;              /* finished since the tick last looked */

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

/* ------------------------------------------------------------------ pages */

void page_add(int tab, td_widget_t *w)
{
    if (w != nullptr && s_page_count[tab] < PAGE_WIDGETS) {
        s_page[tab][s_page_count[tab]++] = w;
    }
}

/* The status line of whichever page is showing.  The Library keeps its own
 * through lib_say, because a launch reports there whatever page is up. */
void page_say(const char *text)
{
    if (s_tab_shown >= 0 && s_tab_shown < TABS && s_status[s_tab_shown] != nullptr &&
        strcmp(td_widget_text(s_status[s_tab_shown]), text) != 0) {
        td_widget_set_text(s_status[s_tab_shown], text);
    }
}

void saves_show(void);

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
    /* The Saves page reads the card, so it is filled when it is shown rather
     * than on every tick -- and the selection it describes can only change on
     * the Library page. */
    if (tab == SAVES_TAB) {
        saves_show();
    }
}

void on_tab(td_widget_t *, void *arg)
{
    show_tab(static_cast<int>(reinterpret_cast<uintptr_t>(arg)));
}

/* -------------------------------------------------------------- the input */

/* One row: a controller button, and the GBA key it presses.  Every button
 * always has a row -- one that presses nothing says "none" rather than
 * disappearing -- so the list never changes height under the player.  The
 * star marks a map that differs from the file. */
const char *input_item(td_widget_t *, int index, int *fg, void *)
{
    static char text[48];
    if (index < 0 || index >= TANG_GBA_SOURCES) {
        return "";
    }
    const int key = s_map[index];
    snprintf(text, sizeof text, "%-8s %s%s", tang_gba_source_name(index),
             tang_gba_key_name(key), s_dirty ? " *" : "");
    *fg = key >= TANG_GBA_KEY_NONE ? td_theme()->dim : TD_COLOR_DEFAULT;
    return text;
}

void input_changed(const char *say)
{
    s_dirty = memcmp(s_map, s_set.map, sizeof s_map) != 0;
    if (s_win != nullptr) {
        td_win_invalidate(s_win);
    }
    page_say(say);
}

/* Steps the selected row through the ten keys and then none, so one button
 * reaches every state a binding can hold. */
void on_input_row(td_widget_t *, void *)
{
    const int row = s_input_list != nullptr ? td_list_selected(s_input_list) : -1;
    if (row < 0 || row >= TANG_GBA_SOURCES) {
        return;
    }
    s_map[row] = (uint8_t)((s_map[row] + 1) % (TANG_GBA_KEY_NONE + 1));
    input_changed(INPUT_HELP);
}

void on_restore(td_widget_t *, void *)
{
    tang_gba_default_map(s_map);
    input_changed("Restored the default map; Save keeps it.");
}

void on_input_save(td_widget_t *, void *)
{
    char why[96];
    tang_gba_settings_t next = s_set;
    memcpy(next.map, s_map, sizeof s_map);
    if (tang_gba_settings_save(&next, TANG_GBA_INI_REAL, why, sizeof why) != 0) {
        page_say(why);
        return;
    }
    s_set = next;
    input_changed("Saved. Taken when the next game starts.");
}

/* The file as it is on the card now, for a window that has just opened. */
void input_load(void)
{
    (void)tang_gba_settings_load(&s_set, TANG_GBA_INI_REAL);
    memcpy(s_map, s_set.map, sizeof s_map);
    s_dirty = false;
}

/* ------------------------------------------------- the placeholder pages */

/* Video: two rows in TinySNES's shape, holding what the core draws now. */
const char *video_item(td_widget_t *, int index, int *fg, void *)
{
    (void)fg;
    if (index == 0) {
        return "Aspect  3:2, 1080x720 in a 1280 frame";
    }
    if (index == 1) {
        return "Bars    black";
    }
    return "";
}

void on_placeholder(td_widget_t *, void *)
{
    page_say(PLACEHOLDER);
}

void show_volume(void)
{
    if (s_volume_label != nullptr && s_volume != nullptr) {
        td_widget_printf(s_volume_label, "%3u%%", (unsigned)s_volume->value);
    }
}

void on_volume(td_widget_t *, void *)
{
    show_volume();
    page_say(PLACEHOLDER);
    if (s_win != nullptr) {
        td_win_invalidate(s_win);
    }
}

/* ----------------------------------------------------------------- the saves */

/* One saved file's last record, in the page's words: when, and what came of
 * it, as TinyNES's page says it.  The stamp is the RTC-kept clock's local time
 * as the firmware wrote it; a save made before the clock had been set says so
 * rather than passing "no-clock" off as a date. */
void describe_save(const td_saves_attempt_t &rec, char *out, size_t cap)
{
    char when[32];

    td_saves_when(rec.stamp, when, sizeof when);
    if (rec.verdict[0] != '\0' && strcmp(rec.stamp, "no-clock") == 0) {
        snprintf(out, cap, "%s, clock was unset then", rec.verdict);
    } else if (rec.verdict[0] != '\0') {
        snprintf(out, cap, "%s, %s", when, rec.verdict);
    } else {
        snprintf(out, cap, "%s, did not finish", when);
    }
}

/* A row with its 8-column name, as the Input and Video pages lay theirs. */
void saves_row(td_widget_t *w, const char *name, const char *text)
{
    char row[TD_TEXT_MAX];

    if (w != nullptr) {
        snprintf(row, sizeof row, "%-8s%s", name, text);
        td_widget_set_text(w, row);
    }
}

/* The details are secondary, so they take the theme's dim colour, read each
 * time in case the theme has changed since the window opened. */
void saves_details(const char *const text[DETAIL_LINES])
{
    for (int i = 0; i < DETAIL_LINES; i++) {
        if (s_detail[i] != nullptr) {
            td_widget_set_color(s_detail[i], td_theme()->dim, TD_COLOR_DEFAULT);
            td_widget_set_text(s_detail[i], text[i] != nullptr ? text[i] : "");
        }
    }
    if (s_win != nullptr) {
        td_win_invalidate(s_win);
    }
}

/* Find the save type of s_scan_cart.  The path is copied before the read, and
 * nothing changes s_scan_cart while this runs, so the answer belongs to it. */
void scan_task(void *)
{
    char cart[sizeof s_scan_cart];
    snprintf(cart, sizeof cart, "%s", s_scan_cart);
    uint8_t type = TANG_GBA_BACKUP_NONE;
    const int rc = tang_gba_save_info(cart, &type);
    s_scan_type = type;
    s_scan_known = rc == 0;
    s_scan_running = false;
    s_scan_done = true;
    vTaskDelete(NULL);
}

/* The selected cartridge's save: where it lives, what kind it is and how big
 * it should be, how big it is, and when the last attempt to write it ran.
 * FatFS gives the file no date, so the time comes from the board's save log
 * (td_saves_log.h), which tang_gba_save stamps as it writes.  The same lines
 * as TinySNES's page, except that a GBA save sits beside its cartridge rather
 * than under /root/saves, and its size comes from the image's save marker. */
void saves_show(void)
{
    static char detail[DETAIL_LINES][TD_TEXT_MAX];
    const char *details[DETAIL_LINES] = {};
    char text[TD_TEXT_MAX];
    const int row = s_lib_list != nullptr ? td_list_selected(s_lib_list) : -1;

    if (s_lib_entries == nullptr || row < 0 || row >= s_lib_count ||
        s_lib_entries[row].is_dir) {
        if (s_save_name != nullptr) {
            td_widget_set_text(s_save_name, "Select a cartridge in the Library.");
        }
        saves_row(s_save_line, "Save", "");
        saves_row(s_type_line, "Type", "");
        saves_row(s_folder_line, "Folder", "");
        saves_details(details);
        return;
    }
    const char *name = s_lib_entries[row].name;
    const char *dot = strrchr(name, '.');
    const int stem = dot != nullptr && dot != name ? (int)(dot - name) : (int)strlen(name);

    char cart_real[TD_PATH_MAX + 8];
    snprintf(cart_real, sizeof cart_real, "%s/%s", s_lib_real, name);
    char save_real[TD_PATH_MAX + 16];
    snprintf(save_real, sizeof save_real, "%s.sav", cart_real);

    if (s_save_name != nullptr) {
        snprintf(text, sizeof text, "%.*s", stem, name);
        td_widget_set_text(s_save_name, text);
    }
    /* A GBA save sits beside its cartridge, as <name>.sav. */
    saves_row(s_folder_line, "Folder", s_lib_shown);

    /* The save type: known, being found, or to be found. */
    const bool known = s_scan_known && strcmp(s_scan_cart, cart_real) == 0;
    uint8_t type = known ? s_scan_type : TANG_GBA_BACKUP_NONE;
    if (!known && !s_scan_running && !s_launching) {
        snprintf(s_scan_cart, sizeof s_scan_cart, "%s", cart_real);
        s_scan_known = false;
        s_scan_running = true;
        if (xTaskCreate(scan_task, "gbscan", LAUNCH_STACK_WORDS, nullptr,
                        tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
            s_scan_running = false;
        }
    }
    if (known && type == TANG_GBA_BACKUP_NONE) {
        saves_row(s_save_line, "Save", "none, no save hardware in this cartridge");
        saves_row(s_type_line, "Type", "none");
        details[0] = "Save details";
        details[1] = "Nothing is written when it ends.";
        saves_details(details);
        return;
    }
    if (known) {
        snprintf(text, sizeof text, "%s, %lu B", tang_gba_save_type_name(type),
                 (unsigned long)tang_gba_save_bytes(type));
        saves_row(s_type_line, "Type", text);
    } else {
        saves_row(s_type_line, "Type", s_launching ? "found when the load is done"
                                                   : "checking the cartridge ...");
    }

    FIL file;
    if (f_open(&file, save_real, FA_READ) != FR_OK) {
        saves_row(s_save_line, "Save", "none yet");
        saves_details(details);
        return;
    }
    const uint32_t size = (uint32_t)f_size(&file);
    (void)f_close(&file);
    details[0] = "Save details";
    if (known) {
        const uint32_t want = tang_gba_save_bytes(type);
        snprintf(detail[1], sizeof detail[1], "%lu B, %s holds %lu",
                 (unsigned long)size, tang_gba_save_type_name(type),
                 (unsigned long)want);
        if (size != want) {
            details[2] = "Differs from the cartridge; it will not load.";
        }
    } else {
        snprintf(detail[1], sizeof detail[1], "%lu B", (unsigned long)size);
    }
    details[1] = detail[1];

    /* When it was written: the log's record, since the file itself has none. */
    char needle[LIB_NAME_BYTES + 8];
    snprintf(needle, sizeof needle, "/%s.sav", name);
    td_saves_attempt_t rec = {};
    td_saves_reader_t reader;
    td_saves_reader_init(&reader);
    FIL log;
    if (f_open(&log, "/sd/root/saves/last.log", FA_READ) == FR_OK) {
        char buf[256];
        UINT got = 0;
        while (f_read(&log, buf, sizeof buf, &got) == FR_OK && got > 0) {
            td_saves_reader_feed(&reader, buf, got, needle, &rec);
        }
        (void)f_close(&log);
    }
    if (rec.verdict[0] != '\0' || rec.armed) {
        describe_save(rec, text, sizeof text);
    } else {
        snprintf(text, sizeof text, "on the card, no record in the log");
    }
    saves_row(s_save_line, "Save", text);
    saves_details(details);
}

void on_close(td_window_t *)
{
    s_win = nullptr;
    s_lib_list = nullptr;
    s_lib_path = nullptr;
    s_lib_message = nullptr;
    s_tab_shown = -1;
    memset(s_page_count, 0, sizeof s_page_count);
    for (int t = 0; t < TABS; t++) {
        s_tab[t] = nullptr;
        s_status[t] = nullptr;
    }
    s_save_name = nullptr;
    s_save_line = nullptr;
    s_type_line = nullptr;
    s_folder_line = nullptr;
    for (int i = 0; i < DETAIL_LINES; i++) {
        s_detail[i] = nullptr;
    }
    s_input_list = nullptr;
    s_video_list = nullptr;
    s_volume = nullptr;
    s_volume_label = nullptr;
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
    /* A save-type scan has finished: the Saves page can say what it found. */
    if (s_scan_done) {
        s_scan_done = false;
        if (s_tab_shown == SAVES_TAB && td_win_is_open(s_win)) {
            saves_show();
        }
    }
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

    memset(s_page_count, 0, sizeof s_page_count);
    s_tab_shown = -1;

    /* The tab row is spaced by caption, as TinySNES's is, because "Library" is
     * wider than the four that follow it. */
    int tab_x = 1;
    for (int t = 0; t < TABS; t++) {
        s_tab[t] = td_button(s_win, tab_x, 0, TAB_NAMES[t], on_tab,
                             reinterpret_cast<void *>(static_cast<uintptr_t>(t)));
        tab_x += static_cast<int>(strlen(TAB_NAMES[t])) + 5;
    }

    /* Library: the cartridge browser. */
    lib_set_path(GBA_ROOT);
    s_lib_path = td_label(s_win, 1, 2, 0, "");
    s_lib_list = td_list(s_win, td_rect(1, 3, -1, LIB_ROWS), lib_get_item,
                         lib_on_activate, nullptr);
    /* The scrollbar belongs to the page like the list it moves: left off it,
     * it stood greyed beside every other tab. */
    page_add(0, td_scrollbar(s_win, -1, 3, LIB_ROWS, s_lib_list));
    page_add(0, s_lib_path);
    page_add(0, s_lib_list);
    page_add(0, td_button(s_win, 1, 16, "Launch", lib_on_launch, nullptr));
    page_add(0, td_button(s_win, 12, 16, "Up", lib_on_up, nullptr));
    page_add(0, td_button(s_win, 19, 16, "Refresh", lib_on_refresh, nullptr));

    /* Input: the pad map, laid out as TinyNES's page, fourteen rows on rows
     * 2 to 15.  No Player button: the GBA has one player. */
    input_load();
    s_input_list = td_list(s_win, td_rect(1, 2, INPUT_LIST_W, TANG_GBA_SOURCES), input_item,
                           on_input_row, nullptr);
    if (s_input_list != nullptr) {
        td_list_set_count(s_input_list, TANG_GBA_SOURCES);
    }
    page_add(INPUT_TAB, s_input_list);
    page_add(INPUT_TAB, td_button(s_win, INPUT_ACTION_X, 2, "Restore defaults", on_restore, nullptr));
    page_add(INPUT_TAB, td_button(s_win, INPUT_ACTION_X, 4, "Save", on_input_save, nullptr));
    page_add(INPUT_TAB, td_label(s_win, INPUT_ACTION_X, 7, 0, "Each controller button"));
    page_add(INPUT_TAB, td_label(s_win, INPUT_ACTION_X, 8, 0, "presses the GBA key"));
    page_add(INPUT_TAB, td_label(s_win, INPUT_ACTION_X, 9, 0, "beside it, on both pads."));
    page_add(INPUT_TAB, td_label(s_win, INPUT_ACTION_X, 11, 0, "HOME is the desk, and"));
    page_add(INPUT_TAB, td_label(s_win, INPUT_ACTION_X, 12, 0, "minus with HOME the exit,"));
    page_add(INPUT_TAB, td_label(s_win, INPUT_ACTION_X, 13, 0, "whatever the map says."));

    /* Video: placeholder, in TinySNES's two-row shape and TinyNES's layout. */
    s_video_list = td_list(s_win, td_rect(1, 2, VIDEO_LIST_W, VIDEO_ROWS), video_item,
                           on_placeholder, nullptr);
    if (s_video_list != nullptr) {
        td_list_set_count(s_video_list, VIDEO_ROWS);
    }
    page_add(VIDEO_TAB, s_video_list);
    page_add(VIDEO_TAB, td_button(s_win, VIDEO_ACTION_X, 2, "Save", on_placeholder, nullptr));
    page_add(VIDEO_TAB, td_label(s_win, 1, 5, 0, "The GBA's 240x160 picture, scaled by 4.5."));
    page_add(VIDEO_TAB, td_label(s_win, 1, 6, 0, "Nothing here is a setting yet."));

    /* Audio: placeholder, in TinySNES's shape. */
    page_add(AUDIO_TAB, td_label(s_win, 1, 2, 0, "Volume, HDMI and Line Out together:"));
    s_volume = td_slider(s_win, 1, 4, VOLUME_SLIDER_W, 5, on_volume, nullptr);
    if (s_volume != nullptr) {
        td_slider_set(s_volume, 100);
    }
    page_add(AUDIO_TAB, s_volume);
    s_volume_label = td_label(s_win, VOLUME_LABEL_X, 4, VOLUME_LABEL_W, "");
    page_add(AUDIO_TAB, s_volume_label);
    show_volume();
    page_add(AUDIO_TAB, td_checkbox(s_win, MUTE_X, 4, "Mute", false, on_placeholder, nullptr));
    page_add(AUDIO_TAB, td_button(s_win, AUDIO_SAVE_X, 4, "Save", on_placeholder, nullptr));
    page_add(AUDIO_TAB, td_label(s_win, 1, 6, 0, "Nothing here is a setting yet."));

    /* Saves, as TinyNES's page: the selected cartridge, when its save was
     * last written, its save type, the folder it sits in beside the
     * cartridge, how a save is made, and the file's details dimmed under
     * them.  Nothing here deletes a file. */
    s_save_name = td_label(s_win, 1, 2, 0, "");
    page_add(SAVES_TAB, s_save_name);
    s_save_line = td_label(s_win, 1, 3, 0, "");
    page_add(SAVES_TAB, s_save_line);
    s_type_line = td_label(s_win, 1, 4, 0, "");
    page_add(SAVES_TAB, s_type_line);
    s_folder_line = td_label(s_win, 1, 5, 0, "");
    page_add(SAVES_TAB, s_folder_line);
    page_add(SAVES_TAB, td_label(s_win, 1, 7, 0, "In a game, minus with HOME saves the game"));
    page_add(SAVES_TAB, td_label(s_win, 1, 8, 0, "and returns to the desk."));
    for (int i = 0; i < DETAIL_LINES; i++) {
        s_detail[i] = td_label(s_win, 1, 10 + i, 0, "");
        page_add(SAVES_TAB, s_detail[i]);
    }

    /* One status label per page, because a widget shared between pages takes
     * the visibility of whichever page is shown last.  The placeholder pages
     * say what they are from the start. */
    for (int t = 0; t < TABS; t++) {
        s_status[t] = td_label(s_win, 1, 17, 0,
                               t == VIDEO_TAB || t == AUDIO_TAB ? PLACEHOLDER
                               : t == INPUT_TAB                  ? INPUT_HELP
                                                                 : "");
        page_add(t, s_status[t]);
    }
    s_lib_message = s_status[0];

    lib_load();
    if (s_lib_list != nullptr) {
        td_list_select(s_lib_list, 0);
    }
    if (s_launching) {
        lib_say("A core load is running");
    } else if (s_playing) {
        lib_say("A game is running; minus+HOME saves and ends it.");
    }
    show_tab(0);
}

const td_app_t s_app = { "TinyGBA", launch, "GB" };

} // namespace

// C linkage: the desktop's init file is C, and calls this by name.
extern "C" void td_tinygba_register(void)
{
    (void)td_app_register(&s_app);
}
