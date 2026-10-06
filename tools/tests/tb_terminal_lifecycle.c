/* SPDX-License-Identifier: MIT
 * Exercise the real BL616 bridge with upstream Terminal's retained state.
 * The scheduler stand-in completes the shell when shutdown asks it to stop.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "task.h"
#include "td_apps.h"
#include "tinydesk/td_vterm.h"
#include "tdsh_bl616.h"

int tang_td_terminal_start(void);
void td_bridge_bl616_stop(void);

static int starts, deletes, columns;
static bool fail_start;
static void (*shell_fn)(void *);
static void *shell_arg;
static int (*input)(void);
static int (*output)(const void *, size_t);
static const td_stats_t stats = {.cols = 80, .rows = 45};

BaseType_t xTaskCreate(void (*fn)(void *), const char *name,
                       uint32_t words, void *arg, unsigned priority, void *handle)
{
    assert(strcmp(name, "tdsh") == 0 && words == 4096 && priority == 4);
    ++starts;
    if (fail_start)
        return pdFAIL;
    shell_fn = fn;
    shell_arg = arg;
    *(TaskHandle_t *)handle = (TaskHandle_t)1;
    return pdPASS;
}

void vTaskDelete(TaskHandle_t task)
{
    assert(task == NULL); /* The graceful exit must avoid forced deletion. */
    ++deletes;
    shell_fn = NULL;
}

void vTaskDelay(TickType_t ticks)
{
    (void)ticks;
    if (shell_fn)
        shell_fn(shell_arg);
}

TickType_t xTaskGetTickCount(void)
{
    return 0;
}
int tdsh_bl616_run_until(volatile const bool *stop)
{
    assert(*stop);
    return 0;
}
void tdsh_bl616_terminal_set_columns(int cols)
{
    columns = cols;
}
void tdsh_bl616_terminal_set_io(int (*read_fn)(void),
                                int (*write_fn)(const void *, size_t))
{
    input = read_fn;
    output = write_fn;
}

const td_stats_t *td_stats(void)
{
    return &stats;
}
int td_wm_desktop_rows(void)
{
    return 44;
}
uint32_t td_millis(void)
{
    return 0;
}
void td_vterm_init(td_vterm_t *vt, int cols, int rows)
{
    vt->cols = cols;
    vt->rows = rows;
}
void td_vterm_write(td_vterm_t *vt, const uint8_t *data, int len)
{
    (void)vt;
    (void)data;
    (void)len;
}
bool td_win_is_open(const td_window_t *win)
{
    (void)win;
    return false;
}
void td_win_invalidate(td_window_t *win)
{
    (void)win;
}
int td_timer_start(uint32_t interval, bool repeat, td_timer_fn fn,
                   void *user, uint32_t now)
{
    (void)interval;
    (void)repeat;
    (void)fn;
    (void)user;
    (void)now;
    return 0;
}
void td_session_init(void)
{
}

static void check_input(void)
{
    const td_term_backend_t *backend = td_terminal_backend();
    assert(backend != NULL && input != NULL && output != NULL);
    assert(columns == 76);
    const uint8_t text[] = "hello\r";
    assert(backend->write(NULL, text, sizeof(text) - 1) == sizeof(text) - 1);
    for (size_t i = 0; i < sizeof(text) - 1; ++i)
        assert(input() == text[i]);
    assert(input() == -1);
    assert(output("hello", 5) == 5);
    uint8_t received[8];
    assert(backend->read(NULL, received, sizeof(received)) == 5);
    assert(memcmp(received, "hello", 5) == 0);
}

int main(void)
{
    for (int cycle = 1; cycle <= 3; ++cycle)
    {
        assert(tang_td_terminal_start() == 0);
        assert(starts == cycle);
        check_input();
        /* Installing the backend twice must not create another shell. */
        td_terminal_set_backend(td_terminal_backend());
        assert(starts == cycle);
        td_bridge_bl616_stop();
        assert(deletes == cycle && input == NULL && output == NULL);
        td_bridge_bl616_stop();
        assert(deletes == cycle);
    }
    fail_start = true;
    assert(tang_td_terminal_start() == -1);
    assert(starts == 4 && input == NULL && output == NULL);
    td_bridge_bl616_stop();
    fail_start = false;
    assert(tang_td_terminal_start() == 0);
    assert(starts == 5);
    check_input();
    td_bridge_bl616_stop();
    puts("terminal lifecycle: PASS (reentry, input, cleanup, failed start, retry)");
    return 0;
}
