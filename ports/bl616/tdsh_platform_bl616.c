// TinyTang — the BL616 platform port for TinyDesk Shell.
//
// Implements tdsh_platform_api_t and drives the portable line editor over the
// USB CDC console.  The shell core in third_party/tinydesk-shell is compiled
// unchanged; everything board-specific lives here, which is what the shell's
// own porting document asks for.

#include "tdsh_bl616.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "bflb_mtimer.h"
#include "bflb_sec_trng.h"

#include "tdsh.h"
#include "dirent.h"
#include <sys/stat.h>
#include "tdsh_terminal.h"

/* ---------------------------------------------------------------- platform */

static uint64_t bl616_monotonic_ms(void *context)
{
    (void)context;
    return bflb_mtimer_get_time_ms();
}

static void bl616_sleep_ms(void *context, uint32_t ms)
{
    (void)context;
    if (ms == 0) {
        taskYIELD();
    } else {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
}

static void bl616_yield(void *context)
{
    (void)context;
    taskYIELD();
}

static int bl616_random_bytes(void *context, void *buffer, size_t length)
{
    (void)context;
    uint8_t *p = (uint8_t *)buffer;
    if (bflb_trng_readlen(p, (uint32_t)length) == 0) {
        return 0;
    }
    /* Fallback only: a timer-seeded xorshift is not a cryptographic source,
     * so it is used solely if the hardware TRNG refuses. */
    uint64_t s = bflb_mtimer_get_time_us() ^ 0x9E3779B97F4A7C15ull;
    for (size_t i = 0; i < length; i++) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        p[i] = (uint8_t)(s >> 24);
    }
    return 0;
}

/* Worker primitive: a FreeRTOS task.  Foreground workers leave the task
 * suspended on a semaphore so the caller can delete it deterministically
 * after it has read the result; background workers delete themselves. */
typedef struct {
    tdsh_worker_fn_t worker;
    tdsh_worker_cleanup_fn_t cleanup;
    void *arg;
    int result;
    SemaphoreHandle_t done;
    bool background;
} bl616_worker_t;

static void bl616_worker_trampoline(void *param)
{
    bl616_worker_t *w = (bl616_worker_t *)param;
    w->result = w->worker(w->arg);
    if (w->cleanup) w->cleanup(w->arg);
    if (w->background) {
        vTaskDelete(NULL);
        return;
    }
    xSemaphoreGive(w->done);
    for (;;) {
        vTaskDelay(portMAX_DELAY);   /* the caller deletes us */
    }
}

static int bl616_worker_run(void *context,
                            const char *name,
                            size_t stack_bytes,
                            int priority,
                            bool background,
                            tdsh_worker_fn_t worker,
                            void *arg,
                            tdsh_worker_cleanup_fn_t cleanup,
                            int *result_out)
{
    (void)context;
    if (!worker) return -1;

    bl616_worker_t *w = (bl616_worker_t *)calloc(1, sizeof(*w));
    if (!w) return -1;
    w->worker = worker;
    w->cleanup = cleanup;
    w->arg = arg;
    w->background = background;

    UBaseType_t words = (UBaseType_t)((stack_bytes ? stack_bytes : 4096u) / sizeof(StackType_t));
    UBaseType_t prio = (UBaseType_t)(priority > 0 ? priority : (int)tskIDLE_PRIORITY + 1);

    if (!background) {
        w->done = xSemaphoreCreateBinary();
        if (!w->done) { free(w); return -1; }
    }

    TaskHandle_t handle = NULL;
    if (xTaskCreate(bl616_worker_trampoline, name ? name : "tdsh", words, w, prio, &handle) != pdPASS) {
        if (w->done) vSemaphoreDelete(w->done);
        free(w);
        return -1;
    }

    if (background) return 0;

    (void)xSemaphoreTake(w->done, portMAX_DELAY);
    int rc = w->result;
    if (result_out) *result_out = rc;
    vTaskDelete(handle);
    vSemaphoreDelete(w->done);
    free(w);
    return 0;
}

static const tdsh_platform_api_t s_platform = {
    .name = "bl616/freertos",
    .context = NULL,
    .monotonic_ms = bl616_monotonic_ms,
    .sleep_ms = bl616_sleep_ms,
    .yield = bl616_yield,
    .random_bytes = bl616_random_bytes,
    .malloc_fn = NULL,
    .calloc_fn = NULL,
    .realloc_fn = NULL,
    .free_fn = NULL,
    .worker_run = bl616_worker_run,
};

/* ----------------------------------------------------------------- console */

static int bl616_terminal_read_byte(void *context, uint8_t *byte_out)
{
    (void)context;
    for (;;) {
        int b = tdsh_bl616_console_read_byte();
        if (b >= 0) {
            *byte_out = (uint8_t)b;
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

static int bl616_terminal_write_bytes(void *context, const void *data, size_t length)
{
    (void)context;
    return tdsh_bl616_console_write(data, length);
}

/* ------------------------------------------------------------------- shell */

static tdsh_session_t s_session;
static char s_hostname[TDSH_HOSTNAME_MAX] = "tinytang";

/* Our own console printf (ports/bl616/tdsh_console_stdio_bl616.c); the shell
 * core reaches it through the redirect header, but this file uses it by name. */
int tdsh_printf(const char *fmt, ...);

/* Tang-specific commands.  This is where the FPGA loader and the peek/poke
 * commands will live; for now it reports the state that is still being brought
 * up, so it can be asked without a reflash. */
/* Which form does this FatFS port accept for the card's root?  Probed rather
 * than assumed: the shell's root maps to cfg.fs_root verbatim, and getting the
 * form wrong makes `ls /` fail while everything below it works. */
static void probe_root(const char *p)
{
    DIR *d = opendir(p);
    struct stat st;
    const int rc = stat(p, &st);
    tdsh_printf("  %-8s opendir=%s  stat=%d(%s)\r\n", p,
                d ? "OK" : "null", rc,
                rc == 0 ? (S_ISDIR(st.st_mode) ? "dir" : "file") : "fail");
    if (d) {
        closedir(d);
    }
}

static int cmd_tang(tdsh_session_t *session, int argc, char **argv)
{
    (void)session; (void)argc; (void)argv;
    tdsh_printf("tinytang: platform=bl616/freertos  sd FRESULT=%d  mounted=%d\r\n",
                tdsh_bl616_fs_last_result(), tdsh_bl616_fs_ready() ? 1 : 0);
    probe_root("/sd");
    probe_root("/sd/");
    probe_root("sd:");
    probe_root("sd:/");
    probe_root("/");
    return 0;
}

static int cmd_tang_mount(tdsh_session_t *session, int argc, char **argv)
{
    (void)session; (void)argc; (void)argv;
    int rc = tdsh_bl616_fs_mount();
    tdsh_printf("tinytang: mount -> %d (FRESULT %d)\r\n", rc, tdsh_bl616_fs_last_result());
    return 0;
}

static const tdsh_command_t s_tang_commands[] = {
    { "tang",      "tang",      "Show TinyTang platform and SD status", cmd_tang,       0 },
    { "tangmount", "tangmount", "Retry the SD card mount",              cmd_tang_mount, 0 },
};

int tdsh_bl616_init(const char *hostname)
{
    if (hostname && hostname[0]) {
        snprintf(s_hostname, sizeof(s_hostname), "%s", hostname);
    }

    tdsh_core_config_t core = TDSH_CORE_CONFIG_DEFAULT();
    core.hostname = s_hostname;
    core.default_user = "root";
    /* The SD card's volume string is the shell's root.  The trailing slash
     * matters: the core maps a logical "/" to fs_root verbatim, and FatFS
     * rejects "/sd" on its own, so without it `ls /` fails while /sd/cores
     * and everything below it works.  With the slash, "/" is "/sd/" and
     * "/cores" is still "/sd/cores". */
    core.fs_root = "/sd/";
    core.history_length = 32;
    core.platform = &s_platform;
    core.path_translate = NULL;
    core.path_translate_context = NULL;

    int rc = tdsh_core_init(&core);
    if (rc) return rc;
    rc = tdsh_register_core_builtins();
    if (rc) return rc;

    rc = tdsh_register_commands(s_tang_commands,
                                sizeof(s_tang_commands) / sizeof(s_tang_commands[0]));
    if (rc) return rc;

    extern int tdsh_bl616_tang_flash_register(void);
    rc = tdsh_bl616_tang_flash_register();
    if (rc) return rc;

    rc = tdsh_bl616_fpga_register();
    if (rc) return rc;

    rc = tdsh_session_init(&s_session, "root", true);
    if (rc) return rc;
    s_session.terminal_caps = TDSH_TERM_CAP_ANSI | TDSH_TERM_CAP_COLOR;
    return 0;
}

static void build_prompt(char *out, size_t cap)
{
    const char marker = strcmp(s_session.username, "root") == 0 ? '#' : '$';
    char display[TDSH_MAX_PATH];
    if (strcmp(s_session.cwd, s_session.home) == 0) {
        snprintf(display, sizeof(display), "~");
    } else if (strncmp(s_session.cwd, s_session.home, strlen(s_session.home)) == 0 &&
               s_session.cwd[strlen(s_session.home)] == '/') {
        snprintf(display, sizeof(display), "~%s", s_session.cwd + strlen(s_session.home));
    } else {
        snprintf(display, sizeof(display), "%s", s_session.cwd);
    }
    snprintf(out, cap, "\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m%c ",
             s_session.username, s_session.hostname, display, marker);
}

static int bl616_readline(const char *prompt, char *line, size_t capacity)
{
    const tdsh_terminal_io_t io = {
        .context = NULL,
        .read_byte = bl616_terminal_read_byte,
        .write_bytes = bl616_terminal_write_bytes,
    };
    return tdsh_terminal_readline(&s_session, &io, prompt, line, capacity);
}

int tdsh_bl616_run(void)
{
    static char line[TDSH_MAX_LINE + 2];
    static char prompt[TDSH_MAX_PATH + TDSH_HOSTNAME_MAX + TDSH_USERNAME_MAX + 64];

    static const char banner[] =
        "\r\n\033[1;36mTinyTang\033[0m — TinyDesk Shell " TDSH_VERSION "\r\n"
        "A Tang core booted from the board. Type 'help' for commands.\r\n\r\n";
    tdsh_bl616_console_write(banner, sizeof(banner) - 1);

    for (;;) {
        build_prompt(prompt, sizeof(prompt));
        int n = bl616_readline(prompt, line, sizeof(line));
        if (n < 0) continue;
        if (line[0]) {
            (void)tdsh_execute_line(&s_session, line);
        }
    }
    return 0;
}
