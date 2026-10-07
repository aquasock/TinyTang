// TinyTang — the Software Update window: reflash the BL616 from the card.
//
// TinyDesk's own Software Update (apps/update.c) fetches images over the
// network and is left out of this port with the other network apps, but its
// Settings button launches the app by name, so this registers one under the
// same name.  It only ever installs /bl616-firmware.bin from the root of the
// card, through the same engine as `tangflash` (tang_fw_update.c).
//
// The window is red on purpose.  Staging and verification are safe and leave
// the running firmware alone; the commit that follows is not.  Once it starts
// the application is being erased with interrupts off, and power lost then
// leaves a board that must be recovered over USB in BOOT mode.  The commit
// ends in a reset into the vendor loader (FLS-002), so the last screen the
// board draws tells the user to power-cycle it; the FPGA keeps showing it.

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "tinydesk/td.h"
#include "td_apps.h"
#include "tang_fw_update.h"

#define IMAGE_REAL   "/sd/bl616-firmware.bin"
#define IMAGE_SHOWN  "/bl616-firmware.bin"

#define WIN_W        62
#define WIN_H        19
#define TICK_MS      10
#define FINAL_MS     1500   /* let the last screen reach HDMI and the console */

#define RED          196
#define WHITE        231
#define YELLOW       226

enum state {
    ST_BLOCKED,     /* nothing to install, or staging failed: Close only */
    ST_ASK,         /* a valid image: Cancel or I understand */
    ST_STAGING,     /* copying into the staging area and checking it */
    ST_FINAL,       /* full screen, then the commit */
};

static td_window_t *s_win;
static td_widget_t *s_close, *s_cancel, *s_go, *s_bar;
static enum state s_state;
static tang_fw_update_t s_update;
static char s_reason[96];
static TickType_t s_final_start;

static void center(int y, int width, const char *text, uint8_t fg, uint8_t attr)
{
    int x = (width - (int)strlen(text)) / 2;
    td_text(x < 0 ? 0 : x, y, text, fg, RED, attr);
}

static void on_draw(td_window_t *win, int w, int h)
{
    (void)win;
    char line[80];
    td_fill(td_rect(0, 0, w, h), ' ', WHITE, RED);

    if (s_state == ST_FINAL) {
        const int y = h / 2 - 3;
        center(y, w, "!!  WRITING FIRMWARE  -  DO NOT POWER OFF  !!", YELLOW, TD_BOLD);
        center(y + 2, w, "Do not power off or unplug the Tang.", WHITE, TD_BOLD);
        center(y + 4, w, "Wait one minute, then power-cycle the Tang.", WHITE, 0);
        center(y + 5, w, "It will start with the new firmware.", WHITE, 0);
        return;
    }

    center(1, w, "!!  WARNING: YOU MAY BRICK YOUR TANG  !!", YELLOW, TD_BOLD);
    td_text(2, 3, "This replaces the BL616 firmware with", WHITE, RED, 0);
    if (s_state == ST_BLOCKED) {
        td_text(2, 4, IMAGE_SHOWN " from the SD card.", WHITE, RED, 0);
    } else {
        snprintf(line, sizeof(line), IMAGE_SHOWN " from the SD card (%u bytes).",
                 (unsigned)s_update.image_size);
        td_text(2, 4, line, WHITE, RED, 0);
    }
    td_text(2, 6, "Do not power off or unplug the Tang until told to.", WHITE, RED, TD_BOLD);
    td_text(2, 7, "If the update is interrupted, the Tang will not start", WHITE, RED, 0);
    td_text(2, 8, "and must be recovered over USB in BOOT mode.", WHITE, RED, 0);
    td_text(2, 9, "Unsaved work will be lost.", WHITE, RED, 0);

    if (s_state == ST_BLOCKED) {
        td_textn(2, 11, s_reason, w - 4, YELLOW, RED, TD_BOLD);
    } else if (s_state == ST_STAGING) {
        snprintf(line, sizeof(line), "Copying and checking the update: %u%%",
                 tang_fw_update_percent(&s_update));
        td_text(2, 11, line, WHITE, RED, 0);
    }
}

static void show(enum state state)
{
    s_state = state;
    td_widget_set_visible(s_close, state == ST_BLOCKED);
    td_widget_set_visible(s_cancel, state == ST_ASK);
    td_widget_set_visible(s_go, state == ST_ASK);
    td_widget_set_visible(s_bar, state == ST_STAGING);
    if (state == ST_BLOCKED) {
        td_widget_focus(s_close);
    } else if (state == ST_ASK) {
        td_widget_focus(s_cancel);   /* a stray Enter must not start it */
    }
    td_win_invalidate(s_win);
}

static void on_tick(td_window_t *win)
{
    if (s_state == ST_STAGING) {
        bool done = false;
        const char *why = tang_fw_update_step(&s_update, &done);
        if (why != NULL) {
            snprintf(s_reason, sizeof(s_reason),
                     "Update stopped: %s. The firmware is unchanged.", why);
            show(ST_BLOCKED);
            return;
        }
        td_progress_set(s_bar, (int)tang_fw_update_percent(&s_update));
        td_win_invalidate(win);
        if (done) {
            show(ST_FINAL);
            td_win_set_fullscreen(win, true);
            s_final_start = xTaskGetTickCount();
        }
    } else if (s_state == ST_FINAL &&
               xTaskGetTickCount() - s_final_start >= pdMS_TO_TICKS(FINAL_MS)) {
        tang_fw_update_commit(&s_update);
    }
}

static void on_understand(td_widget_t *w, void *user)
{
    (void)w;
    (void)user;
    if (s_state == ST_ASK) {
        td_progress_set(s_bar, 0);
        show(ST_STAGING);
    }
}

static void on_dismiss(td_widget_t *w, void *user)
{
    (void)w;
    (void)user;
    td_win_request_close(s_win);
}

/* Once staging starts the window stays until the commit. */
static bool on_close_request(td_window_t *win)
{
    (void)win;
    return s_state == ST_BLOCKED || s_state == ST_ASK;
}

static void on_close(td_window_t *win)
{
    (void)win;
    tang_fw_update_close(&s_update);
    s_win = NULL;
}

static void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }
    td_window_desc_t d = {0};
    d.title = "Software Update";
    d.rect = td_rect(-1, -1, WIN_W, WIN_H);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE;
    d.on_draw = on_draw;
    d.on_close = on_close;
    d.on_close_request = on_close_request;
    d.on_tick = on_tick;
    d.tick_ms = TICK_MS;
    s_win = td_win_create(&d);
    if (s_win == NULL) {
        return;
    }

    const int client_w = WIN_W - 2;
    s_bar = td_progress(s_win, 2, 12, client_w - 4);
    s_close = td_button(s_win, 2, 14, "Close", on_dismiss, NULL);
    s_cancel = td_button(s_win, 2, 14, "Cancel", on_dismiss, NULL);
    s_go = td_button(s_win, client_w - 18, 14, "I understand", on_understand, NULL);

    const char *why = tang_fw_update_open(&s_update, IMAGE_REAL);
    if (why == NULL) {
        show(ST_ASK);
    } else {
        if (strcmp(why, "no such file") == 0) {
            snprintf(s_reason, sizeof(s_reason), "No " IMAGE_SHOWN " on the SD card.");
        } else {
            snprintf(s_reason, sizeof(s_reason), "Cannot update: %s.", why);
        }
        show(ST_BLOCKED);
    }
}

static const td_app_t s_app = {"Software Update", launch, "\xE2\x86\x91 "};   /* ↑ */

void td_update_register(void)
{
    td_app_register(&s_app);
}
