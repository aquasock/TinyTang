// TinyNES Control Deck -- the window that changes how a game behaves.
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
#include "tang_ae350_run.h"
#include "tang_core_load.h"

/* The USB console writer.  It is declared for the shell's own sources in
 * ports/bl616/tdsh_stdio_redirect.h, which also renames printf here; only the
 * function is wanted, as ports/bl616/tang_jtag_glue.h declares it. */
int tdsh_printf(const char *fmt, ...);
}

#include "FreeRTOS.h"
#include "task.h"

#include "tang_nes_ini.h"

namespace {

const int TICK_MS = 400;
const int LIB_MAX_ENTRIES = 256;
const int LIB_NAME_BYTES = 56;
constexpr char ROOT_SHOWN[] = "/root/media/nes";
constexpr char ROOT_REAL[] = "/sd/root/media/nes";

/* This SDK's FatFS declares AM_RDO, AM_HID, AM_SYS, AM_DIR and AM_ARC but not
 * the volume label's bit, which FatFS gives as 0x08; a card's own label entry
 * is skipped with the other attributes a browser has no use for. */
constexpr BYTE AM_VOLUME_LABEL = 0x08;

/* A launch does not go through the shell at all.  A shell command runs in a
 * worker task and a script needs its line table on top of that; with the
 * desktop up the heap cannot give either one, so picking a game here used to
 * fail with "not enough memory to load the script" and nothing else (BL6-010).
 * The Library loads and runs in a task of its own, through the same routines
 * `tangload` and `ae350run` call, so no script and no worker sit in the path.
 * Nothing else starts TinyNES: a cartridge opened from the Files app or the
 * desktop is refused with a pointer to this window instead of being launched
 * or opened as text (third_party/patches/tinydesk/0002).
 *
 * These are the three files a launch needs, as FatFS mounts them. */
constexpr char CORE_REAL[] = "/sd/root/cores/console138k/tinynes.bin";
constexpr char PLAYER_REAL[] = "/sd/root/ae350/tinynes-play.elf";
constexpr char DESKTOP_REAL[] = "/sd/root/cores/console138k/desktop.bin";

/* 8 KiB is what the Phosphor player gives its task, and half what a shell
 * worker gave this run: no interpreter, no script, no line table.  The margin
 * left over is printed when the game ends so the size stays visible. */
constexpr uint32_t LAUNCH_STACK_WORDS = 2048;
const int TABS = 4;
const int PAGE_WIDGETS = 16;
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

/* No Audio tab: the I2S2's Line Out is a fixed level for the amplifier, and
 * the NES is mono, so volume, mute and channels had nothing to set. */
const char *const TAB_NAMES[TABS] = { "Library", "Input", "Video", "Saves" };

td_window_t *s_win;
td_widget_t *s_tab[TABS];
td_widget_t *s_page[TABS][PAGE_WIDGETS];
int s_page_count[TABS];
int s_tab_shown = -1;

/* The Library page.  This was the TinyNES Launcher window, now a page, so its
 * names carry a prefix that keeps them apart from the pages beside it. */
struct lib_entry
{
    char name[LIB_NAME_BYTES];
    bool is_dir;
};

td_widget_t *s_lib_list;
td_widget_t *s_lib_path;
td_widget_t *s_lib_message;   /* the Library page's status label */
lib_entry *s_lib_entries;
int s_lib_count;
bool s_lib_truncated;
char s_lib_shown[TD_PATH_MAX];
char s_lib_real[TD_PATH_MAX + 4];

/* A launch owns the core and the link until the game ends, so a second one is
 * refused rather than reloading the core under the first.  The launch frees
 * the list before it starts, so s_lib_reload asks the window's tick to put the
 * page back once the task has finished. */
volatile bool s_launching;
volatile bool s_lib_reload;
char s_launch_cart[TD_PATH_MAX + 4];   /* real path handed to the launch task */

td_widget_t *s_list;             /* the Input page's rows */
td_widget_t *s_video_list;       /* the Video page's rows: aspect, palette */
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

/* The [video] keys this app owns: the picture's pixel aspect and its palette,
 * written in the names the parser reads back. */
bool put_video(FIL *file)
{
    char line[48];

    snprintf(line, sizeof line, "aspect = %s\n", tang_nes_aspect_name(s_set.aspect));
    if (!put(file, line)) {
        return false;
    }
    snprintf(line, sizeof line, "palette = %s\n", tang_nes_palette_name(s_set.palette));
    return put(file, line);
}

bool is_named_line(const char *line, const char *key)
{
    const size_t n = strlen(key);

    return strncmp(line, key, n) == 0 &&
           (line[n] == ' ' || line[n] == '\t' || line[n] == '=');
}

bool is_video_line(const char *line)
{
    return is_named_line(line, "aspect") || is_named_line(line, "palette");
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
    bool in_video = false;
    bool done = false;          /* the mapping has been put out */
    bool video_done = false;    /* ...and the aspect and palette */
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
                if (in_video && !video_done) {
                    ok = ok && put_video(&out);
                    video_done = true;
                }
                in_input = strncmp(line, "[input]", 7) == 0;
                in_video = strncmp(line, "[video]", 7) == 0;
                ok = ok && put(&out, line) && put(&out, "\n");
                if (in_input) {
                    ok = ok && put_mapping(&out);
                    done = true;
                }
                if (in_video) {
                    ok = ok && put_video(&out);
                    video_done = true;
                }
                continue;
            }
            if (in_input && is_key_line(line)) {
                continue;       /* ours, and already out under the header */
            }
            if (in_video && is_video_line(line)) {
                continue;       /* likewise */
            }
            ok = ok && put(&out, line) && put(&out, "\n");
        }
    }
    /* A last line with no newline after it. */
    if (ok && len != 0) {
        line[len] = '\0';
        if (!(in_input && is_key_line(line)) &&
            !(in_video && is_video_line(line))) {
            ok = put(&out, line) && put(&out, "\n");
        }
    }
    /* A file with no [input] section at all, as the first save on a fresh card
     * would be, and the same for [video]. */
    if (ok && !done) {
        ok = put(&out, "\n[input]\n") && put_mapping(&out);
    }
    if (ok && !video_done) {
        ok = put(&out, "\n[video]\n") && put_video(&out);
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

/* The Video page's rows: the picture's pixel aspect and its palette, which
 * the core takes from the host when a game starts. */
const int VIDEO_ROWS = 2;

const char *video_item(td_widget_t *, int index, int *fg, void *)
{
    static char text[48];

    (void)fg;
    if (index == 0) {
        snprintf(text, sizeof text, "Aspect      %s%s",
                 tang_nes_aspect_name(s_set.aspect), s_dirty ? " *" : "");
    } else if (index == 1) {
        snprintf(text, sizeof text, "Palette     %s%s",
                 tang_nes_palette_name(s_set.palette), s_dirty ? " *" : "");
    } else {
        return "";
    }
    return text;
}

/* Steps the selected row to its next value. */
void on_video_row(td_widget_t *, void *)
{
    const int row = s_video_list != nullptr ? td_list_selected(s_video_list) : -1;

    if (row == 0) {
        s_set.aspect = (uint8_t)((s_set.aspect + 1) % 3);
    } else if (row == 1) {
        s_set.palette = (uint8_t)((s_set.palette + 1) % TANG_NES_PALETTES);
    } else {
        return;
    }
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
    s_video_list = nullptr;
    s_player_button = nullptr;
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

/* ---------------------------------------------------------------- helpers */

bool lib_at_root(void) { return strcmp(s_lib_shown, ROOT_SHOWN) == 0; }

void lib_set_path(const char *shown)
{
    snprintf(s_lib_shown, sizeof s_lib_shown, "%s", shown);
    snprintf(s_lib_real, sizeof s_lib_real, "/sd%s", shown);
}

void lib_say(const char *text)
{
    if (s_lib_message != nullptr && strcmp(td_widget_text(s_lib_message), text) != 0) {
        td_widget_set_text(s_lib_message, text);
    }
}

/* True for a name that can be handed to the shell unchanged.  A quote would
 * end FILE's own quoting and a '$' or a backtick could be read as one, and a
 * name that did would lib_start a game at the wrong path -- silently, which is
 * how a mangled path read once already.  The library carries none of these;
 * one that did is left out of the list rather than listed and then refused. */
bool lib_handable(const char *name)
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

bool lib_is_cartridge(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot != nullptr && (strcasecmp(dot, ".nes") == 0 || strcasecmp(dot, ".zip") == 0);
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

/* Read the current folder.  The folder is counted before anything is
 * allocated, so a folder of a few files costs a few hundred bytes and not
 * the ceiling: the script worker a launch starts wants 16 KB of stack on the
 * same heap (BL6-010), and reserving the ceiling for every folder is what
 * the Phosphor catalog was rewritten to stop doing. */
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

/* ---------------------------------------------------------------- actions */

void lib_descend_to(const char *name);

/* Start the chosen cartridge in a task of this app's own: load the TinyNES
 * core, run the player with the cartridge, then put the desktop core back --
 * the sequence a shell launch used to perform. */
void launch_task(void *)
{
    const tang_load_result_t loaded = tang_core_load(CORE_REAL);
    int code = 1;
    if (loaded == TANG_LOAD_OK) {
        code = tang_ae350_run(PLAYER_REAL, s_launch_cart);
        /* Back to the desktop core whatever the run did: its desk layer is
         * what the screen returns to, and the script reloaded it here too. */
        (void)tang_core_load(DESKTOP_REAL);
    } else if (loaded == TANG_LOAD_BUSY) {
        tdsh_printf("nes: another core load is already running\r\n");
    } else {
        tdsh_printf("nes: the core did not load\r\n");
    }
    tdsh_printf("nes: done (%d), %u bytes of stack left\r\n", code,
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
     * core and serves the AE350 wants the heap more than this page does.  The
     * tick puts the list back when the game ends. */
    free(s_lib_entries);
    s_lib_entries = nullptr;
    s_lib_count = 0;
    s_lib_truncated = false;
    if (s_lib_list != nullptr) {
        td_list_set_count(s_lib_list, 0);
    }
    lib_say("Starting ...");
    s_launching = true;
    if (xTaskCreate(launch_task, "neslaunch", LAUNCH_STACK_WORDS, nullptr,
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
        if (slash == nullptr || slash <= next + strlen(ROOT_SHOWN)) {
            snprintf(next, sizeof next, "%s", ROOT_SHOWN);
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

/* Free the list on close: the heap is shared with the shell workers and the
 * Bluetooth stack, and nothing here is worth keeping while the window is not
 * open. */
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

void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }
    load_settings();

    td_window_desc_t d = {};
    d.title = "TinyNES";
    d.rect = td_rect(-1, -1, 66, 26);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE | TD_WIN_RESIZABLE;
    d.min_w = 50;
    d.min_h = 18;
    d.on_close = on_close;
    d.on_tick = on_tick;
    d.tick_ms = TICK_MS;
    d.icon = "NES";
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

    /* Library: the cartridge browser.  Launch hands the path to the same
     * handler the Files app uses, so what starts a game here is what starts
     * one there; the handler reloads the core, and this window survives that
     * because it lives on the BL616. */
    lib_set_path(ROOT_SHOWN);
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

    /* Input: whose mapping is shown, the eight rows, and the actions. */
    s_player_button = td_button(s_win, 1, 2, "Player 1", on_player, nullptr);
    page_add(1, s_player_button);
    s_list = td_list(s_win, td_rect(1, 4, 34, TANG_NES_BUTTONS), input_item,
                     on_row, nullptr);
    if (s_list != nullptr) {
        td_list_set_count(s_list, TANG_NES_BUTTONS);
    }
    page_add(1, s_list);
    page_add(1, td_button(s_win, 38, 4, "Change", on_row, nullptr));
    page_add(1, td_button(s_win, 38, 6, "Restore defaults", on_restore, nullptr));
    page_add(1, td_button(s_win, 38, 8, "Save", on_save, nullptr));
    page_add(1, td_label(s_win, 38, 10, 0, "Change steps the"));
    page_add(1, td_label(s_win, 38, 11, 0, "selected row."));

    /* Video: two rows, the pixel aspect and the palette, and the Save that
     * writes them.  Scale, scanlines and overscan were dropped, so the page
     * no longer offers them. */
    s_video_list = td_list(s_win, td_rect(1, 2, 34, VIDEO_ROWS), video_item,
                           on_video_row, nullptr);
    if (s_video_list != nullptr) {
        td_list_set_count(s_video_list, VIDEO_ROWS);
    }
    page_add(2, s_video_list);
    page_add(2, td_button(s_win, 38, 2, "Change", on_video_row, nullptr));
    page_add(2, td_button(s_win, 38, 4, "Save", on_save, nullptr));
    page_add(2, td_label(s_win, 1, 6, 0, "Both are taken when the next game"));
    page_add(2, td_label(s_win, 1, 7, 0, "starts, like the buttons."));

    page_add(3, td_label(s_win, 1, 2, 0, "Battery saves are in /root/saves/nes"));
    page_add(3, td_label(s_win, 1, 3, 0, "and written when the game ends."));
    page_add(3, td_label(s_win, 1, 5, 0, "Save states are not implemented,"));
    page_add(3, td_label(s_win, 1, 6, 0, "so this page is a placeholder."));

    /* What the file is, and what a save said.  One label per page, because a
     * widget shared between pages takes the visibility of whichever page is
     * shown last -- which hid this on the Input tab, where Save is. */
    for (int t = 0; t < TABS; t++) {
        s_status[t] = td_label(s_win, 1, 17, 0, "");
        page_add(t, s_status[t]);
    }
    /* The Library page reports through the same per-page status label, so it
     * does not carry a second message line. */
    s_lib_message = s_status[0];
    status_from_file();

    show_tab(0);
}

const td_app_t s_app = { "TinyNES Control Deck", launch, "NES" };

} // namespace

// C linkage: the desktop's init file is C, and calls this by name.
extern "C" void td_tinynes_register(void)
{
    (void)td_app_register(&s_app);
}
