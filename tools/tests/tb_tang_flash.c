/* SPDX-License-Identifier: MIT
 * tangput, tangget and tangload (ports/bl616/tdsh_tang_flash.c) with two shells
 * able to run at once: tangput is refused from a shell with its own terminal
 * route before it reads a byte of the USB console, a second tangload is
 * refused while the first is programming without touching the cores' hooks,
 * and tangget's range arithmetic, seek and raw window are driven against the
 * card's stub.  The real file is included; the programmer, the card and the
 * console are fakes.
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

/* The card's file, for tangget: a byte image whose contents move with the
 * offset, so a misplaced seek or a repeated chunk cannot compare equal. */
static uint8_t g_file[0x2500];      /* 9,472 B: two full 4 KiB chunks and a short one */
static size_t g_file_size = sizeof(g_file);
static size_t g_file_pos;
static size_t g_read_limit;         /* 0 reads to the end; else f_read stops there */
static FSIZE_t g_seek = (FSIZE_t)-1;   /* where tangget seeked, or -1 for never */
static int g_seeks, g_open_fail, g_bad_path;
static FSIZE_t g_seek_fail_at = (FSIZE_t)-1;

/* What tangget put on the wire, and when the console was raw: '[' opened the
 * raw window, ']' closed it, 'X' is a raw write outside one, and 'T' is text
 * printed while it was open, which no header may be. */
static uint8_t g_raw[0x4000];
static size_t g_raw_len;
static char g_raw_trace[16];
static bool g_raw_on;

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
    if (g_raw_on) strncat(g_raw_trace, "T", sizeof(g_raw_trace) - strlen(g_raw_trace) - 1);
    pthread_mutex_unlock(&g_lock);
    return n;
}
tdsh_bl616_route_t *tdsh_bl616_route_current(void) { return g_route; }
int tdsh_bl616_console_read_byte(void)
{
    g_console_reads++;
    return *g_console ? (uint8_t)*g_console++ : -1;
}
void tdsh_bl616_console_set_raw(bool raw)
{
    g_raw_on = raw;
    strncat(g_raw_trace, raw ? "[" : "]",
            sizeof(g_raw_trace) - strlen(g_raw_trace) - 1);
}
bool tdsh_bl616_console_write_raw(const void *data, size_t length)
{
    if (!g_raw_on) {
        strncat(g_raw_trace, "X", sizeof(g_raw_trace) - strlen(g_raw_trace) - 1);
    }
    if (g_raw_len + length <= sizeof(g_raw)) {
        memcpy(g_raw + g_raw_len, data, length);
        g_raw_len += length;
        return true;
    }
    return false;
}
int tdsh_path_to_real(tdsh_session_t *session, const char *in, char *out, size_t out_size,
                      char *logical, size_t logical_size)
{
    (void)session; (void)logical; (void)logical_size;
    if (g_bad_path) return -1;
    snprintf(out, out_size, "/sd%s", in);
    return 0;
}
int tdsh_register_commands(const tdsh_command_t *commands, size_t count)
{
    (void)commands; (void)count;
    return 0;
}
FRESULT f_open(FIL *fp, const char *path, int mode)
{
    (void)fp; (void)path; (void)mode;
    g_opens++;
    if (g_open_fail) return FR_DISK_ERR;
    g_file_pos = 0;
    return FR_OK;
}
FRESULT f_read(FIL *fp, void *buf, UINT n, UINT *got)
{
    (void)fp;
    size_t avail = g_file_size - g_file_pos;
    if (g_read_limit && g_file_pos >= g_read_limit) {
        avail = 0;
    } else if (g_read_limit && avail > g_read_limit - g_file_pos) {
        avail = g_read_limit - g_file_pos;
    }
    if (avail > n) avail = n;
    memcpy(buf, g_file + g_file_pos, avail);
    g_file_pos += avail;
    *got = (UINT)avail;
    return FR_OK;
}
FRESULT f_write(FIL *fp, const void *buf, UINT n, UINT *wrote)
{
    (void)fp;
    memcpy(g_written + g_written_len, buf, n);
    g_written_len += n;
    *wrote = n;
    return FR_OK;
}
FRESULT f_close(FIL *fp) { (void)fp; return FR_OK; }
/* tangget asks a file's size and seeks into it.  The seek is recorded so the
 * test can see where the range read must have started. */
FSIZE_t f_size(FIL *fp) { (void)fp; return g_file_size; }
FRESULT f_lseek(FIL *fp, FSIZE_t ofs)
{
    (void)fp;
    if (ofs == g_seek_fail_at) return FR_DISK_ERR;
    g_seek = ofs;
    g_seeks++;
    g_file_pos = ofs;
    return FR_OK;
}
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
static bool g_i2c_bus = true;   /* the desktop core holds the bus before a load */
void tang_ini_core_replacing(void) { g_i2c_bus = false; hook("ini-replacing"); }
/* A select needs the bus the desktop core is still driving: if this hook runs
 * after the bus was given up, it does nothing and the game core's pad is dead. */
void tang_i2c_mux_for_game_core(void)
{
    hook(g_i2c_bus ? "mux-select" : "mux-select-NOBUS");
}
void tang_osd_desk_core_reloaded(void) { hook("osd-reloaded"); }
void tang_ini_core_loaded(void) { g_i2c_bus = true; hook("ini-loaded"); }

static void clear(void)
{
    g_out[0] = g_hooks[0] = g_raw_trace[0] = '\0';
    g_raw_len = 0;
    g_raw_on = false;
    g_seek = (FSIZE_t)-1;
    g_seeks = 0;
}
static int tangload(void)
{
    char *argv[] = {"tangload", "/root/cores/console138k/tinytang.bin"};
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
    tdsh_bl616_route_t other = {0};
    g_route = &other;
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

    /* --------------------------------------------------------- tangget */
    for (size_t i = 0; i < sizeof(g_file); i++) {
        g_file[i] = (uint8_t)((i * 7u) ^ (i >> 3));
    }
    char want[96];
    char *get_all[] = {"tangget", "/root/save.bin"};

    /* The whole file: three chunks, no seek, and one raw window that closes. */
    clear();
    CHECK(cmd_tangget(NULL, 2, get_all) == 0);
    CHECK(g_seeks == 0);
    snprintf(want, sizeof(want), "tangget: %zu bytes at 0 of %zu\r\n",
             sizeof(g_file), sizeof(g_file));
    CHECK(strstr(g_out, want) != NULL);
    CHECK(strstr(g_out, "tangget: sent 9472 of 9472 bytes\r\n") != NULL);
    CHECK(g_raw_len == sizeof(g_file) && memcmp(g_raw, g_file, g_raw_len) == 0);
    CHECK(strcmp(g_raw_trace, "[]") == 0);

    /* A window inside the file: the header names the range, the seek lands on
     * it and only those bytes go out. */
    char *get_range[] = {"tangget", "/root/save.bin", "4096", "100"};
    clear();
    CHECK(cmd_tangget(NULL, 4, get_range) == 0);
    CHECK(g_seeks == 1 && g_seek == 4096);
    CHECK(strstr(g_out, "tangget: 100 bytes at 4096 of 9472\r\n") != NULL);
    CHECK(g_raw_len == 100 && memcmp(g_raw, g_file + 4096, 100) == 0);
    CHECK(strcmp(g_raw_trace, "[]") == 0);

    /* An offset with no length is the rest of the file, across a chunk edge. */
    char *get_tail[] = {"tangget", "/root/save.bin", "4090"};
    clear();
    CHECK(cmd_tangget(NULL, 3, get_tail) == 0);
    CHECK(g_seek == 4090 && g_raw_len == sizeof(g_file) - 4090);
    CHECK(memcmp(g_raw, g_file + 4090, g_raw_len) == 0);

    /* A length that runs past the end -- here 9,000 bytes from offset 9,000 of
     * a 9,472-byte file -- is shortened to what is left, not refused. */
    char *get_past[] = {"tangget", "/root/save.bin", "9000", "9000"};
    clear();
    CHECK(cmd_tangget(NULL, 4, get_past) == 0);
    CHECK(g_seek == 9000 && g_raw_len == sizeof(g_file) - 9000);
    CHECK(strstr(g_out, "tangget: 472 bytes at 9000 of 9472\r\n") != NULL);
    CHECK(strstr(g_out, "tangget: sent 472 of 472 bytes\r\n") != NULL);
    CHECK(memcmp(g_raw, g_file + 9000, g_raw_len) == 0);

    /* An offset past the end clamps to the end: nothing to send, and no
     * refusal, because a host that asked for too much still gets its answer. */
    char *get_after[] = {"tangget", "/root/save.bin", "999999"};
    clear();
    CHECK(cmd_tangget(NULL, 3, get_after) == 0);
    CHECK(g_seek == sizeof(g_file) && g_raw_len == 0);
    CHECK(strstr(g_out, "tangget: 0 bytes at 9472 of 9472\r\n") != NULL);
    CHECK(strstr(g_out, "tangget: sent 0 of 0 bytes\r\n") != NULL);

    /* A seek the card refuses is reported, and nothing is put on the wire. */
    g_seek_fail_at = 500;
    char *get_noseek[] = {"tangget", "/root/save.bin", "500", "10"};
    clear();
    CHECK(cmd_tangget(NULL, 4, get_noseek) == 1);
    CHECK(strstr(g_out, "tangget: cannot seek to 500\r\n") != NULL);
    CHECK(g_raw_len == 0 && g_raw_trace[0] == '\0');
    g_seek_fail_at = (FSIZE_t)-1;

    /* A read that stops early ends the transfer short and fails, and the raw
     * window is still closed before the message that says so. */
    g_read_limit = 4096;
    clear();
    CHECK(cmd_tangget(NULL, 2, get_all) == 1);
    CHECK(strstr(g_out, "tangget: sent 4096 of 9472 bytes\r\n") != NULL);
    CHECK(g_raw_len == 4096 && memcmp(g_raw, g_file, 4096) == 0);
    CHECK(strcmp(g_raw_trace, "[]") == 0);
    g_read_limit = 0;

    /* From a shell with its own route: refused before the card is touched. */
    int opens_before = g_opens;
    g_route = &other;
    clear();
    CHECK(cmd_tangget(NULL, 2, get_all) == 1);
    CHECK(strstr(g_out, "tangget: run it from the USB console\r\n") != NULL);
    CHECK(g_opens == opens_before && g_raw_len == 0);
    g_route = NULL;

    /* The card and the shell's own path check, each named in its message. */
    g_open_fail = 1;
    clear();
    CHECK(cmd_tangget(NULL, 2, get_all) == 1);
    CHECK(strstr(g_out, "tangget: cannot open /root/save.bin\r\n") != NULL);
    g_open_fail = 0;

    g_bad_path = 1;
    clear();
    CHECK(cmd_tangget(NULL, 2, get_all) == 1);
    CHECK(strstr(g_out, "tangget: bad path /root/save.bin\r\n") != NULL);
    g_bad_path = 0;

    char *get_none[] = {"tangget"};
    clear();
    CHECK(cmd_tangget(NULL, 1, get_none) == 1);
    CHECK(strstr(g_out, "usage: tangget <path> [offset [length]]\r\n") != NULL);

    /* One load: the hooks in order around the programmer. */
    g_release = true;
    clear();
    CHECK(tangload() == 0);
    CHECK(strcmp(g_hooks, "mux-select phos-replacing ini-replacing program osd-reloaded ini-loaded ") == 0);

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
    CHECK(strcmp(g_hooks, "mux-select phos-replacing ini-replacing program ") == 0);
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
    CHECK(strcmp(g_hooks, "mux-select phos-replacing ini-replacing program ") == 0);
    g_program_ok = true;
    clear();
    CHECK(tangload() == 0);
    CHECK(strstr(g_hooks, "ini-loaded") != NULL);

    if (failures) {
        printf("test_tang_flash: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_tang_flash: all checks passed\n");
    return 0;
}
