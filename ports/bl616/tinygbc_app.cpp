// TinyGBC -- the window that starts Game Boy cartridges.
//
// The same shape as the TinyNES app beside it: an app window with a Library
// page that browses the card and starts what is picked, over a core loaded
// from the card.  A launch loads /cores/console138k/tinygbc.bin, runs the
// AE350 player with the cartridge as the file it asks for, then puts the
// desktop core back.
//
// The Library page starts games, and the Audio page sets one master volume and
// a mute for HDMI and the Line Out together, kept in /tinygbc.ini at the card root
// (ports/bl616/tang_gbc_ini.c) and sent to the player when a game starts.  The
// Saves page shows the selected cartridge's save state and when its battery
// save was last written, as TinyNES's does.  Input and Video are placeholders:
// they name what the player will grow, so the window does not have to change
// shape when they arrive.
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
#include "tang_gbc_ini.h"
#include "td_saves_log.h"

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
constexpr char DESKTOP_REAL[] = "/sd/root/cores/console138k/tinytang.bin";

/* Where the library opens: the media root itself, showing only the Game Boy
 * folders inside it, so the monochrome and the Color set are both reachable
 * and TinyNES's own folder is never offered to a Game Boy player.  Listed as
 * the shell shows them; the task works in /sd. */
constexpr char GB_ROOT[] = "/root/media";
const char *const GB_DIRS[] = { "gb", "gbc", "gbcolor" };

/* 8 KiB is what the Phosphor player gives its task, and half what a shell
 * worker gave this run: no interpreter, no script, no line table. */
/* The window's fixed size, frame included, and the Library rows it shows:
 * rows 3 to 14 of the client area, above the buttons on row 16 and the status
 * line on row 17.  54 columns is the narrowest the five tabs fit in with a
 * column between them: 52 inside the frame, from column 1. */
constexpr int WIN_W = 54;
constexpr int WIN_H = 20;
constexpr int LIB_ROWS = 12;

constexpr uint32_t LAUNCH_STACK_WORDS = 2048;
const int TABS = 5;
const int PAGE_WIDGETS = 16;

/* There is no named accent, only TD_COLOR_DEFAULT and raw palette indices. */
const int ACCENT = 14;

/* The tabs exist from the start so the window keeps its shape; Library, Audio
 * and Saves are built, the others are placeholders. */
const char *const TAB_NAMES[TABS] = { "Library", "Input", "Video", "Audio", "Saves" };
const int AUDIO_TAB = 3;
const int SAVES_TAB = 4;

/* Where a save is built before it replaces the real file, and where the file
 * being replaced is set aside meanwhile, as the TinyNES app does it: FatFS's
 * f_rename refuses a name already in use, so FAT has no atomic replace. */
const char TMP_PATH[] = "/sd/tinygbc.ini.tmp";
const char BAK_PATH[] = "/sd/tinygbc.ini.bak";

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

/* The Audio page: what Save writes, and its widgets. */
tang_gbc_settings_t s_set;
bool s_dirty;                    /* edited here and not yet saved */
td_widget_t *s_volume;
td_widget_t *s_volume_label;

/* The Input page: the selected row, and the list that shows the mapping. */
const int INPUT_TAB = 1;
const int INPUT_ROWS = 8;
td_widget_t *s_input_list;

/* The Video page: two rows, the monochrome palette and the colour correction.
 * The palette row is greyed for a colour cartridge, whose picture the core
 * takes from its own data and never from a palette setting. */
const int VIDEO_TAB = 2;
const int VIDEO_ROWS = 2;
td_widget_t *s_video_list;
/* The cartridge the last run actually loaded, and what the player said it was:
 * 1 colour, 0 monochrome, -1 nothing known. */
char s_last_cart[TD_PATH_MAX + 8];
int s_last_cgb = -1;

/* The Saves page: the save state of the cartridge selected in the Library, one
 * file per cartridge that the player writes when L is pressed with HOME in the
 * game (TinyGBC core-log entry 25), read again whenever the page is shown. */
const char STATE_SHOWN[] = "/root/states/gbc";
const char STATE_REAL[] = "/sd/root/states/gbc";
const int STATE_LINES = 6;
td_widget_t *s_state_line[STATE_LINES];
td_widget_t *s_batt_line;   /* when the battery save was last written */
td_widget_t *s_rtc_line;    /* when the cartridge's clock was last written */

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

void state_show(void);

void on_tab(td_widget_t *, void *user)
{
    const int tab = (int)(uintptr_t)user;
    show_tab(tab);
    if (tab == SAVES_TAB) {
        state_show();
    }
}

/* --------------------------------------------------------------- the file */

void audio_status(void)
{
    static char text[64];

    if (s_status[AUDIO_TAB] == nullptr) {
        return;
    }
    snprintf(text, sizeof text, "%s %s, %u key(s)%s", TANG_GBC_INI_SHOWN,
             s_set.found ? "read" : "absent", (unsigned)s_set.keys,
             s_dirty ? "  edited" : "");
    td_widget_set_text(s_status[AUDIO_TAB], text);
}

bool put(FIL *file, const char *text)
{
    const size_t length = strlen(text);
    UINT wrote = 0;

    return f_write(file, text, (UINT)length, &wrote) == FR_OK && wrote == length;
}

bool put_audio(FIL *file)
{
    char line[48];

    snprintf(line, sizeof line, "volume = %u\nmute = %s\n",
             (unsigned)s_set.audio.volume, s_set.audio.mute ? "yes" : "no");
    return put(file, line);
}

/* One line per Game Boy button, in the names the parser reads back. */
bool put_mapping(FIL *file)
{
    char line[48];

    for (int i = 0; i < TANG_GBC_BUTTONS; i++) {
        snprintf(line, sizeof line, "%s = %s\n",
                 tang_gbc_button_name(i), tang_gbc_pad_name(s_set.pad_map[i]));
        if (!put(file, line)) {
            return false;
        }
    }
    return true;
}

/* One line per picture setting, in the names the parser reads back. */
bool put_video(FIL *file)
{
    char line[48];

    snprintf(line, sizeof line, "palette = %s\ncolour = %s\n",
             tang_gbc_palette_name(s_set.palette),
             tang_gbc_colour_name(s_set.colour));
    return put(file, line);
}

bool is_named_line(const char *line, const char *key)
{
    const size_t n = strlen(key);

    return strncmp(line, key, n) == 0 &&
           (line[n] == ' ' || line[n] == '\t' || line[n] == '=');
}

bool is_audio_line(const char *line)
{
    return is_named_line(line, "volume") || is_named_line(line, "mute");
}

/* A line of [input]: one of the eight Game Boy buttons, e.g. `a = x`. */bool is_mapping_line(const char *line)
{
    for (int i = 0; i < TANG_GBC_BUTTONS; i++) {
        if (is_named_line(line, tang_gbc_button_name(i))) {
            return true;
        }
    }
    return false;
}

/* A line of [video]: the palette or the colour correction. */
bool is_video_line(const char *line)
{
    return is_named_line(line, "palette") || is_named_line(line, "colour");
}

/* Write the mapping, the picture and the volume where the file's [input],
 * [video] and [audio] sections are, keeping every line outside them byte for
 * byte, so a hand-written comment or a key belonging to another section
 * survives a save.  Those three sections are rewritten whole.  A card with no
 * /tinygbc.ini yet gets one.  NULL when it wrote, else a short reason. */
const char *save_settings(void)
{
    FIL in, out;
    char chunk[64];
    char line[200];
    size_t len = 0;
    bool in_input = false;
    bool in_video = false;
    bool in_audio = false;
    bool input_done = false;    /* the mapping has been put out */
    bool video_done = false;    /* ...and the palette and the correction */
    bool audio_done = false;    /* ...and the volume and mute */
    bool ok = true;
    UINT got = 0;
    const char *reason = NULL;
    const bool existed = f_open(&in, TANG_GBC_INI_REAL, FA_READ) == FR_OK;

    if (f_open(&out, TMP_PATH, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        if (existed) {
            (void)f_close(&in);
        }
        return "cannot create the temporary";
    }
    if (!existed) {
        ok = put(&out, "# TinyGBC settings, written by the TinyGBC window.\n");
    }
    while (existed && ok && f_read(&in, chunk, sizeof chunk, &got) == FR_OK && got != 0) {
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
                in_input = strncmp(line, "[input]", 7) == 0;
                in_video = strncmp(line, "[video]", 7) == 0;
                in_audio = strncmp(line, "[audio]", 7) == 0;
                ok = put(&out, line) && put(&out, "\n");
                if (in_input && !input_done) {
                    ok = ok && put_mapping(&out);
                    input_done = true;
                }
                if (in_video && !video_done) {
                    ok = ok && put_video(&out);
                    video_done = true;
                }
                if (in_audio && !audio_done) {
                    ok = ok && put_audio(&out);
                    audio_done = true;
                }
                continue;
            }
            if ((in_input && is_mapping_line(line)) ||
                (in_video && is_video_line(line)) ||
                (in_audio && is_audio_line(line))) {
                continue;       /* ours, and already out under the header */
            }
            ok = put(&out, line) && put(&out, "\n");
        }
    }
    /* A last line with no newline after it. */
    if (ok && len != 0) {
        line[len] = '\0';
        if (!((in_input && is_mapping_line(line)) ||
              (in_video && is_video_line(line)) ||
              (in_audio && is_audio_line(line)))) {
            ok = put(&out, line) && put(&out, "\n");
        }
    }
    if (ok && !input_done) {
        ok = put(&out, "\n[input]\n") && put_mapping(&out);
    }
    if (ok && !video_done) {
        ok = put(&out, "\n[video]\n") && put_video(&out);
    }
    if (ok && !audio_done) {
        ok = put(&out, "\n[audio]\n") && put_audio(&out);
    }
    if (!ok) {
        reason = "a write to the temporary failed";
    }
    if (f_close(&out) != FR_OK && reason == NULL) {
        reason = "the temporary did not close";
    }
    if (existed) {
        (void)f_close(&in);
    }
    if (reason != NULL) {
        (void)f_unlink(TMP_PATH);
        return reason;
    }

    /* Through the backup name, so a whole file sits under one of the three
     * names at every point and an interruption never leaves half of one. */
    (void)f_unlink(BAK_PATH);
    if (existed && f_rename(TANG_GBC_INI_REAL, BAK_PATH) != FR_OK) {
        (void)f_unlink(TMP_PATH);
        return "cannot set the settings file aside";
    }
    if (f_rename(TMP_PATH, TANG_GBC_INI_REAL) != FR_OK) {
        if (existed) {
            (void)f_rename(BAK_PATH, TANG_GBC_INI_REAL);
        }
        return "cannot rename over the settings file";
    }
    (void)f_unlink(BAK_PATH);
    return NULL;
}

/* ------------------------------------------------------------------ audio */

/* The percentage beside the slider, because the bar alone cannot say whether
 * it is at 45 or 50. */
void show_volume(void)
{
    if (s_volume_label != nullptr) {
        td_widget_printf(s_volume_label, "%3u%%%s", (unsigned)s_set.audio.volume,
                         s_set.audio.mute ? "  muted" : "");
    }
}

void on_volume(td_widget_t *w, void *)
{
    s_set.audio.volume = (uint8_t)w->value;
    s_dirty = true;
    show_volume();
    audio_status();
}

void on_mute(td_widget_t *w, void *)
{
    s_set.audio.mute = td_checkbox_get(w) ? 1 : 0;
    s_dirty = true;
    show_volume();
    audio_status();
}

/* ------------------------------------------------------------------ video */

/* Whether the palette setting can do anything for the cartridge selected in the
 * Library: it cannot when that is the cartridge the last run reported as a
 * colour one, because a colour cartridge's picture comes from its own data and
 * the core returns before reading the option. A cartridge nothing is known
 * about -- anything not yet launched, which includes every .zip -- is left
 * enabled rather than guessed at. */
bool palette_inert(void)
{
    const int row = s_lib_list != nullptr ? td_list_selected(s_lib_list) : -1;
    char path[TD_PATH_MAX + 8];
    int n;

    if (s_last_cgb != 1 || s_lib_entries == nullptr || row < 0 || row >= s_lib_count ||
        s_lib_entries[row].is_dir) {
        return false;
    }
    n = snprintf(path, sizeof path, "%s/%s", s_lib_real, s_lib_entries[row].name);
    return n > 0 && n < static_cast<int>(sizeof path) &&
           strcmp(path, s_last_cart) == 0;
}

const char *video_item(td_widget_t *, int index, int *fg, void *)
{
    static char text[48];

    (void)fg;
    if (index == 0) {
        snprintf(text, sizeof text, "%-9s %s%s", "Palette",
                 tang_gbc_palette_name(s_set.palette), s_dirty ? " *" : "");
        if (palette_inert()) {
            /* Greyed, and left readable: the name still says what is set. */
            *fg = td_theme()->dim;
        }
        return text;
    }
    if (index == 1) {
        snprintf(text, sizeof text, "%-9s %s%s", "Colour",
                 tang_gbc_colour_name(s_set.colour), s_dirty ? " *" : "");
        return text;
    }
    return "";
}

/* Steps the selected row to its next value. The palette row does nothing for a
 * colour cartridge, so neither does its Change. */
void on_video_row(td_widget_t *, void *)
{
    const int row = s_video_list != nullptr ? td_list_selected(s_video_list) : -1;

    if (row == 0) {
        if (palette_inert()) {
            td_widget_set_text(s_status[VIDEO_TAB],
                               "A colour cartridge draws its own colours.");
            return;
        }
        s_set.palette = (uint8_t)((s_set.palette + 1) % TANG_GBC_PAL_COUNT);
    } else if (row == 1) {
        s_set.colour = (uint8_t)((s_set.colour + 1) % TANG_GBC_COLOUR_COUNT);
    } else {
        return;
    }
    s_dirty = true;
    if (s_win != nullptr) {
        td_win_invalidate(s_win);
    }
}

/* -------------------------------------------------------------- the input */

/* One row: the Game Boy button and the controller button that answers to it.
 * Activating it steps the mapping to the next controller button, as the
 * TinyNES page does, because a capture would need the pad itself and the only
 * pad an app can see is the desk layer's seven-bit word. */
const char *input_item(td_widget_t *, int index, int *fg, void *)
{
    static char text[48];

    (void)fg;
    if (index < 0 || index >= TANG_GBC_BUTTONS) {
        return "";
    }
    snprintf(text, sizeof text, "%-8s %s%s", tang_gbc_button_name(index),
             tang_gbc_pad_name(s_set.pad_map[index]), s_dirty ? " *" : "");
    return text;
}

void on_row(td_widget_t *, void *)
{
    const int row = s_input_list != nullptr ? td_list_selected(s_input_list) : -1;

    if (row < 0 || row >= TANG_GBC_BUTTONS) {
        return;
    }
    s_set.pad_map[row] = (uint8_t)((s_set.pad_map[row] + 1) % TANG_GBC_CTL_COUNT);
    s_dirty = true;
    if (s_win != nullptr) {
        td_win_invalidate(s_win);
    }
}

/* The mapping alone, not the whole file: resetting the buttons must not move
 * the volume the Audio page set. */
void on_restore(td_widget_t *, void *)
{
    tang_gbc_default_map(s_set.pad_map);
    s_dirty = true;
    if (s_win != nullptr) {
        td_win_invalidate(s_win);
    }
}

void on_save(td_widget_t *, void *)
{
    if (!s_dirty) {
        td_widget_set_text(s_status[s_tab_shown], "Nothing to save.");
        return;
    }
    const char *why = save_settings();
    if (why == NULL) {
        s_dirty = false;
        (void)tang_gbc_load(&s_set, TANG_GBC_INI_REAL);
        td_widget_set_text(s_status[s_tab_shown],
                           "Saved.  It applies from the next game start.");
        if (s_win != nullptr) {
            td_win_invalidate(s_win);    /* and the rows lose their change mark */
        }
    } else {
        td_widget_set_text(s_status[s_tab_shown], why);
    }
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

/* A folder the library offers at the media root: the Game Boy sets, by name
 * without case.  Anything else there belongs to another player. */
bool lib_is_gb_dir(const char *name)
{
    for (size_t i = 0; i < sizeof GB_DIRS / sizeof GB_DIRS[0]; i++) {
        if (strcasecmp(name, GB_DIRS[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* At the media root only the Game Boy folders are offered; below it every
 * folder and every cartridge is, which is where the letters live. */
bool lib_listable(const FILINFO &info, bool at_root)
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
        return at_root ? lib_is_gb_dir(info.fname) : true;
    }
    return !at_root && lib_is_cartridge(info.fname);
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
    const bool at_root = lib_at_root();

    /* Counted before anything is allocated, so a folder of a few files costs
     * a few hundred bytes and not the ceiling. */
    const int parent = at_root ? 0 : 1;
    int capacity = parent;
    FRESULT result;
    while ((result = f_readdir(&dir, &info)) == FR_OK && info.fname[0] != '\0') {
        if (!lib_listable(info, at_root)) {
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
        if (!lib_listable(info, at_root)) {
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
        snprintf(text, sizeof text, "No gb, gbc or gbcolor folder");
    } else {
        snprintf(text, sizeof text, "%d entr%s", s_lib_count, s_lib_count == 1 ? "y" : "ies");
    }
    lib_say(text);
}

/* The library always starts at the media root; lib_list() offers only the
 * Game Boy folders there, so both sets sit one step below it.  A card without
 * that root says so in the list rather than showing nothing. */
void lib_choose_root(void)
{
    snprintf(s_root, sizeof s_root, "%s", GB_ROOT);
}

/* ------------------------------------------------------------ the state */

/* TinyGBC's state header (firmware/apps/gb_bench/main.cpp, StateHeader), the
 * layout of TinyNES's with its own magic: little-endian, as both ends are. */
struct state_header
{
    char magic[4];
    uint16_t header_bytes;
    uint16_t version;
    uint32_t rom_crc;
    uint32_t gambatte;
    uint32_t patches;
    uint32_t frame;
    uint32_t length;
    uint32_t crc;
};
static_assert(sizeof(state_header) == 32, "TinyGBC's state header is 32 bytes");

uint32_t state_crc32(uint32_t crc, const uint8_t *bytes, unsigned n)
{
    crc = ~crc;
    while (n-- != 0) {
        crc ^= *bytes++;
        for (int bit = 0; bit < 8; bit++) {
            crc = crc >> 1 ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void state_lines(const char *const text[STATE_LINES])
{
    for (int i = 0; i < STATE_LINES; i++) {
        if (s_state_line[i] != nullptr) {
            td_widget_set_text(s_state_line[i], text[i] != nullptr ? text[i] : "");
        }
    }
    if (s_win != nullptr) {
        td_win_invalidate(s_win);
    }
}

/* One saved file, in the page's words.  The stamp is the RTC-kept clock's
 * local time as the firmware wrote it; a save made before the clock had been
 * set says so rather than passing "no-clock" off as a date. */
void describe_save(const td_saves_attempt_t &rec, const char *verb, char *out,
                   size_t cap)
{
    char when[32];

    td_saves_when(rec.stamp, when, sizeof when);
    if (rec.verdict[0] != '\0' && strcmp(rec.stamp, "no-clock") == 0) {
        snprintf(out, cap, "%s, %lu B; clock was unset then", verb, rec.want);
    } else if (rec.verdict[0] != '\0') {
        snprintf(out, cap, "%s %s, %s, %lu B", verb, when, rec.verdict, rec.want);
    } else {
        snprintf(out, cap, "%s %s, did not finish", verb, when);
    }
}

/* What the card holds for the Library's selected cartridge.  The CRC-32 is of
 * the whole file, the number the player prints as it sends a state and the
 * one TinyTang verifies it by, so the three can be matched by eye. */
void state_show(void)
{
    static char line[STATE_LINES][TD_TEXT_MAX];
    static char batt[TD_TEXT_MAX];
    const char *text[STATE_LINES] = {};
    const int row = s_lib_list != nullptr ? td_list_selected(s_lib_list) : -1;

    if (s_lib_entries == nullptr || row < 0 || row >= s_lib_count ||
        s_lib_entries[row].is_dir) {
        text[0] = "Select a cartridge in the Library.";
        state_lines(text);
        if (s_batt_line != nullptr) {
            td_widget_set_text(s_batt_line, "Battery save: select a cartridge.");
        }
        if (s_rtc_line != nullptr) {
            td_widget_set_text(s_rtc_line, "");
        }
        return;
    }
    const char *name = s_lib_entries[row].name;
    const char *dot = strrchr(name, '.');
    const int stem = dot != nullptr && dot != name ? (int)(dot - name) : (int)strlen(name);

    /* The board's log, read once for both files: every line names the file it
     * carried and is stamped with the RTC-kept clock's local time.  The files
     * carry no date of their own, because FatFS has no clock on this board. */
    td_saves_attempt_t saved_state = {};
    td_saves_attempt_t saved_batt = {};
    td_saves_attempt_t saved_rtc = {};
    {
        char needle_state[LIB_NAME_BYTES + 8];
        char needle_sav[LIB_NAME_BYTES + 8];
        char needle_rtc[LIB_NAME_BYTES + 8];
        snprintf(needle_state, sizeof needle_state, "/%.*s.state", stem, name);
        snprintf(needle_sav, sizeof needle_sav, "/%.*s.sav", stem, name);
        snprintf(needle_rtc, sizeof needle_rtc, "/%.*s.rtc", stem, name);
        FIL log;
        if (f_open(&log, "/sd/root/saves/last.log", FA_READ) == FR_OK) {
            char buf[256];
            UINT got = 0;
            td_saves_reader_t reader_state;
            td_saves_reader_t reader_batt;
            td_saves_reader_t reader_rtc;
            td_saves_reader_init(&reader_state);
            td_saves_reader_init(&reader_batt);
            td_saves_reader_init(&reader_rtc);
            while (f_read(&log, buf, sizeof buf, &got) == FR_OK && got > 0) {
                td_saves_reader_feed(&reader_state, buf, got, needle_state,
                                     &saved_state);
                td_saves_reader_feed(&reader_batt, buf, got, needle_sav,
                                     &saved_batt);
                td_saves_reader_feed(&reader_rtc, buf, got, needle_rtc,
                                     &saved_rtc);
            }
            (void)f_close(&log);
        }
    }
    if (s_batt_line != nullptr) {
        if (saved_batt.verdict[0] != '\0' || saved_batt.armed) {
            describe_save(saved_batt, "Battery save", batt, sizeof batt);
        } else {
            snprintf(batt, sizeof batt, "Battery save: none written yet.");
        }
        td_widget_set_text(s_batt_line, batt);
    }
    /* The clock of an MBC3 or HuC3 cartridge, which the player keeps only
     * when this board's clock has the date; most cartridges have none. */
    if (s_rtc_line != nullptr) {
        static char clock[TD_TEXT_MAX];
        if (saved_rtc.verdict[0] != '\0' || saved_rtc.armed) {
            describe_save(saved_rtc, "Clock", clock, sizeof clock);
        } else {
            snprintf(clock, sizeof clock, "Clock: none kept for this cartridge.");
        }
        td_widget_set_text(s_rtc_line, clock);
    }

    char real[TD_PATH_MAX + 8];
    snprintf(line[0], sizeof line[0], "%.*s", stem, name);
    snprintf(line[1], sizeof line[1], "%s/%.*s.state", STATE_SHOWN, stem, name);
    snprintf(real, sizeof real, "%s/%.*s.state", STATE_REAL, stem, name);
    text[0] = line[0];
    text[1] = line[1];

    FIL file;
    if (f_open(&file, real, FA_READ) != FR_OK) {
        text[2] = "No save state yet. L saves one in the game.";
        state_lines(text);
        return;
    }
    const uint32_t size = (uint32_t)f_size(&file);
    state_header h = {};
    uint8_t chunk[256];
    uint32_t crc = 0, total = 0;
    bool ok = true;
    for (;;) {
        UINT got = 0;
        if (f_read(&file, chunk, sizeof chunk, &got) != FR_OK) {
            ok = false;
            break;
        }
        if (got == 0) {
            break;
        }
        if (total == 0 && got >= sizeof h) {
            memcpy(&h, chunk, sizeof h);
        }
        crc = state_crc32(crc, chunk, got);
        total += got;
    }
    (void)f_close(&file);

    snprintf(line[2], sizeof line[2], "Size %lu bytes, CRC-32 %08lx",
             (unsigned long)size, (unsigned long)crc);
    text[2] = line[2];
    if (!ok || memcmp(h.magic, "TGS1", 4) != 0 || h.header_bytes != sizeof h ||
        h.length + sizeof h != size) {
        text[3] = "Not a TinyGBC save state, or cut short.";
    } else {
        snprintf(line[3], sizeof line[3], "Game CRC-32 %08lx, frame %lu",
                 (unsigned long)h.rom_crc, (unsigned long)h.frame);
        snprintf(line[4], sizeof line[4], "Core gambatte %08lx",
                 (unsigned long)h.gambatte);
        text[3] = line[3];
        text[4] = line[4];
    }
    /* When it was written: the log's record, since the file itself predates
     * any clock.  A save that started and never finished says that instead. */
    if (saved_state.verdict[0] != '\0' || saved_state.armed) {
        describe_save(saved_state, "Saved", line[5], sizeof line[5]);
    } else {
        snprintf(line[5], sizeof line[5], "Saved: no record in the log.");
    }
    text[5] = line[5];
    state_lines(text);
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
        /* What the player said it loaded, kept for the Video page's greyed
         * palette row. */
        snprintf(s_last_cart, sizeof s_last_cart, "%s", s_launch_cart);
        s_last_cgb = tang_ae350_cartridge_cgb();
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
    s_volume = nullptr;
    s_volume_label = nullptr;
    memset(s_state_line, 0, sizeof s_state_line);
    s_batt_line = nullptr;
    s_rtc_line = nullptr;
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

    /* The file is read every time the window opens, so a hand edit shows
     * without a power cycle. */
    (void)tang_gbc_load(&s_set, TANG_GBC_INI_REAL);
    s_dirty = false;

    td_window_desc_t d = {};
    d.title = "TinyGBC";
    /* A fixed size, the same for TinyNES and TinyGBC: 54 columns, the
     * narrowest the five tabs fit in, and 20 rows, which gives the Library
     * list twelve entries.  Without TD_WIN_RESIZABLE there is no resize grip, no
     * maximise and no F11, and the minimum pins the size as well. */
    d.rect = td_rect(-1, -1, WIN_W, WIN_H);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE;
    d.min_w = WIN_W;
    d.min_h = WIN_H;
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
    s_lib_list = td_list(s_win, td_rect(1, 3, -1, LIB_ROWS), lib_get_item,
                         lib_on_activate, nullptr);
    td_scrollbar(s_win, -1, 3, LIB_ROWS, s_lib_list);
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

    /* Input: which controller button answers to each Game Boy button, edited
     * a row at a time, written to /tinygbc.ini by Save and taken by the player
     * when the next game starts. */
    page_add(INPUT_TAB, td_label(s_win, 1, 2, 0, "Controller button per Game Boy button:"));
    s_input_list = td_list(s_win, td_rect(1, 3, -1, INPUT_ROWS), input_item,
                           on_row, nullptr);
    /* A list draws nothing until its count is set, whatever its height. */
    if (s_input_list != nullptr) {
        td_list_set_count(s_input_list, TANG_GBC_BUTTONS);
    }
    page_add(INPUT_TAB, s_input_list);
    page_add(INPUT_TAB, td_button(s_win, 1, 12, "Change", on_row, nullptr));
    page_add(INPUT_TAB, td_button(s_win, 10, 12, "Restore", on_restore, nullptr));
    page_add(INPUT_TAB, td_button(s_win, 20, 12, "Save", on_save, nullptr));
    page_add(INPUT_TAB, td_label(s_win, 1, 13, 0, "Change steps the selected row."));
    page_add(INPUT_TAB, td_label(s_win, 1, 14, 0, "Taken when the next game starts."));

    /* Video: the monochrome palette and the colour correction, both taken by
     * the player when the next game starts. The palette row is greyed for a
     * colour cartridge, which draws its own colours. */
    page_add(VIDEO_TAB, td_label(s_win, 1, 1, 0, "Palette is for monochrome cartridges."));
    s_video_list = td_list(s_win, td_rect(1, 3, -1, VIDEO_ROWS), video_item,
                           on_video_row, nullptr);
    /* A list draws nothing until its count is set, whatever its height. */
    if (s_video_list != nullptr) {
        td_list_set_count(s_video_list, VIDEO_ROWS);
    }
    page_add(VIDEO_TAB, s_video_list);
    page_add(VIDEO_TAB, td_button(s_win, 1, 7, "Change", on_video_row, nullptr));
    page_add(VIDEO_TAB, td_button(s_win, 10, 7, "Save", on_save, nullptr));
    page_add(VIDEO_TAB, td_label(s_win, 1, 9, 0, "Off is the picture before these settings."));
    page_add(VIDEO_TAB, td_label(s_win, 1, 10, 0, "Taken when the next game starts."));

    /* Audio: the master volume, its percentage, the mute, and the Save, laid
     * out as the TinyNES app's page is. */
    page_add(AUDIO_TAB, td_label(s_win, 1, 2, 0, "Volume, HDMI and Line Out together:"));
    s_volume = td_slider(s_win, 1, 4, 20, TANG_VOLUME_STEP, on_volume, nullptr);
    if (s_volume != nullptr) {
        td_slider_set(s_volume, s_set.audio.volume);
    }
    page_add(AUDIO_TAB, s_volume);
    s_volume_label = td_label(s_win, 23, 4, 14, "");
    page_add(AUDIO_TAB, s_volume_label);
    show_volume();
    page_add(AUDIO_TAB, td_checkbox(s_win, 1, 6, "Mute", s_set.audio.mute != 0,
                                    on_mute, nullptr));
    page_add(AUDIO_TAB, td_button(s_win, 1, 8, "Save", on_save, nullptr));
    page_add(AUDIO_TAB, td_label(s_win, 1, 10, 0, "Left and Right move the slider 5%."));
    page_add(AUDIO_TAB, td_label(s_win, 1, 11, 0, "Taken when the next game starts."));

    /* Saves: the selected cartridge's save state, then where battery saves
     * go.  L saves and R loads in the game; there is one state a cartridge. */
    page_add(SAVES_TAB, td_label(s_win, 1, 2, 0, "Save state (L saves, R loads, with HOME):"));
    for (int i = 0; i < STATE_LINES; i++) {
        s_state_line[i] = td_label(s_win, 3, 3 + i, 0, "");
        page_add(SAVES_TAB, s_state_line[i]);
    }
    page_add(SAVES_TAB, td_label(s_win, 1, 10, 0, "Battery saves and clocks are in /root/saves/gbc"));
    s_batt_line = td_label(s_win, 1, 11, 0, "Battery save: none written yet.");
    page_add(SAVES_TAB, s_batt_line);
    s_rtc_line = td_label(s_win, 1, 12, 0, "");
    page_add(SAVES_TAB, s_rtc_line);

    /* One status label per page, because a widget shared between pages takes
     * the visibility of whichever page is shown last. */
    for (int t = 0; t < TABS; t++) {
        s_status[t] = td_label(s_win, 1, 17, 0, "");
        page_add(t, s_status[t]);
    }
    s_lib_message = s_status[0];
    audio_status();

    show_tab(0);
}

const td_app_t s_app = { "TinyGBC", launch, "GBC" };

} // namespace

// C linkage: the desktop's init file is C, and calls this by name.
extern "C" void td_tinygbc_register(void)
{
    (void)td_app_register(&s_app);
}
