/* SPDX-License-Identifier: MIT
 * The OLED terminal's independent shell session, run on real threads: the
 * real app (ports/bl616/tang_oled.c), the real per-task terminal routes and
 * script workers (ports/bl616/tdsh_platform_bl616.c) and the real printf
 * layer, inside TinyDesk's real window manager.  The shell core is replaced by
 * a line reader and a small command set, and the USB console by a recorder.
 *
 * Checked: one session however many callers start it at once, and a failed
 * start that can be retried; the OLED session's output reaches the OLED and
 * never USB, and the console's never the OLED, also when both print at once;
 * script workers, foreground and background, inherit the route of the shell
 * that started them, including its input; keys reach the session only while
 * its window has focus.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "tang_oled.h"
#include "tdsh.h"
#include "tdsh_bl616.h"
#include "tdsh_platform.h"
#include "tdsh_terminal.h"
#include "tinydesk/td.h"

int tdsh_printf(const char *fmt, ...);

static int failures;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            failures++;                                                   \
        }                                                                 \
    } while (0)

/* ------------------------------------------------------------ the USB CDC */
static pthread_mutex_t g_usb_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_usb_out[65536];
static size_t g_usb_len;
static char g_usb_in[64];
static size_t g_usb_in_head, g_usb_in_tail;

static int usb_append(const void *data, size_t length)
{
    pthread_mutex_lock(&g_usb_lock);
    if (g_usb_len + length < sizeof(g_usb_out)) {
        memcpy(g_usb_out + g_usb_len, data, length);
        g_usb_len += length;
        g_usb_out[g_usb_len] = '\0';
    }
    pthread_mutex_unlock(&g_usb_lock);
    return (int)length;
}
int tdsh_bl616_console_write(const void *data, size_t length) { return usb_append(data, length); }
int tdsh_bl616_console_write_usb(const void *data, size_t length) { return usb_append(data, length); }
int tdsh_bl616_console_read_byte(void)
{
    int b = -1;
    pthread_mutex_lock(&g_usb_lock);
    if (g_usb_in_tail != g_usb_in_head) b = (uint8_t)g_usb_in[g_usb_in_tail++];
    pthread_mutex_unlock(&g_usb_lock);
    return b;
}
static void usb_type(char c)
{
    pthread_mutex_lock(&g_usb_lock);
    g_usb_in[g_usb_in_head++] = c;
    pthread_mutex_unlock(&g_usb_lock);
}
static bool usb_has(const char *text)
{
    pthread_mutex_lock(&g_usb_lock);
    bool found = strstr(g_usb_out, text) != NULL;
    pthread_mutex_unlock(&g_usb_lock);
    return found;
}
static size_t usb_unread(void)
{
    pthread_mutex_lock(&g_usb_lock);
    size_t n = g_usb_in_head - g_usb_in_tail;
    pthread_mutex_unlock(&g_usb_lock);
    return n;
}

/* --------------------------------------------- the rest of the firmware */
bool tang_osd_desk_enabled(void) { return false; }
int tang_osd_desk_read_byte(void) { return -1; }
int tang_ble_register(void) { return 0; }
void tang_ble_boot(void) { }
int tang_crash_register(void) { return 0; }
void tang_crash_report(void) { }
int tang_desktop_register(void) { return 0; }
int tang_osd_register(void) { return 0; }
int tang_phosphor_register(void) { return 0; }
int tang_tangini_register(void) { return 0; }
int tang_usb_role_register(void) { return 0; }
int tang_usbstat_register(void) { return 0; }
int tdsh_bl616_fpga_register(void) { return 0; }
int tdsh_bl616_tang_flash_register(void) { return 0; }
int tdsh_bl616_fs_last_result(void) { return 0; }
int tdsh_bl616_fs_mount(void) { return 0; }
bool tdsh_bl616_fs_ready(void) { return true; }
int bflb_trng_readlen(uint8_t *data, uint32_t len) { memset(data, 0, len); return 0; }

/* ------------------------------------------------------ the shell core */
static const tdsh_platform_api_t *g_platform;
static const tdsh_command_t *g_oledterm;

int tdsh_core_init(const tdsh_core_config_t *config) { g_platform = config->platform; return 0; }
int tdsh_register_core_builtins(void) { return 0; }
int tdsh_register_commands(const tdsh_command_t *commands, size_t count)
{
    (void)commands; (void)count;
    return 0;
}
int tdsh_register_command(const tdsh_command_t *command)
{
    if (strcmp(command->name, "oledterm") == 0) g_oledterm = command;
    return 0;
}
int tdsh_session_init(tdsh_session_t *session, const char *username, bool interactive)
{
    memset(session, 0, sizeof(*session));
    snprintf(session->username, sizeof(session->username), "%s", username);
    snprintf(session->cwd, sizeof(session->cwd), "/root");
    snprintf(session->home, sizeof(session->home), "/root");
    session->interactive = interactive;
    return 0;
}

/* A line reader: echo, Ctrl-U clears, Backspace erases, CR ends. */
int tdsh_terminal_readline(tdsh_session_t *session, const tdsh_terminal_io_t *io,
                           const char *prompt, char *buffer, size_t capacity)
{
    (void)session;
    io->write_bytes(io->context, prompt, strlen(prompt));
    size_t n = 0;
    for (;;) {
        uint8_t b;
        if (io->read_byte(io->context, &b) != 0) return -EIO;
        if (b == '\r') break;
        if (b == 21) {
            n = 0;
        } else if (b == 127) {
            if (n) n--;
        } else if (n + 1 < capacity) {
            buffer[n++] = (char)b;
            io->write_bytes(io->context, &b, 1);
        }
    }
    buffer[n] = '\0';
    io->write_bytes(io->context, "\r\n", 2);
    return (int)n;
}

/* What the commands saw: the line and the route of the task running it. */
static pthread_mutex_t g_rec_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_last_line[128];
static int g_lines;
static tdsh_bl616_route_t *g_line_route, *g_worker_route, *g_bg_route;
static int g_worker_byte = -2, g_bg_done;

static int worker(void *arg)
{
    tdsh_printf("W:ready\r\n");
    int b = -1;
    for (int i = 0; i < 400 && b < 0; i++) {
        b = tdsh_bl616_input_read_byte();
        if (b < 0) vTaskDelay(5);
    }
    pthread_mutex_lock(&g_rec_lock);
    g_worker_route = tdsh_bl616_route_current();
    g_worker_byte = b;
    pthread_mutex_unlock(&g_rec_lock);
    tdsh_printf("W:%c\r\n", b < 0 ? '-' : b);
    return *(int *)arg;
}

static int bg_worker(void *arg)
{
    (void)arg;
    tdsh_printf("BG\r\n");
    pthread_mutex_lock(&g_rec_lock);
    g_bg_route = tdsh_bl616_route_current();
    g_bg_done = 1;
    pthread_mutex_unlock(&g_rec_lock);
    return 0;
}

int tdsh_execute_line(tdsh_session_t *session, const char *line)
{
    (void)session;
    pthread_mutex_lock(&g_rec_lock);
    snprintf(g_last_line, sizeof(g_last_line), "%s", line);
    g_line_route = tdsh_bl616_route_current();
    g_lines++;
    pthread_mutex_unlock(&g_rec_lock);
    if (strncmp(line, "echo ", 5) == 0) {
        tdsh_printf("%s\r\n", line + 5);
    } else if (strcmp(line, "worker") == 0) {
        static int seven = 7;
        int result = 0;
        g_platform->worker_run(g_platform->context, "w", 4096, 1, false, worker, &seven,
                               NULL, &result);
        tdsh_printf("W:result %d\r\n", result);
    } else if (strcmp(line, "bg") == 0) {
        g_platform->worker_run(g_platform->context, "bg", 4096, 1, true, bg_worker, NULL,
                               NULL, NULL);
    } else if (strcmp(line, "flood") == 0) {
        for (int i = 0; i < 200; i++) tdsh_printf("o%03d\r\n", i);
    }
    return 0;
}

/* ------------------------------------------------------------ helpers */
static bool oled_has(const char *text)
{
    uint16_t cells[384];
    uint32_t cursor;
    if (!tang_oled_snapshot(cells, &cursor)) return false;
    for (int y = 0; y < 16; y++) {
        char row[25];
        for (int x = 0; x < 24; x++) row[x] = (char)(cells[y * 24 + x] & 0xff);
        row[24] = '\0';
        if (strstr(row, text)) return true;
    }
    return false;
}
static bool wait_oled(const char *text)
{
    for (int i = 0; i < 400; i++) {
        if (oled_has(text)) return true;
        vTaskDelay(5);
    }
    return false;
}
static int oledterm(const char *a, const char *b)
{
    char *argv[3] = {"oledterm", (char *)a, (char *)b};
    return g_oledterm->fn(NULL, b ? 3 : 2, argv);
}
static int lines_seen(void)
{
    pthread_mutex_lock(&g_rec_lock);
    int n = g_lines;
    pthread_mutex_unlock(&g_rec_lock);
    return n;
}
static bool wait_lines(int n)
{
    for (int i = 0; i < 400 && lines_seen() < n; i++) vTaskDelay(5);
    return lines_seen() >= n;
}

static uint32_t s_now = 1000;
static const char *s_reply = "\x1b[45;80R";
static int fake_read(void *ctx) { (void)ctx; return *s_reply ? (uint8_t)*s_reply++ : -1; }
static int fake_write(void *ctx, const uint8_t *b, int n) { (void)ctx; (void)b; return n; }
static uint32_t fake_millis(void *ctx) { (void)ctx; return s_now; }
static void fake_sleep(void *ctx, uint32_t ms) { (void)ctx; s_now += ms; }
static const td_hal_t s_hal = {fake_read, fake_write, fake_millis, fake_sleep, NULL};

static void key(uint32_t k)
{
    td_event_t ev = {0};
    ev.type = TD_EV_KEY;
    ev.key = k;
    ev.time_ms = s_now;
    td_wm_dispatch(&ev);
}
static void type(const char *s)
{
    while (*s) key((uint8_t)*s++);
}

static pthread_barrier_t g_barrier;
static int g_race_rc[8];
static void *race(void *arg)
{
    int i = (int)(intptr_t)arg;
    pthread_barrier_wait(&g_barrier);
    g_race_rc[i] = tang_oled_start();
    return NULL;
}

static bool other_event(td_window_t *w, const td_event_t *ev) { (void)w; (void)ev; return true; }

int main(void)
{
    CHECK(tdsh_bl616_init("tinytang") == 0);
    CHECK(g_platform != NULL && g_oledterm != NULL);
    td_init(&s_hal);
    td_oled_terminal_register();

    /* A start that cannot create its task fails cleanly and can be retried. */
    uint16_t cells[384];
    uint32_t cursor;
    stub_task_create_budget = 0;
    CHECK(tang_oled_start() == -ENOMEM);
    CHECK(!tang_oled_snapshot(cells, &cursor));
    stub_task_create_budget = -1;

    /* Eight callers at once start one session. */
    const int before = stub_tasks_created;
    pthread_barrier_init(&g_barrier, NULL, 8);
    pthread_t threads[8];
    for (int i = 0; i < 8; i++) pthread_create(&threads[i], NULL, race, (void *)(intptr_t)i);
    for (int i = 0; i < 8; i++) pthread_join(threads[i], NULL);
    CHECK(stub_tasks_created - before == 1);
    int zeros = 0;
    for (int i = 0; i < 8; i++) {
        CHECK(g_race_rc[i] == 0 || g_race_rc[i] == -EBUSY);
        zeros += g_race_rc[i] == 0;
    }
    CHECK(zeros >= 1);
    CHECK(tang_oled_start() == 0);

    /* The banner and prompt reach the OLED, not USB. */
    CHECK(wait_oled("OLED Terminal 24x16"));
    CHECK(wait_oled("oled:/root#"));
    CHECK(!usb_has("OLED Terminal"));
    CHECK(tdsh_bl616_route_current() == NULL);

    /* A command submitted from the console runs in the OLED session, on the
     * OLED's route, and its output stays there. */
    CHECK(oledterm("run", "echo hello-oled") == 0);
    CHECK(wait_oled("hello-oled"));
    CHECK(wait_lines(1));
    tdsh_bl616_route_t *oled_route = g_line_route;
    CHECK(oled_route != NULL);
    CHECK(strcmp(g_last_line, "echo hello-oled") == 0);
    CHECK(!usb_has("hello-oled"));
    tdsh_printf("hello-usb\r\n");
    CHECK(usb_has("hello-usb"));
    CHECK(!oled_has("hello-usb"));

    /* The window takes keys only while it has focus. */
    CHECK(td_app_launch("OLED Terminal"));
    td_window_t *oled_win = td_win_focused();
    CHECK(oled_win != NULL);
    td_window_desc_t other_desc = {.title = "Other", .rect = {0, 0, 20, 6},
                                   .flags = TD_WIN_RAW_KEYS, .on_event = other_event};
    td_window_t *other = td_win_create(&other_desc);
    CHECK(other != NULL && td_win_focused() == other);
    type("xyz");
    td_win_focus(oled_win);
    int n = lines_seen();
    type("echo typed");
    key(TD_KEY_ENTER);
    CHECK(wait_lines(n + 1));
    CHECK(strcmp(g_last_line, "echo typed") == 0);
    CHECK(wait_oled("typed"));

    /* A foreground worker started by the OLED shell writes to the OLED and
     * reads the OLED's keys, never the USB console's. */
    usb_type('z');
    n = lines_seen();
    CHECK(oledterm("run", "worker") == 0);
    CHECK(wait_oled("W:ready"));
    key('q');
    CHECK(wait_oled("W:result 7"));
    CHECK(g_worker_route == oled_route);
    CHECK(g_worker_byte == 'q');
    CHECK(oled_has("W:q"));
    CHECK(usb_unread() == 1);
    CHECK(!usb_has("W:"));

    /* The same worker started from the console stays on the console. */
    int result = 0;
    static int eight = 8;
    CHECK(g_platform->worker_run(NULL, "w", 4096, 1, false, worker, &eight, NULL, &result) == 0);
    CHECK(result == 8);
    CHECK(g_worker_route == NULL);
    CHECK(g_worker_byte == 'z');
    CHECK(usb_has("W:ready") && usb_has("W:z"));
    CHECK(usb_unread() == 0);

    /* A background worker inherits the route too. */
    CHECK(oledterm("run", "bg") == 0);
    for (int i = 0; i < 400 && !g_bg_done; i++) vTaskDelay(5);
    CHECK(g_bg_done && g_bg_route == oled_route);
    CHECK(wait_oled("BG"));
    CHECK(!usb_has("BG"));

    /* Both shells printing at once: nothing crosses over. */
    CHECK(oledterm("run", "flood") == 0);
    for (int i = 0; i < 200; i++) tdsh_printf("u%03d\r\n", i);
    CHECK(wait_oled("o199"));
    char marker[8];
    bool usb_all = true;
    for (int i = 0; i < 200; i++) {
        snprintf(marker, sizeof(marker), "u%03d", i);
        usb_all &= usb_has(marker);
    }
    CHECK(usb_all);
    CHECK(!usb_has("o0") && !usb_has("o1"));
    CHECK(!oled_has("u"));

    if (failures) {
        printf("test_oled_session: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_oled_session: all checks passed\n");
    return 0;
}
