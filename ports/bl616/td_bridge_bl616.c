// TinyTang — TinyDesk Shell inside TinyDesk's Terminal window.
//
// The desktop draws a Terminal app, and a td_term_backend_t is what fills it.
// This is that backend: the window's keys go into one ring, the shell's output
// comes back out of another, and a task runs the shell against them.  Without
// one the app draws "No shell backend in this build." — that message is
// emitted for exactly one reason, s_backend being NULL.
//
//   desktop (the outer shell task)          "tdsh" (this shell task)
//     keys   --write()--> [in ring]  --> terminal read
//     vterm  <--read()--- [out ring] <-- terminal write
//
// Both rings are single-producer, single-consumer, so neither needs a lock:
// only the desktop writes the in ring and only the shell reads it, and the
// reverse holds for the out ring.
//
// Only one shell runs at a time, which is what keeps this small.  The outer
// shell -- the console shell that started the desktop -- is parked inside the
// command that launched it and is neither reading nor writing, so the terminal
// redirect can be a single global and the inner shell can share the outer's
// session.  That is enough here: one user, one session, and a window onto it.
//
// The shell's output is dropped rather than allowed to wedge if the window
// stops being drained -- closing the Terminal leaves nobody reading the out
// ring, and blocking the shell forever because a window is shut would be a
// worse failure than losing some of its output.

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "FreeRTOS.h"
#include "task.h"

#include "td_apps.h"
#include "tinydesk/td.h"
#include "tdsh.h"
#include "tdsh_bl616.h"

#define IN_RING          1024u
#define OUT_RING         8192u
#define SHELL_TASK_WORDS 4096
#define SHELL_PRIORITY   4          /* below the UI, so typing stays smooth */
#define OUT_STALL_MS     100        /* how long a full out ring may block */

static uint8_t           s_in[IN_RING];
static volatile unsigned s_in_head, s_in_tail;

static uint8_t           s_out[OUT_RING];
static volatile unsigned s_out_head, s_out_tail;

static volatile bool s_stop;
static volatile bool s_finished;
static bool          s_started;
static TaskHandle_t  s_task;
static unsigned      s_dropped;

/* ------------------------------------------------- the shell's side of it */

/* Non-blocking, and -1 when empty: the terminal read contract. */
static int ring_read_in(void)
{
    if (s_in_head == s_in_tail) {
        return -1;
    }
    const int b = s_in[s_in_tail];
    s_in_tail = (s_in_tail + 1u) & (IN_RING - 1u);
    return b;
}

static int ring_write_out(const void *data, size_t length)
{
    const uint8_t *p = (const uint8_t *)data;
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(OUT_STALL_MS);

    for (size_t done = 0; done < length; ) {
        const unsigned next = (s_out_head + 1u) & (OUT_RING - 1u);
        if (next == s_out_tail) {
            /* Full: let the desktop drain it, but do not wait forever. */
            if ((int32_t)(xTaskGetTickCount() - deadline) >= 0) {
                s_dropped += (unsigned)(length - done);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        s_out[s_out_head] = p[done++];
        s_out_head = next;
    }
    return (int)length;
}

static void shell_task(void *arg)
{
    (void)arg;
    (void)tdsh_bl616_run_until(&s_stop);   /* returns when the desktop says so */
    s_finished = true;
    vTaskDelete(NULL);
}

/* ---------------------------------------------- the desktop's side of it */

static int backend_start(void *ctx, int cols, int rows)
{
    (void)ctx; (void)rows;

    /* The window's width, for the shell's line editor to wrap at. */
    tdsh_bl616_terminal_set_columns(cols);

    if (s_started) {
        return 0;
    }

    s_in_head = s_in_tail = 0;
    s_out_head = s_out_tail = 0;
    s_dropped = 0;
    s_stop = false;
    s_finished = false;

    tdsh_bl616_terminal_set_io(ring_read_in, ring_write_out);

    if (xTaskCreate(shell_task, "tdsh", SHELL_TASK_WORDS, NULL,
                    SHELL_PRIORITY, &s_task) != pdPASS) {
        tdsh_bl616_terminal_set_io(NULL, NULL);
        s_task = NULL;
        return -1;
    }

    s_started = true;
    return 0;
}

static int backend_read(void *ctx, uint8_t *buf, int cap)
{
    (void)ctx;
    int n = 0;
    while (n < cap && s_out_head != s_out_tail) {
        buf[n++] = s_out[s_out_tail];
        s_out_tail = (s_out_tail + 1u) & (OUT_RING - 1u);
    }
    return n;
}

static int backend_write(void *ctx, const uint8_t *buf, int len)
{
    (void)ctx;
    int n = 0;
    while (n < len) {
        const unsigned next = (s_in_head + 1u) & (IN_RING - 1u);
        if (next == s_in_tail) {
            break;                  /* full: drop the rest of the keystrokes */
        }
        s_in[s_in_head] = buf[n++];
        s_in_head = next;
    }
    return n;
}

/* The window was resized: the editor reads the width at its next line. */
static void backend_resize(void *ctx, int cols, int rows)
{
    (void)ctx; (void)rows;
    tdsh_bl616_terminal_set_columns(cols);
}

static const char *backend_user(void *ctx)
{
    (void)ctx;
    return "root";
}

static const td_term_backend_t s_backend = {
    .name = "tdsh",
    .start = backend_start,
    .read = backend_read,
    .write = backend_write,
    .resize = backend_resize,
    .user = backend_user,
    .set_user = NULL,       /* one user on this board */
};

const td_term_backend_t *td_bridge_bl616_backend(void)
{
    return &s_backend;
}

/* The Terminal app retains its started flag across td_shutdown(), but this
 * port stops the shell at desktop exit. Start the bridge on every entry;
 * backend_start is idempotent when Terminal installs it again. */
int tang_td_terminal_start(void)
{
    const int cols = td_stats()->cols;
    const int rows = td_wm_desktop_rows();
    const int width = cols - 2 < 82 ? cols - 2 : 82;
    const int height = rows - 1 < 26 ? rows - 1 : 26;
    if (backend_start(NULL, width - 2, height - 2) != 0) {
        return -1;
    }
    td_terminal_set_backend(&s_backend);
    return 0;
}

/* ------------------------------------------------------------- shutdown */

/* Called from desktop_run() once td_run() has returned.  Without this the
 * inner shell would keep running against rings nobody drains, and the terminal
 * redirect would keep pointing away from the console. */
void td_bridge_bl616_stop(void)
{
    if (!s_started) {
        return;
    }

    s_stop = true;

    /* Wake a read that is sitting on an empty in ring, so the loop reaches the
     * stop check instead of waiting for a keystroke that will never come. */
    const uint8_t nl = '\n';
    (void)backend_write(NULL, &nl, 1);

    for (int i = 0; i < 200 && !s_finished; i++) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* The graceful path is the usual one.  This is the fallback for a shell
     * that is somewhere we cannot see -- inside FatFS, say -- and it carries
     * the usual risk of deleting a task mid-syscall.  Leaving it alive would
     * be worse: with the redirect cleared it would start reading the console
     * alongside the outer shell. */
    if (!s_finished && s_task) {
        vTaskDelete(s_task);
    }
    s_task = NULL;

    tdsh_bl616_terminal_set_io(NULL, NULL);
    s_started = false;
}
