// TinyTang: TinyCreature, the window that starts the fixed Celtic scene.
//
// Version one intentionally has one scene. TinyCreature's plant table is
// compiled from the Celtic level script, so presenting arbitrary .tcl files
// as interchangeable worlds would pair data that do not belong together.
// The app uses the same in-process core-load and AE350-run path as TinyNES,
// TinyGBC and Tiny89, then restores the desktop core on every completed run.

#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern "C" {
#include "ff.h"
#include "td_apps.h"
#include "tinydesk/td.h"
#include "tang_ae350_run.h"
#include "tang_core_load.h"

int tdsh_printf(const char *fmt, ...);
}

#include "FreeRTOS.h"
#include "task.h"

namespace {

constexpr char CORE_REAL[] = "/sd/root/cores/console138k/tinycreature.bin";
constexpr char PLAYER_REAL[] = "/sd/root/ae350/tinycreature-play.elf";
constexpr char SCENE_REAL[] = "/sd/root/media/tinycreature/celtic.tcl";
constexpr char DESKTOP_REAL[] = "/sd/root/cores/console138k/tinytang.bin";

constexpr int WIN_W = 54;
constexpr int WIN_H = 18;
constexpr int TABS = 2;
constexpr int PAGE_WIDGETS = 16;
constexpr uint32_t TICK_MS = 400;
constexpr uint32_t LAUNCH_STACK_WORDS = 2048;
constexpr int ACCENT = 14;

td_window_t *s_win;
td_widget_t *s_tab[TABS];
td_widget_t *s_page[TABS][PAGE_WIDGETS];
int s_page_count[TABS];
int s_tab_shown = -1;
td_widget_t *s_core_line;
td_widget_t *s_player_line;
td_widget_t *s_scene_line;
td_widget_t *s_status;
bool s_launching;
bool s_finished;
int s_last_code;

void page_add(int tab, td_widget_t *widget)
{
    if (widget != nullptr && s_page_count[tab] < PAGE_WIDGETS) {
        s_page[tab][s_page_count[tab]++] = widget;
    }
}

void show_tab(int tab)
{
    if (tab == s_tab_shown) {
        return;
    }
    for (int t = 0; t < TABS; ++t) {
        for (int i = 0; i < s_page_count[t]; ++i) {
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

bool installed(const char *path)
{
    FILINFO info;
    return f_stat(path, &info) == FR_OK && (info.fattrib & AM_DIR) == 0 &&
           info.fsize != 0;
}

bool refresh_install(void)
{
    const bool core = installed(CORE_REAL);
    const bool player = installed(PLAYER_REAL);
    const bool scene = installed(SCENE_REAL);
    td_widget_set_text(s_core_line, core ? "Core:    installed" : "Core:    missing");
    td_widget_set_text(s_player_line,
                       player ? "Player:  installed" : "Player:  missing");
    td_widget_set_text(s_scene_line,
                       scene ? "Scene:   Celtic, installed" : "Scene:   Celtic, missing");
    if (!s_launching && !s_finished) {
        say(core && player && scene ? "Ready" : "Run TinyCreature's launcher deploy first");
    }
    return core && player && scene;
}

void launch_task(void *)
{
    const tang_load_result_t loaded = tang_core_load(CORE_REAL);
    int code = 1;
    if (loaded == TANG_LOAD_OK) {
        code = tang_ae350_run(PLAYER_REAL, SCENE_REAL);
        /* The player exits on F12 or Escape. Put the desktop back regardless
         * of whether it reported success or a failed terrain check. */
        (void)tang_core_load(DESKTOP_REAL);
    } else if (loaded == TANG_LOAD_BUSY) {
        tdsh_printf("tinycreature: another core load is already running\r\n");
    } else {
        tdsh_printf("tinycreature: the core did not load\r\n");
    }
    tdsh_printf("tinycreature: done (%d), %u bytes of stack left\r\n", code,
                (unsigned)(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)));
    s_last_code = code;
    s_finished = true;
    s_launching = false;
    vTaskDelete(nullptr);
}

void on_start(td_widget_t *, void *)
{
    if (s_launching) {
        say("TinyCreature is already starting");
        return;
    }
    s_finished = false;
    if (!refresh_install()) {
        return;
    }
    say("Starting Celtic ...");
    s_launching = true;
    if (xTaskCreate(launch_task, "tclaunch", LAUNCH_STACK_WORDS, nullptr,
                    tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
        s_launching = false;
        say("Not enough memory to start TinyCreature");
    }
}

void on_refresh(td_widget_t *, void *)
{
    s_finished = false;
    (void)refresh_install();
}

void on_tab(td_widget_t *, void *ctx)
{
    show_tab(static_cast<int>(reinterpret_cast<intptr_t>(ctx)));
}

void on_close(td_window_t *)
{
    s_win = nullptr;
    s_core_line = nullptr;
    s_player_line = nullptr;
    s_scene_line = nullptr;
    s_status = nullptr;
}

void on_tick(td_window_t *)
{
    if (!s_finished || s_launching) {
        return;
    }
    s_finished = false;
    char text[48];
    snprintf(text, sizeof text, "Returned to TinyDesk (%d)", s_last_code);
    (void)refresh_install();
    say(text);
}

void launch(void)
{
    if (s_win != nullptr && td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }

    td_window_desc_t desc = {};
    desc.title = "TinyCreature";
    desc.rect = td_rect(-1, -1, WIN_W, WIN_H);
    desc.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE;
    desc.min_w = WIN_W;
    desc.min_h = WIN_H;
    desc.on_close = on_close;
    desc.on_tick = on_tick;
    desc.tick_ms = TICK_MS;
    desc.icon = "TC";
    s_win = td_win_create(&desc);
    if (s_win == nullptr) {
        return;
    }

    memset(s_page_count, 0, sizeof s_page_count);
    s_tab_shown = -1;
    s_tab[0] = td_button(s_win, 1, 0, "Launch", on_tab, (void *)0);
    s_tab[1] = td_button(s_win, 12, 0, "Controls", on_tab, (void *)1);

    page_add(0, td_label(s_win, 1, 2, 0, "TinyCreature: the Celtic God land"));
    page_add(0, td_label(s_win, 1, 4, 0, "One matched player and scene:"));
    s_core_line = td_label(s_win, 3, 6, 0, "");
    s_player_line = td_label(s_win, 3, 7, 0, "");
    s_scene_line = td_label(s_win, 3, 8, 0, "");
    page_add(0, s_core_line);
    page_add(0, s_player_line);
    page_add(0, s_scene_line);
    page_add(0, td_button(s_win, 1, 12, "Start", on_start, nullptr));
    page_add(0, td_button(s_win, 10, 12, "Refresh", on_refresh, nullptr));
    s_status = td_label(s_win, 1, 15, 0, "");
    td_widget_set_color(s_status, td_theme()->dim, TD_COLOR_DEFAULT);
    page_add(0, s_status);

    page_add(1, td_label(s_win, 1, 2, 0, "Mouse controls while the scene is shown:"));
    page_add(1, td_label(s_win, 3, 4, 0, "Middle-drag   turn and tilt"));
    page_add(1, td_label(s_win, 3, 5, 0, "Left-drag     move across the land"));
    page_add(1, td_label(s_win, 3, 6, 0, "Wheel         move in and out"));
    page_add(1, td_label(s_win, 1, 9, 0, "F12 or Escape returns to TinyDesk."));
    page_add(1, td_label(s_win, 1, 11, 0, "The game data comes from your own copy"));
    page_add(1, td_label(s_win, 1, 12, 0, "of Black & White and is not distributed."));

    show_tab(0);
    (void)refresh_install();
}

const td_app_t s_app = {"TinyCreature", launch, "TC"};

} // namespace

extern "C" void td_tinycreature_register(void)
{
    (void)td_app_register(&s_app);
}
