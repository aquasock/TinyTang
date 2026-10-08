/* SPDX-License-Identifier: MIT
 * tangput and tangload (ports/bl616/tdsh_tang_flash.c) with two shells able to
 * run at once: tangput is refused from a shell with its own terminal route
 * before it reads a byte of the USB console, and a second tangload is refused
 * while the first is programming, without touching the cores' hooks.  The
 * real file is included; the programmer, the card and the console are fakes.
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../ports/bl616/tdsh_tang_flash.c"

static int failures;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            failures++;                                                   \
        }                                                                 \
    } while (0)

/* --------------------------------------------------------------- fakes */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_out[4096];
static char g_hooks[256];
static tdsh_bl616_route_t *g_route;
static int g_console_reads, g_opens;
static const char *g_console = "";
static char g_written[64];
static size_t g_written_len;

static void hook(const char *name)
{
    pthread_mutex_lock(&g_lock);
    strcat(g_hooks, name);
    strcat(g_hooks, " ");
    pthread_mutex_unlock(&g_lock);
}

int tdsh_printf(const char *fmt, ...)
{
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    pthread_mutex_lock(&g_lock);
    strncat(g_out, line, sizeof(g_out) - strlen(g_out) - 1);
    pthread_mutex_unlock(&g_lock);
    return n;
}
tdsh_bl616_route_t *tdsh_bl616_route_current(void) { return g_route; }
int tdsh_bl616_console_read_byte(void)
{
    g_console_reads++;
    return *g_console ? (uint8_t)*g_console++ : -1;
}
void tdsh_bl616_console_set_raw(bool raw) { (void)raw; }
int tdsh_path_to_real(tdsh_session_t *session, const char *in, char *out, size_t out_size,
                      char *logical, size_t logical_size)
{
    (void)session; (void)logical; (void)logical_size;
    snprintf(out, out_size, "/sd%s", in);
    return 0;
}
int tdsh_register_commands(const tdsh_command_t *commands, size_t count)
{
    (void)commands; (void)count;
    return 0;
}
FRESULT f_open(FIL *fp, const char *path, int mode) { (void)fp; (void)path; (void)mode; g_opens++; return FR_OK; }
FRESULT f_read(FIL *fp, void *buf, UINT n, UINT *got) { (void)fp; (void)buf; (void)n; *got = 0; return FR_OK; }
FRESULT f_write(FIL *fp, const void *buf, UINT n, UINT *wrote)
{
    (void)fp;
    memcpy(g_written + g_written_len, buf, n);
    g_written_len += n;
    *wrote = n;
    return FR_OK;
}
FRESULT f_close(FIL *fp) { (void)fp; return FR_OK; }
const char *tang_fw_update_open(tang_fw_update_t *u, const char *real_path) { (void)u; (void)real_path; return "not in this test"; }
const char *tang_fw_update_step(tang_fw_update_t *u, bool *done) { (void)u; *done = true; return "not in this test"; }
void tang_fw_update_commit(tang_fw_update_t *u) { (void)u; abort(); }

/* The programmer holds until released, so a second load can arrive mid-way. */
static pthread_mutex_t g_prog_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_prog_cond = PTHREAD_COND_INITIALIZER;
static bool g_programming, g_release, g_program_ok = true;
bool fpga_program(const char *fname)
{
    (void)fname;
    hook("program");
    pthread_mutex_lock(&g_prog_lock);
    g_programming = true;
    pthread_cond_broadcast(&g_prog_cond);
    while (!g_release) pthread_cond_wait(&g_prog_cond, &g_prog_lock);
    g_programming = false;
    pthread_mutex_unlock(&g_prog_lock);
    return g_program_ok;
}
void tang_phosphor_core_replacing(void) { hook("phos-replacing"); }
void tang_oled_core_replacing(void) { hook("oled-replacing"); }
void tang_osd_desk_core_reloaded(void) { hook("osd-reloaded"); }
void tang_ini_core_loaded(void) { hook("ini-loaded"); }
void tang_oled_core_loaded(void) { hook("oled-loaded"); }

static void clear(void)
{
    g_out[0] = g_hooks[0] = '\0';
}
static int tangload(void)
{
    char *argv[] = {"tangload", "/cores/console138k/desktop.bin"};
    return cmd_tangload(NULL, 2, argv);
}
static int g_first_rc = -1;
static void *first_load(void *arg)
{
    (void)arg;
    g_first_rc = tangload();
    return NULL;
}

int main(void)
{
    /* tangput from a shell with its own route: refused before any read. */
    tdsh_bl616_route_t oled = {0};
    g_route = &oled;
    g_console = "abc";
    char *put[] = {"tangput", "3", "/x.bin"};
    clear();
    CHECK(cmd_tangput(NULL, 3, put) == 1);
    CHECK(g_console_reads == 0 && g_opens == 0);
    CHECK(strstr(g_out, "USB console") != NULL);

    /* From the console it still works. */
    g_route = NULL;
    clear();
    CHECK(cmd_tangput(NULL, 3, put) == 0);
    CHECK(g_written_len == 3 && memcmp(g_written, "abc", 3) == 0);

    /* One load: the hooks in order around the programmer. */
    g_release = true;
    clear();
    CHECK(tangload() == 0);
    CHECK(strcmp(g_hooks, "phos-replacing oled-replacing program osd-reloaded ini-loaded oled-loaded ") == 0);

    /* A second load while the first programs is refused and touches nothing. */
    g_release = false;
    clear();
    pthread_t first;
    pthread_create(&first, NULL, first_load, NULL);
    pthread_mutex_lock(&g_prog_lock);
    while (!g_programming) pthread_cond_wait(&g_prog_cond, &g_prog_lock);
    pthread_mutex_unlock(&g_prog_lock);
    CHECK(tangload() == 1);
    CHECK(strstr(g_out, "another tangload is running") != NULL);
    CHECK(strcmp(g_hooks, "phos-replacing oled-replacing program ") == 0);
    pthread_mutex_lock(&g_prog_lock);
    g_release = true;
    pthread_cond_broadcast(&g_prog_cond);
    pthread_mutex_unlock(&g_prog_lock);
    pthread_join(first, NULL);
    CHECK(g_first_rc == 0);

    /* A failed load releases the programmer too, and the next one runs. */
    g_program_ok = false;
    clear();
    CHECK(tangload() == 1);
    CHECK(strcmp(g_hooks, "phos-replacing oled-replacing program ") == 0);
    g_program_ok = true;
    clear();
    CHECK(tangload() == 0);
    CHECK(strstr(g_hooks, "oled-loaded") != NULL);

    if (failures) {
        printf("test_tang_flash: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_tang_flash: all checks passed\n");
    return 0;
}
