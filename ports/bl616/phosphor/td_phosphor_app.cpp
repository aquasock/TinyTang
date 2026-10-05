// TinyTang: the Phosphor app, a TinyDesk window that plays /music through the
// Phosphor core.
//
// It is a view onto the playback task (phosphor_player): the list plays a file
// with phosphor_player_play, the buttons stop, pause and step through the
// folder, and twice a second the window reads the task's status, which costs
// no link traffic.  The track carries on when the window closes, as it does
// after `phosphor play ... nowait`.  Only tracks this window started advance to
// the next file when they end, so a track played from the shell is left alone.
//
// The core reports no duration, so the progress bar is shown only for WAV and
// FLAC, whose headers give the length exactly (phosphor_media.h); every format
// shows its elapsed time.  Whether a Phosphor core is there at all is read from
// its magic register every few seconds; without one the transport is dimmed and
// the window says how to load it.

#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern "C" {
#include "ff.h"
#include "td_apps.h"
void td_phosphor_register(void);
}

#include "fpga_debug.h"
#include "phosphor_media.h"
#include "phosphor_player.h"

namespace {

constexpr char MUSIC_REAL[] = "/sd/music";
constexpr char MUSIC_SHOWN[] = "/music";
constexpr int MAX_TRACKS = 256;
constexpr int NAME_BYTES = 96;
constexpr uint32_t TICK_MS = 500;
constexpr uint32_t CORE_CHECK_TICKS = 10;  // every five seconds
constexpr uint32_t CORE_CHECK_TIMEOUT_MS = 100;
constexpr uint32_t PHOSPHOR_MAGIC_ADDRESS = 0x0000;
constexpr uint32_t PHOSPHOR_MAGIC = 0x54504830;  // "TPH0"

struct entry {
    char name[NAME_BYTES];
};

td_window_t *s_win;
td_widget_t *s_folder, *s_list, *s_title, *s_line, *s_bar, *s_message, *s_auto;
td_widget_t *s_buttons[5];
td_widget_t *s_pause;

entry *s_entries;
int s_count;
bool s_truncated;

bool s_core;
uint32_t s_ticks;

// The track the status was last read for, and what was worked out from it.
uint32_t s_seen_track;
int s_current = -1;            // its index in the list, -1 if not in /music
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

int compare(const void *a, const void *b)
{
    return strcasecmp(static_cast<const entry *>(a)->name, static_cast<const entry *>(b)->name);
}

void load_folder(void)
{
    s_count = 0;
    s_truncated = false;
    DIR dir;
    FILINFO info;
    if (f_opendir(&dir, MUSIC_REAL) == FR_OK) {
        while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != '\0') {
            if ((info.fattrib & (AM_DIR | AM_HID | AM_SYS)) != 0 ||
                !phosphor_media::supported(info.fname)) {
                continue;
            }
            if (s_count == MAX_TRACKS || strlen(info.fname) >= NAME_BYTES) {
                s_truncated = true;
                continue;
            }
            strcpy(s_entries[s_count++].name, info.fname);
        }
        (void)f_closedir(&dir);
        qsort(s_entries, static_cast<size_t>(s_count), sizeof(entry), compare);
        char text[64];
        snprintf(text, sizeof(text), "%s: %d track%s%s", MUSIC_SHOWN, s_count,
                 s_count == 1 ? "" : "s", s_truncated ? " (some not listed)" : "");
        set_text(s_folder, text);
    } else {
        set_text(s_folder, "No /music folder on the card");
    }
    td_list_set_count(s_list, s_count);
}

// The list index of a status path such as "/music/song.flac", or -1.
int index_of(const char *path)
{
    const size_t prefix = strlen(MUSIC_SHOWN);
    if (strncmp(path, MUSIC_SHOWN, prefix) != 0 || path[prefix] != '/') {
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
        say("No Phosphor core: tangload /cores/console138k/phosphortang.bin");
    }
    return s_core;
}

void play_index(int index)
{
    if (index < 0 || index >= s_count || !need_core()) {
        return;
    }
    char real[sizeof(MUSIC_REAL) + 1 + NAME_BYTES + 16];
    char shown[sizeof(MUSIC_SHOWN) + 1 + NAME_BYTES + 16];
    snprintf(real, sizeof(real), "%s/%s", MUSIC_REAL, s_entries[index].name);
    snprintf(shown, sizeof(shown), "%s/%s", MUSIC_SHOWN, s_entries[index].name);
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

void on_activate(td_widget_t *, void *) { play_index(td_list_selected(s_list)); }

void on_play(td_widget_t *, void *)
{
    const int selected = td_list_selected(s_list);
    play_index(selected >= 0 ? selected : 0);
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
    const int to = phosphor_media::neighbour(from, s_count, direction);
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
    snprintf(text, sizeof(text), "%s%s", playing ? "> " : "  ", s_entries[index].name);
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
        const int next = phosphor_media::neighbour(s_current, s_count, 1);
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
    if (++s_ticks % CORE_CHECK_TICKS == 0) {
        const bool core = check_core();
        if (core != s_core) {
            s_core = core;
            show_core();
            if (!s_core) {
                (void)need_core();
            }
        }
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
    s_entries = static_cast<entry *>(malloc(sizeof(entry) * MAX_TRACKS));
    if (s_entries == nullptr) {
        return;
    }
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

    s_folder = td_label(s_win, 0, 0, 0, "");
    s_list = td_list(s_win, td_rect(0, 1, -1, -8), get_item, on_activate, nullptr);
    td_scrollbar(s_win, -1, 1, -8, s_list);
    s_title = td_label(s_win, 0, -7, 0, "");
    s_line = td_label(s_win, 0, -6, 0, "");
    s_bar = td_progress(s_win, 0, -5, 0);
    s_buttons[0] = td_button(s_win, 0, -3, "Prev", on_prev, nullptr);
    s_buttons[1] = td_button(s_win, 9, -3, "Play", on_play, nullptr);
    s_buttons[2] = s_pause = td_button(s_win, 18, -3, "Pause ", on_pause, nullptr);
    s_buttons[3] = td_button(s_win, 29, -3, "Stop", on_stop, nullptr);
    s_buttons[4] = td_button(s_win, 38, -3, "Next", on_next, nullptr);
    s_auto = td_checkbox(s_win, 0, -2, "Play the next track when one ends", true, nullptr,
                         nullptr);
    s_message = td_label(s_win, 0, -1, 0, "");
    td_widget_set_color(s_message, td_theme()->dim, TD_COLOR_DEFAULT);

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
