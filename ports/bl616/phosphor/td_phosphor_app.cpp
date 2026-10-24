// TinyTang: the Phosphor app, a TinyDesk window that plays /root/media/music through the
// Phosphor core.
//
// It is a view onto the playback task (phosphor_player): the list plays a file
// with phosphor_player_play, the buttons stop, pause and step through the
// folder, and twice a second the window reads the task's status, which costs
// no link traffic.  The track carries on when the window closes, as it does
// after `phosphor play ... nowait`.  Only tracks this window started advance to
// the next file when they end, so a track played from the shell is left alone.
//
// The window has two tabs, the way the TinyNES Control Deck does it.  Player is
// the library and the transport, and its Load button runs the same script the
// desktop icon and the shell run, so loading a core has one path.  Scope holds
// the XY scope's controls, which until now only a shell `phosphor poke 0xac`
// could reach: the enable and the trail ramp, plus the core's own status and
// counters.  Glow is not offered: the core's halo is a 25 % green lift with a
// 6 % fringe row, which does not read as a glow, and a script that still sets
// bit 3 shows as "glow on" in the status line.
//
// The core reports no duration, so the progress bar is shown only for WAV and
// FLAC, whose headers give the length exactly (phosphor_media.h); every format
// shows its elapsed time.  Whether a Phosphor core is there at all is read from
// its magic register every few seconds; without one the transport is dimmed and
// the Player tab offers the Load button.

#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern "C" {
#include "ff.h"
#include "td_apps.h"
#include "tang_core_load.h"
#include "tang_osd.h"
void td_phosphor_register(void);
}

#include "fpga_debug.h"
#include "phosphor_media.h"
#include "phosphor_player.h"

namespace {

constexpr char MUSIC_REAL[] = "/sd/root/media/music";
constexpr char MUSIC_SHOWN[] = "/root/media/music";
constexpr int MAX_TRACKS = 256;
constexpr int NAME_BYTES = 96;
constexpr uint32_t TICK_MS = 500;
constexpr uint32_t CORE_CHECK_TICKS = 10;  // every five seconds
constexpr uint32_t CORE_CHECK_TIMEOUT_MS = 100;
constexpr uint32_t PHOSPHOR_MAGIC_ADDRESS = 0x0000;
constexpr uint32_t PHOSPHOR_MAGIC = 0x54504830;  // "TPH0"

// The Load button does not go through /root/scripts/tinyphosphor.tdsh, which the
// desktop icon and the shell still use.  A shell command runs in a worker task
// and a script needs its line table; with the desktop up the heap cannot give
// either one, so a click used to fail with "not enough memory to load the
// script" and nothing else.  The button loads in-process instead, through the
// same routine `tangload` now calls, so no script and no worker sit in the path.

// The stable image name that script also loads
// (/root/cores/console138k/tinyphosphor.bin), as FatFS mounts it.
constexpr char CORE_REAL[] = "/sd/root/cores/console138k/tinyphosphor.bin";

// Scope registers, as src/debug/debug_regs.sv decodes them.  0xac, register
// ABI 1.14, is a 16-bit control word:
//   0      enable
//   2:1    persistence (0 133 ms, 1 267 ms, 2 533 ms, 3 67 ms -- a fade-rate
//          class, not a length, which is why the codes are out of order)
//   3      instant, no decay; the core keeps the bit but this page stopped
//          offering it, because with no plane brightness the picture is blank
//   4      bloom, the halo on the channels the beam is not using
//   5      reserved (freeze was removed: clearing the plane on the control
//          change made it a black screen, and holding the image was not
//          wanted)
//   6      swap the channels
//   7      dots only, no line between points
//   9:8    channels: 0 XY, 1 X only, 2 Y only
//   11:10  colour: 0 green, 1 white, 2 amber, 3 cyan
// The core resets to 0x0001, so the scope is on at 67 ms, green, stereo, with
// no bloom, swap or dots.  0xb4 reports the scope's state with the
// active settings at 31:16, the FIFO level at 15:8; 0xb8 is the sweep count and
// 0xb0 the dropped-sample count.
constexpr uint32_t SCOPE_CONTROL_ADDRESS = 0x00ac;
constexpr uint32_t SCOPE_STATUS_ADDRESS = 0x00b4;
constexpr uint32_t SCOPE_SWEEPS_ADDRESS = 0x00b8;
constexpr uint32_t SCOPE_DROPPED_ADDRESS = 0x00b0;
constexpr uint32_t SCOPE_TIMEOUT_MS = 100;
constexpr uint32_t SCOPE_REFRESH_TICKS = 8;  // every four seconds on the tab
constexpr int SCOPE_PERSISTENCE = 4;
constexpr int SCOPE_CHANNELS = 3;
constexpr int TABS = 2;
constexpr int PAGE_WIDGETS = 40;

struct entry {
    char name[NAME_BYTES];
    bool is_dir;
};

td_window_t *s_win;
td_widget_t *s_folder, *s_list, *s_title, *s_line, *s_bar, *s_message, *s_auto;
td_widget_t *s_buttons[5];
td_widget_t *s_pause;

// One row of tab buttons over one page of widgets, the TinyNES Control Deck's
// arrangement.  There is no tab widget in TinyDesk, so a page is shown by
// hiding the other page's widgets.
td_widget_t *s_tab[TABS];
td_widget_t *s_page[TABS][PAGE_WIDGETS];
int s_page_count[TABS];
int s_tab_shown = -1;

td_widget_t *s_load;
td_widget_t *s_scope_enable, *s_scope_bloom, *s_scope_line, *s_scope_counters;
td_widget_t *s_scope_swap, *s_scope_dots;
td_widget_t *s_channel_button[SCOPE_CHANNELS];
td_widget_t *s_persist_button[SCOPE_PERSISTENCE];
const char *const TAB_NAMES[TABS] = { "Player", "Scope" };

// One age is AGE_TICK_CYCLES of the pixel clock, 4.167 ms, so the four ramp
// codes are 16, 32, 64 and 128 ages.  The row is labelled in milliseconds
// because that is what the eye sees, and each button carries its code rather
// than its position, since the codes are not in duration order.  "Instant" is
// the ABI 1.13 override at bit 3: no decay at all, only the points the beam is
// on now.
struct persistence_option {
    const char *name;
    int code;
    bool instant;
};
constexpr persistence_option PERSISTENCE[SCOPE_PERSISTENCE] = {
    {"67ms", 3, false},
    {"133ms", 0, false},
    {"267ms", 1, false},
    {"533ms", 2, false},
};
constexpr const char *const CHANNEL_NAMES[SCOPE_CHANNELS] = {"XY", "X", "Y"};
int s_persist;
bool s_instant;  // read from the core, never written: the page has no Instant
bool s_swap, s_dots;
int s_channel;
uint32_t s_scope_ticks;

entry *s_entries;
int s_count;
bool s_truncated;

// The folder the list is showing.  The library opens on the music root and
// walks down into album folders; ".." climbs back and never above the root.
char s_dir_shown[TD_PATH_MAX] = {};
char s_dir_real[TD_PATH_MAX] = {};

bool s_core;
uint32_t s_ticks;
uint32_t s_fast_ticks;  // after a load, probe every tick for a few seconds

// The track the status was last read for, and what was worked out from it.
uint32_t s_seen_track;
int s_current = -1;            // its index in the list, -1 if not in /root/media/music
uint32_t s_duration_ms;        // 0: not known
// The newest track this window started, the one it may advance from.
uint32_t s_own_track;
uint32_t s_advanced_from;      // a track already advanced from, never twice

void set_text(td_widget_t *w, const char *text)
{
    if (w != nullptr && strcmp(td_widget_text(w), text) != 0) {
        td_widget_set_text(w, text);
    }
}

/* ------------------------------------------------------------------ pages */

void sync_scope(void);  // defined below need_core()
void scope_show_choice(td_widget_t **buttons, int count, int chosen);

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
                            t == tab ? td_theme()->accent : TD_COLOR_DEFAULT);
    }
    s_tab_shown = tab;
    if (tab == 1 && s_win != nullptr) {
        // The core owns the scope's state, so read it when the page appears
        // rather than showing what this window last knew.
        sync_scope();
    }
}

void on_tab(td_widget_t *, void *user) { show_tab(static_cast<int>(reinterpret_cast<uintptr_t>(user))); }

int compare(const void *a, const void *b)
{
    const entry *x = static_cast<const entry *>(a);
    const entry *y = static_cast<const entry *>(b);
    if (x->is_dir != y->is_dir) {
        return x->is_dir ? -1 : 1;
    }
    return strcasecmp(x->name, y->name);
}

bool listable(const FILINFO &info)
{
    if ((info.fattrib & (AM_HID | AM_SYS)) != 0 || info.fname[0] == '.') {
        return false;
    }
    if (strlen(info.fname) >= NAME_BYTES) {
        s_truncated = true;
        return false;
    }
    if ((info.fattrib & AM_DIR) != 0) {
        return true;
    }
    return phosphor_media::supported(info.fname);
}

void load_folder(void)
{
    free(s_entries);
    s_entries = nullptr;
    s_count = 0;
    s_truncated = false;
    td_list_set_count(s_list, 0);
    DIR dir;
    FILINFO info;
    if (f_opendir(&dir, s_dir_real) != FR_OK) {
        char text[TD_TEXT_MAX];
        snprintf(text, sizeof text, "%s is not on the card", s_dir_shown);
        set_text(s_folder, text);
        return;
    }

    // Count before allocating: reserving all 256 names costs 24 KB even for
    // a small folder, leaving too little heap for a desktop script worker.
    const bool at_root = strcmp(s_dir_real, MUSIC_REAL) == 0;
    int capacity = at_root ? 0 : 1;   // ".." when below the root
    FRESULT result;
    while ((result = f_readdir(&dir, &info)) == FR_OK && info.fname[0] != '\0') {
        if (listable(info)) {
            if (capacity < MAX_TRACKS) {
                ++capacity;
            } else {
                s_truncated = true;
            }
        }
    }
    if (result != FR_OK || f_rewinddir(&dir) != FR_OK) {
        (void)f_closedir(&dir);
        set_text(s_folder, "Could not read that folder");
        return;
    }
    if (capacity != 0) {
        s_entries = static_cast<entry *>(malloc(sizeof(entry) * capacity));
        if (s_entries == nullptr) {
            (void)f_closedir(&dir);
            set_text(s_folder, "Not enough memory for the music list");
            return;
        }
    }
    if (!at_root) {
        snprintf(s_entries[s_count].name, NAME_BYTES, "..");
        s_entries[s_count].is_dir = true;
        ++s_count;
    }
    while ((result = f_readdir(&dir, &info)) == FR_OK && info.fname[0] != '\0') {
        if (!listable(info)) {
            continue;
        }
        // The folder may grow between passes. Never exceed the allocation.
        if (s_count == capacity) {
            s_truncated = true;
            continue;
        }
        strcpy(s_entries[s_count].name, info.fname);
        s_entries[s_count].is_dir = (info.fattrib & AM_DIR) != 0;
        ++s_count;
    }
    (void)f_closedir(&dir);
    if (result != FR_OK) {
        free(s_entries);
        s_entries = nullptr;
        s_count = 0;
        set_text(s_folder, "Could not read that folder");
        return;
    }
    if (s_count > 1) {
        qsort(s_entries, static_cast<size_t>(s_count), sizeof(entry), compare);
    }
    char text[TD_TEXT_MAX];
    snprintf(text, sizeof(text), "%s: %d item%s%s", s_dir_shown, s_count,
             s_count == 1 ? "" : "s", s_truncated ? " (some not listed)" : "");
    set_text(s_folder, text);
    td_list_set_count(s_list, s_count);
}

// The list index of a status path such as "/root/media/music/song.flac", or -1.
int index_of(const char *path)
{
    const size_t prefix = strlen(s_dir_shown);
    if (strncmp(path, s_dir_shown, prefix) != 0 || path[prefix] != '/') {
        return -1;
    }
    const char *name = path + prefix + 1;
    for (int i = 0; i < s_count; ++i) {
        if (strcmp(s_entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

uint32_t read_duration(const char *real)
{
    static uint8_t header[phosphor_media::HEADER_BYTES];
    FIL file;
    if (real[0] == '\0' || f_open(&file, real, FA_READ) != FR_OK) {
        return 0;
    }
    UINT got = 0;
    const FSIZE_t size = f_size(&file);
    const bool read = f_read(&file, header, sizeof(header), &got) == FR_OK;
    (void)f_close(&file);
    uint32_t ms = 0;
    if (!read || !phosphor_media::duration_ms(real, header, got, size, &ms)) {
        return 0;
    }
    return ms;
}

bool check_core(void)
{
    fpga_debug_result r;
    return fpga_debug_request(FPGA_EXT_READ32, PHOSPHOR_MAGIC_ADDRESS, 0, &r,
                              CORE_CHECK_TIMEOUT_MS) &&
           r.status == 0 && r.data == PHOSPHOR_MAGIC;
}

void show_core(void)
{
    const int fg = s_core ? TD_COLOR_DEFAULT : td_theme()->dim;
    for (td_widget_t *b : s_buttons) {
        td_widget_set_color(b, fg, TD_COLOR_DEFAULT);
    }
}

void say(const char *text) { set_text(s_message, text); }

bool need_core(void)
{
    if (!s_core) {
        s_core = check_core();
        show_core();
    }
    if (!s_core) {
        say("No Phosphor core yet: press Load, or run /root/scripts/tinyphosphor.tdsh");
    }
    return s_core;
}

/* ------------------------------------------------------------------ scope */

// The control word the core decodes at 0xac.  Writing it is the only way the
// scope's appearance is set; the core resets it to enable with no trails.
uint32_t scope_word(bool enable)
{
    return (enable ? 1u : 0u) | (static_cast<uint32_t>(s_persist & 3) << 1) |
           (s_instant ? 8u : 0u) |
           (td_checkbox_get(s_scope_bloom) ? 16u : 0u) |
           (td_checkbox_get(s_scope_swap) ? 64u : 0u) |
           (td_checkbox_get(s_scope_dots) ? 128u : 0u) |
           (static_cast<uint32_t>(s_channel & 3) << 8);
    // Bits 11:10 select a colour in ABI 1.14, but this image ignores them: the
    // beam stays the green phosphor until the palette can widen without moving
    // the bus clock's global route.
}

void scope_show_persistence(void)
{
    for (int i = 0; i < SCOPE_PERSISTENCE; i++) {
        const persistence_option &o = PERSISTENCE[i];
        const bool chosen = o.code == s_persist;
        td_widget_set_color(s_persist_button[i], TD_COLOR_DEFAULT,
                            chosen ? td_theme()->accent : TD_COLOR_DEFAULT);
    }
}

bool scope_read(uint32_t address, uint32_t *value)
{
    fpga_debug_result r;
    if (!fpga_debug_request(FPGA_EXT_READ32, address, 0, &r, SCOPE_TIMEOUT_MS) || r.status != 0) {
        return false;
    }
    *value = r.data;
    return true;
}

// Mirror the core's word into the page: the checkboxes and the highlighted
// trail button follow whatever the core reports, not what this window sent.
void sync_scope(void)
{
    if (s_win == nullptr) {
        return;
    }
    uint32_t control = 0, status = 0, sweeps = 0, dropped = 0;
    if (!scope_read(SCOPE_CONTROL_ADDRESS, &control)) {
        set_text(s_scope_line, s_core ? "Scope: no answer from the core"
                                      : "Scope: no core loaded, press Load on the Player tab");
        set_text(s_scope_counters, "");
        return;
    }
    control &= 0xffffu;
    s_persist = static_cast<int>((control >> 1) & 3);
    s_instant = (control & 8u) != 0;
    s_swap = (control & 64u) != 0;
    s_dots = (control & 128u) != 0;
    s_channel = static_cast<int>((control >> 8) & 3);
    td_checkbox_set(s_scope_enable, (control & 1u) != 0);
    td_checkbox_set(s_scope_bloom, (control & 16u) != 0);
    td_checkbox_set(s_scope_swap, s_swap);
    td_checkbox_set(s_scope_dots, s_dots);
    scope_show_persistence();
    scope_show_choice(s_channel_button, SCOPE_CHANNELS, s_channel);
    // The widget states changed underneath the last frame; ask for a repaint or
    // the boxes keep the glyph they were drawn with.
    td_wm_invalidate();

    if (!scope_read(SCOPE_STATUS_ADDRESS, &status)) {
        set_text(s_scope_line, "Scope: control read, status did not answer");
        set_text(s_scope_counters, "");
        return;
    }
    // src/visualizers/scope_xy.sv packs status as: bit 0 enabled, bit 1
    // initialised, bit 2 drawing, bit 3 loading, bit 4 cache ready, 16:8 the
    // sample FIFO level, 24:20 the active settings and 31:24 the age clock.
    char text[96];
    snprintf(text, sizeof(text), "Scope: %s%s%s  fifo %lu  %s%s",
             (status & 1u) ? "on" : "off", (status & 2u) ? ", ready" : ", clearing",
             (status & 4u) ? ", drawing" : "",
             static_cast<unsigned long>((status >> 8) & 0xffu),
             (status & 8u) == 0 ? "settled" : "loading",
             (control & 16u) != 0 ? "  bloom" : "");
    set_text(s_scope_line, text);

    if (scope_read(SCOPE_SWEEPS_ADDRESS, &sweeps) && scope_read(SCOPE_DROPPED_ADDRESS, &dropped)) {
        snprintf(text, sizeof(text), "%lu sweep%s, %lu dropped",
                 static_cast<unsigned long>(sweeps), sweeps == 1 ? "" : "s",
                 static_cast<unsigned long>(dropped));
        set_text(s_scope_counters, text);
    } else {
        set_text(s_scope_counters, "");
    }
}

void push_scope(void)
{
    if (!need_core()) {
        return;
    }
    const uint32_t word = scope_word(td_checkbox_get(s_scope_enable));
    fpga_debug_result r;
    if (!fpga_debug_request(FPGA_EXT_WRITE32, SCOPE_CONTROL_ADDRESS, word, &r,
                            SCOPE_TIMEOUT_MS) || r.status != 0) {
        set_text(s_scope_line, "Scope: the write was refused");
        return;
    }
    // Read back rather than trusting the local copy: a refused or clipped write
    // would otherwise leave the page showing a setting the core never took.
    sync_scope();
}

void scope_show_choice(td_widget_t **buttons, int count, int chosen)
{
    for (int i = 0; i < count; i++) {
        td_widget_set_color(buttons[i], TD_COLOR_DEFAULT,
                            i == chosen ? td_theme()->accent : TD_COLOR_DEFAULT);
    }
}

void on_scope_enable(td_widget_t *, void *) { push_scope(); }
void on_scope_bloom(td_widget_t *, void *) { push_scope(); }
void on_scope_swap(td_widget_t *, void *) { push_scope(); }
void on_scope_dots(td_widget_t *, void *) { push_scope(); }

void on_channel(td_widget_t *, void *user)
{
    s_channel = static_cast<int>(reinterpret_cast<uintptr_t>(user));
    scope_show_choice(s_channel_button, SCOPE_CHANNELS, s_channel);
    push_scope();
}

void on_persist(td_widget_t *, void *user)
{
    const int index = static_cast<int>(reinterpret_cast<uintptr_t>(user));
    if (index < 0 || index >= SCOPE_PERSISTENCE) {
        return;
    }
    s_persist = PERSISTENCE[index].code;
    s_instant = PERSISTENCE[index].instant;
    scope_show_persistence();
    push_scope();
}

void on_scope_resync(td_widget_t *, void *)
{
    s_core = check_core();
    show_core();
    sync_scope();
}

/* ----------------------------------------------------------------- launch */

// Load the core without the shell.  `tangload` is a command, and a command's
// work runs in a 16 KB worker task; a script also needs its own line table.
// With the desktop up the heap cannot always supply those, and the load then
// fails before it starts.  tang_core_load() is the same sequence in this task,
// so the desktop's memory no longer decides whether a core can be loaded.
//
// The call blocks for the erase and programming.  The FPGA reconfiguration
// wipes the desktop layer anyway, and tang_core_load() re-arms it.
void on_load(td_widget_t *, void *)
{
    say("Loading the core ...");
    const tang_load_result_t result = tang_core_load(CORE_REAL);
    if (result != TANG_LOAD_OK) {
        say(result == TANG_LOAD_BUSY ? "Another load is already running"
                                     : "The core did not load; see the console");
        return;
    }
    // The script handed the screen to the player with `osd off`, because the
    // core comes out of configuration with its overlay on.  Same here, so the
    // scope is what the HDMI shows until F12 brings TinyDesk back.
    (void)tang_osd_set(false);
    s_core = check_core();
    show_core();
    s_fast_ticks = s_core ? 0 : 12;
    say(s_core ? "Core loaded; F12 shows the scope" : "Core loaded, link not answering yet");
    if (s_tab_shown == 1) {
        sync_scope();
    }
}

void play_index(int index)
{
    if (index < 0 || index >= s_count || s_entries[index].is_dir || !need_core()) {
        return;
    }
    char real[TD_PATH_MAX + 4];
    char shown[TD_PATH_MAX + 4];
    snprintf(real, sizeof(real), "%s/%s", s_dir_real, s_entries[index].name);
    snprintf(shown, sizeof(shown), "%s/%s", s_dir_shown, s_entries[index].name);
    uint32_t track = 0;
    if (!phosphor_player_play(real, shown, &track)) {
        say("The playback task is not running");
        return;
    }
    s_own_track = track;
    td_list_select(s_list, index);
    say("");
}

void refresh(void);

// The first playable row, skipping folders and "..".
int first_track(void)
{
    for (int i = 0; i < s_count; ++i) {
        if (!s_entries[i].is_dir) {
            return i;
        }
    }
    return -1;
}

// The next playable row in `direction`, skipping folders; -1 when there is none.
int next_track(int from, int direction)
{
    for (int i = from + direction; i >= 0 && i < s_count; i += direction) {
        if (!s_entries[i].is_dir) {
            return i;
        }
    }
    return -1;
}

// Walk into a folder, or back up with "..", and reload the list.  ".." at the
// music root does nothing, so the library cannot wander off into the card.
void open_dir(const char *name)
{
    char shown[TD_PATH_MAX];
    char real[TD_PATH_MAX];
    if (strcmp(name, "..") == 0) {
        if (strcmp(s_dir_real, MUSIC_REAL) == 0) {
            return;
        }
        snprintf(shown, sizeof shown, "%s", s_dir_shown);
        snprintf(real, sizeof real, "%s", s_dir_real);
        char *slash = strrchr(shown, '/');
        if (slash != nullptr) {
            *slash = '\0';
        }
        slash = strrchr(real, '/');
        if (slash != nullptr) {
            *slash = '\0';
        }
    } else {
        const int n = snprintf(shown, sizeof shown, "%s/%s", s_dir_shown, name);
        const int m = snprintf(real, sizeof real, "%s/%s", s_dir_real, name);
        if (n <= 0 || n >= static_cast<int>(sizeof shown) || m <= 0 ||
            m >= static_cast<int>(sizeof real)) {
            say("That folder's path is too long");
            return;
        }
    }
    snprintf(s_dir_shown, sizeof s_dir_shown, "%s", shown);
    snprintf(s_dir_real, sizeof s_dir_real, "%s", real);
    load_folder();
    if (s_list != nullptr) {
        td_list_select(s_list, 0);
    }
}

void on_activate(td_widget_t *, void *)
{
    const int index = td_list_selected(s_list);
    if (index < 0 || index >= s_count) {
        return;
    }
    if (s_entries[index].is_dir) {
        open_dir(s_entries[index].name);
        return;
    }
    play_index(index);
}

void on_play(td_widget_t *, void *)
{
    const int selected = td_list_selected(s_list);
    const bool usable = selected >= 0 && selected < s_count &&
                        !s_entries[selected].is_dir;
    play_index(usable ? selected : first_track());
    refresh();
}

void on_stop(td_widget_t *, void *)
{
    if (!need_core()) {
        return;
    }
    (void)phosphor_player_stop(3000);
    refresh();
}

void on_pause(td_widget_t *, void *)
{
    if (!need_core()) {
        return;
    }
    phosphor_player_status s;
    phosphor_player_get(&s);
    if (!phosphor_player_pause(!s.paused)) {
        say("Nothing is playing");
    }
}

// Prev and Next step from the track playing, or from the selection.
void step(int direction)
{
    const int from = s_current >= 0 ? s_current : td_list_selected(s_list);
    const int to = next_track(from, direction);
    if (to < 0) {
        say(direction > 0 ? "That was the last track" : "That was the first track");
        return;
    }
    play_index(to);
    refresh();
}

void on_prev(td_widget_t *, void *) { step(-1); }
void on_next(td_widget_t *, void *) { step(1); }

const char *get_item(td_widget_t *, int index, int *fg, void *)
{
    static char text[NAME_BYTES + 4];
    if (index < 0 || index >= s_count) {
        return "";
    }
    const bool playing = index == s_current;
    if (playing) {
        *fg = td_theme()->accent;
    }
    snprintf(text, sizeof(text), "%s%s%s", playing ? "> " : "  ",
             s_entries[index].name, s_entries[index].is_dir ? "/" : "");
    return text;
}

void refresh(void)
{
    phosphor_player_status s;
    phosphor_player_get(&s);

    if (s.track != s_seen_track) {
        s_seen_track = s.track;
        s_current = index_of(s.path);
        s_duration_ms = read_duration(s.real);
        td_wm_invalidate();
    }

    // Advance once from a track of this window's that played to its end.
    if (s.state == phosphor_player_state::ENDED && s.track == s_own_track &&
        s.track != s_advanced_from && td_checkbox_get(s_auto)) {
        s_advanced_from = s.track;
        const int next = next_track(s_current, 1);
        if (next >= 0) {
            play_index(next);
            phosphor_player_get(&s);
            s_seen_track = s.track;
            s_current = next;
            s_duration_ms = read_duration(s.real);
            td_wm_invalidate();
        }
    }

    char text[160];
    if (s.state == phosphor_player_state::IDLE) {
        set_text(s_title, "Nothing played yet");
        set_text(s_line, "");
    } else {
        const char *name = strrchr(s.path, '/');
        snprintf(text, sizeof(text), "%s", name != nullptr ? name + 1 : s.path);
        set_text(s_title, text);

        char elapsed[16];
        char total[16] = "";
        phosphor_media::format_time(phosphor_media::elapsed_ms(s.samples, s.rate), elapsed,
                                    sizeof(elapsed));
        if (s_duration_ms != 0) {
            total[0] = '/';
            total[1] = ' ';
            phosphor_media::format_time(s_duration_ms, total + 2, sizeof(total) - 2);
        }
        const char *state = s.paused ? "paused" : phosphor_player_state_name(s.state);
        if (s.rate != 0) {
            snprintf(text, sizeof(text), "%-8s %s %s   %lu Hz   %lu underrun%s", state, elapsed,
                     total, static_cast<unsigned long>(s.rate),
                     static_cast<unsigned long>(s.underruns), s.underruns == 1 ? "" : "s");
        } else {
            snprintf(text, sizeof(text), "%-8s %s %s", state, elapsed, total);
        }
        set_text(s_line, text);
    }

    const bool bar = s_duration_ms != 0 && s.state != phosphor_player_state::IDLE;
    td_widget_set_visible(s_bar, bar);
    if (bar) {
        const int percent =
            s.state == phosphor_player_state::ENDED
                ? 100
                : phosphor_media::percent(phosphor_media::elapsed_ms(s.samples, s.rate),
                                          s_duration_ms);
        if (s_bar->value != percent) {
            td_progress_set(s_bar, percent);
        }
    }

    set_text(s_pause, s.paused ? "Resume" : "Pause ");
    if (s.state == phosphor_player_state::FAILED) {
        say(s.error);
    } else if (s_core && strncmp(td_widget_text(s_message), "No Phosphor", 11) == 0) {
        say("");
    }
}

void on_tick(td_window_t *)
{
    ++s_ticks;
    // After a Load the core takes a few seconds to configure and answer, so
    // probe every tick for a while instead of waiting out the slow check.
    if (s_fast_ticks != 0 && !s_core) {
        --s_fast_ticks;
        if (check_core()) {
            s_core = true;
            s_fast_ticks = 0;
            show_core();
            if (s_tab_shown == 1) {
                sync_scope();
            }
        }
    } else if (s_ticks % CORE_CHECK_TICKS == 0) {
        const bool core = check_core();
        if (core != s_core) {
            s_core = core;
            show_core();
            if (!s_core) {
                (void)need_core();
            }
        }
    }
    // Scope counters move only while audio plays, so they are read slowly and
    // only while the page is on screen; the Player tab keeps the link quiet.
    if (s_tab_shown == 1 && ++s_scope_ticks % SCOPE_REFRESH_TICKS == 0) {
        sync_scope();
    }
    refresh();
}

void on_close(td_window_t *)
{
    s_win = nullptr;
    free(s_entries);
    s_entries = nullptr;
    s_count = 0;
}

void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }
    (void)phosphor_player_init();
    td_window_desc_t d = {};
    d.title = "Phosphor";
    d.rect = td_rect(-1, -1, 64, 24);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE | TD_WIN_RESIZABLE;
    d.min_w = 50;
    d.min_h = 16;
    d.on_close = on_close;
    d.on_tick = on_tick;
    d.tick_ms = TICK_MS;
    s_win = td_win_create(&d);
    if (s_win == nullptr) {
        free(s_entries);
        s_entries = nullptr;
        return;
    }
    s_count = 0;
    s_ticks = 0;
    s_seen_track = 0;
    s_current = -1;
    s_duration_ms = 0;

    // The tab row stays visible on both pages, so it is created once here and
    // never added to a page.  The page bookkeeping is per window: this array
    // keeps counters across a close and reopen, and stale entries would stop
    // later adds at PAGE_WIDGETS.
    for (int t = 0; t < TABS; t++) {
        s_page_count[t] = 0;
        s_tab[t] = td_button(s_win, t == 0 ? 0 : 11, 0, TAB_NAMES[t], on_tab,
                             reinterpret_cast<void *>(static_cast<uintptr_t>(t)));
    }

    s_folder = td_label(s_win, 0, 1, 0, "");
    s_list = td_list(s_win, td_rect(0, 2, -1, -9), get_item, on_activate, nullptr);
    td_widget_t *scroll = td_scrollbar(s_win, -1, 2, -9, s_list);
    s_title = td_label(s_win, 0, -7, 0, "");
    s_line = td_label(s_win, 0, -6, 0, "");
    s_bar = td_progress(s_win, 0, -5, 0);
    s_buttons[0] = td_button(s_win, 0, -3, "Prev", on_prev, nullptr);
    s_buttons[1] = td_button(s_win, 9, -3, "Play", on_play, nullptr);
    s_buttons[2] = s_pause = td_button(s_win, 18, -3, "Pause ", on_pause, nullptr);
    s_buttons[3] = td_button(s_win, 29, -3, "Stop", on_stop, nullptr);
    s_buttons[4] = td_button(s_win, 38, -3, "Next", on_next, nullptr);
    s_load = td_button(s_win, 47, -3, "Load", on_load, nullptr);
    s_auto = td_checkbox(s_win, 0, -2, "Play the next track when one ends", true, nullptr,
                         nullptr);
    s_message = td_label(s_win, 0, -1, 0, "");
    td_widget_set_color(s_message, td_theme()->dim, TD_COLOR_DEFAULT);

    page_add(0, s_folder);
    page_add(0, s_list);
    page_add(0, scroll);
    page_add(0, s_title);
    page_add(0, s_line);
    page_add(0, s_bar);
    for (td_widget_t *b : s_buttons) {
        page_add(0, b);
    }
    page_add(0, s_load);
    page_add(0, s_auto);
    page_add(0, s_message);

    s_scope_enable = td_checkbox(s_win, 0, 1, "Enable the XY scope", true, on_scope_enable,
                                 nullptr);
    td_widget_t *persist_label = td_label(s_win, 0, 3, 0, "Persistence (Afterglow)");
    // The captions run from four to seven characters, so each button takes the
    // width the widget gives a caption (length plus five) and the next starts
    // where it ends.
    int x = 0;
    for (int i = 0; i < SCOPE_PERSISTENCE; i++) {
        s_persist_button[i] = td_button(s_win, x, 4, PERSISTENCE[i].name, on_persist,
                                        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        x += static_cast<int>(strlen(PERSISTENCE[i].name)) + 5;
    }
    s_scope_bloom = td_checkbox(s_win, 0, 11, "Bloom", false, on_scope_bloom, nullptr);
    // "[x] caption" needs the caption's length plus the box and its gaps, so
    // each checkbox starts after the whole caption of the one before; placing
    // them by guesswork clipped the end of the caption.
    s_scope_swap = td_checkbox(s_win, 0, 6, "Swap X/Y", false, on_scope_swap, nullptr);
    s_scope_dots = td_checkbox(s_win, 14, 6, "Dots", false, on_scope_dots, nullptr);

    td_widget_t *channel_label = td_label(s_win, 0, 8, 0, "Channels");
    x = 0;
    for (int i = 0; i < SCOPE_CHANNELS; i++) {
        s_channel_button[i] = td_button(s_win, x, 9, CHANNEL_NAMES[i], on_channel,
                                        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        x += static_cast<int>(strlen(CHANNEL_NAMES[i])) + 5;
    }

    s_scope_line = td_label(s_win, 0, -3, 0, "");
    s_scope_counters = td_label(s_win, 0, -2, 0, "");
    td_widget_t *resync = td_button(s_win, 0, -1, "Resync", on_scope_resync, nullptr);
    td_widget_set_color(s_scope_line, td_theme()->dim, TD_COLOR_DEFAULT);
    td_widget_set_color(s_scope_counters, td_theme()->dim, TD_COLOR_DEFAULT);

    page_add(1, s_scope_enable);
    page_add(1, persist_label);
    for (td_widget_t *b : s_persist_button) {
        page_add(1, b);
    }
    page_add(1, s_scope_bloom);
    page_add(1, s_scope_swap);
    page_add(1, s_scope_dots);
    page_add(1, channel_label);
    for (td_widget_t *b : s_channel_button) {
        page_add(1, b);
    }
    page_add(1, s_scope_line);
    page_add(1, s_scope_counters);
    page_add(1, resync);

    // The core's reset word: enabled, 67 ms, green, stereo, nothing else.
    s_persist = 3;
    s_instant = false;
    s_swap = s_dots = false;
    s_channel = 0;
    s_scope_ticks = 0;
    s_fast_ticks = 0;
    scope_show_persistence();
    scope_show_choice(s_channel_button, SCOPE_CHANNELS, s_channel);
    // A reopened window keeps the tab it last showed, so force the pages to be
    // re-arranged rather than relying on the stored index.
    s_tab_shown = -1;
    show_tab(0);

    // A reopened window starts at the music root rather than at whatever
    // album the last one was browsing.
    snprintf(s_dir_shown, sizeof s_dir_shown, "%s", MUSIC_SHOWN);
    snprintf(s_dir_real, sizeof s_dir_real, "%s", MUSIC_REAL);
    load_folder();
    s_core = check_core();
    show_core();
    if (!s_core) {
        (void)need_core();
    }
    refresh();
    if (s_current >= 0) {
        td_list_select(s_list, s_current);
    }
    td_widget_focus(s_list);
}

const td_app_t s_app = {"Phosphor", launch, "|>"};

} // namespace

void td_phosphor_register(void)
{
    td_app_register(&s_app);
}
