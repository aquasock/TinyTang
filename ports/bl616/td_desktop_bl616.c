// TinyTang — TinyDesk, the desktop environment, on the BL616.
//
// TinyDesk draws overlapping, draggable text-mode windows with ANSI escape
// sequences and reads the keyboard and mouse back from the terminal.  There is
// no display hardware, which is exactly what makes it fit here: the board
// renders the desktop and streams it down the same USB CDC console the shell
// uses, and the computer on the other end is only a terminal.
//
// The port surface is td_hal_t -- four functions.  Three of them are console
// calls this port already has, and the fourth is the board's millisecond
// clock, so most of this file is a table of pointers plus the sysinfo the
// desktop asks for.
//
// The filesystem is not ported at all.  TinyDesk's Files and Editor apps are
// written against stdio, <dirent.h> and <sys/stat.h>, and ports/bl616 already
// implements exactly those over FatFS for the shell, so ports/common/
// td_fs_stdio.c compiles against that shim unchanged.  The ESP32-C6 port does
// the same thing through its VFS.
//
// Three things are deliberately absent for now:
//
//   * There is no clock.  The board has no RTC and nothing sets the time, so
//     time_now() reports "not set" rather than inventing a date.
//   * The network apps (Network, MQTT, Modbus, OTA) are left out, because they
//     are built on POSIX sockets and mbedTLS; see td_apps_register_all() below.
//   * There is one user and one session.  The Terminal window shares both with
//     the console shell instead of starting its own; see td_bridge_bl616.c.
//
// The Terminal window is filled by td_bridge_bl616.c, which runs the shell
// against the ring buffers the window reads and writes.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "bflb_mtimer.h"
#include "bflb_clock.h"
#include "mem.h"          /* the SDK's heap: g_kmemheap, kfree_size() */

#include "tinydesk/td.h"
#include "td_apps.h"
#include "td_fs_stdio.h"
#include "tinydesk/td_sysinfo.h"

#include "tdsh.h"
#include "tdsh_bl616.h"
#include "tang_osd_desk.h"

int tdsh_printf(const char *fmt, ...);

/* The shell bridge (td_bridge_bl616.c): what fills TinyDesk's Terminal. */
const td_term_backend_t *td_bridge_bl616_backend(void);
void td_bridge_bl616_stop(void);
void td_phosphor_register(void);   /* phosphor/td_phosphor_app.cpp */

/* The desktop's root is the card -- the same filesystem the shell uses, so
 * both see the same files. */
#define TD_DESKTOP_ROOT  "/sd"
#define TD_SETTINGS_PATH TD_DESKTOP_ROOT "/tinydesk_settings.bin"
#define TD_TZ_PATH       TD_DESKTOP_ROOT "/tinydesk_tz"

#define TD_PLATFORM_NAME "Tang Console (BL616)"

/* ---------------------------------------------------------------- the HAL */

static int hal_read_byte(void *ctx)
{
    (void)ctx;
    /* The desktop layer answers first when it is drawing the screen.  It is a
     * terminal emulator sized to the OSD, so it is what tells the desktop how
     * big the desktop is -- by answering the size query with its own grid.
     * Letting the USB console answer instead would size the desktop to whatever
     * window happens to be open on someone's PC, or to nothing at all when no
     * PC is attached.
     *
     * With a PC terminal attached, both the layer and the terminal can answer
     * the query, and the desktop takes any cursor position report as a resize
     * (`input.c`, final == 'R') -- so it would flip between 80x45 and the
     * terminal's size once a second.  That is handled on the way out: the
     * console writer keeps the query off the wire while the layer owns the
     * display (see usb_cdc_bl616.c), so the layer is the only answer. */
    const int desk = tang_osd_desk_read_byte();
    if (desk >= 0) {
        return desk;
    }
    /* The console read is already non-blocking and returns -1 when empty,
     * which is exactly the contract the desktop asks for. */
    return tdsh_bl616_console_read_byte();
}

static int hal_write(void *ctx, const uint8_t *buf, int len)
{
    (void)ctx;
    if (len <= 0) {
        return 0;
    }
    /* With the desk layer up, HDMI is the display: the desktop's drawing goes
     * to the layer's mirror alone and not to the USB console, which keeps the
     * shell, command output and transfers but no longer floods a computer
     * terminal with window redraws.  The layer answers the desktop's size
     * query, as it did when both saw it.  With the layer off the desktop is
     * being viewed from a computer terminal and draws to the console as before. */
    if (tang_osd_desk_enabled()) {
        tang_osd_desk_feed(buf, (size_t)len);
        return len;
    }
    return tdsh_bl616_console_write(buf, (size_t)len);
}

static uint32_t hal_millis(void *ctx)
{
    (void)ctx;
    return (uint32_t)bflb_mtimer_get_time_ms();
}

static void hal_sleep_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1));
}

static const td_hal_t s_hal = {
    .read_byte = hal_read_byte,
    .write = hal_write,
    .millis = hal_millis,
    .sleep_ms = hal_sleep_ms,
    .ctx = NULL,
};

/* ------------------------------------------------------- sysinfo plumbing */

static bool settings_load(void *data, int len)
{
    FILE *f = fopen(TD_SETTINGS_PATH, "rb");
    if (!f) {
        return false;       /* first boot: the desktop uses its defaults */
    }
    const bool ok = fread(data, 1, (size_t)len, f) == (size_t)len;
    fclose(f);
    return ok;
}

static bool settings_save(const void *data, int len)
{
    FILE *f = fopen(TD_SETTINGS_PATH, "wb");
    if (!f) {
        return false;
    }
    const bool ok = fwrite(data, 1, (size_t)len, f) == (size_t)len;
    fclose(f);
    return ok;
}

/* No RTC and nothing here sets the clock, so say so rather than inventing a
 * date that the Files app would then stamp on everything. */
static bool time_now(int64_t *utc)
{
    (void)utc;
    return false;
}

static bool get_tz(long *seconds)
{
    *seconds = 0;
    FILE *f = fopen(TD_TZ_PATH, "r");
    if (f) {
        long v = 0;
        if (fscanf(f, "%ld", &v) == 1) {
            *seconds = v;
        }
        fclose(f);
    }
    return true;
}

static bool set_tz(long seconds)
{
    FILE *f = fopen(TD_TZ_PATH, "w");
    if (!f) {
        return false;
    }
    fprintf(f, "%ld\n", seconds);
    return fclose(f) == 0;
}

static td_sysinfo_t s_info;

/* The app set this board can run.
 *
 * TinyDesk's own td_apps_register_all() also brings up Network, MQTT, Modbus
 * and the OTA updater, and those sit on proto/td_sock.c and proto/td_tls.c:
 * POSIX sockets, esp_timer, and mbedTLS with esp_crt_bundle.  None of that
 * exists on the BL616, and nothing on this desktop needs the network, so the
 * local apps are registered here instead. */
void td_apps_register_all(void)
{
    td_terminal_register();
    td_files_register();
    td_editor_register();
    td_sysmon_register();
    td_taskmgr_register();
    td_logview_register();
    td_settings_register();
    td_counter_register();
    td_phosphor_register();
    td_about_register();
    td_datetime_install_clock();
    td_session_init();
}

/* ----------------------------------- what System Monitor and Task Manager read */
//
// Both apps were blank because every member they read was left NULL.  None of
// it needs new machinery: the allocator the SDK actually uses, and FreeRTOS,
// already know both things.
//
// The heap is TLSF, not FreeRTOS's -- so xPortGetFreeHeapSize() does not even
// exist in this build, which the linker was quick to point out.  The SDK's own
// mem.h keeps the running free figure and the usable size, updating the former
// on every allocation and free.  It keeps no low-water mark, so this tracks
// one across its own samples: a minimum of the readings, not of every
// allocation.
//
// There is one heap.  mem.h maps PMEM_HEAP to the same g_kmemheap unless the
// chip is a BL618, so this board has no external RAM and the app is told so
// with NULL rather than a zero.

static uint32_t s_heap_min;

static uint32_t heap_free(void)
{
    return kfree_size();
}

static uint32_t heap_min_free(void)
{
    const uint32_t now = kfree_size();
    if (s_heap_min == 0 || now < s_heap_min) {
        s_heap_min = now;
    }
    return s_heap_min;
}

static uint32_t heap_total(void)
{
    return (uint32_t)g_kmemheap.heapsize;
}

static int sys_task_count(void)     { return (int)uxTaskGetNumberOfTasks(); }

static int sys_cpu_mhz(void)
{
    return (int)(bflb_clk_get_system_clock(BFLB_SYSTEM_CPU_CLK) / 1000000u);
}

static char task_state_char(eTaskState state)
{
    switch (state) {
    case eRunning:   return 'R';
    case eReady:     return 'r';
    case eBlocked:   return 'B';
    case eSuspended: return 'S';
    case eDeleted:   return 'D';
    default:         return '?';
    }
}

/* The task list comes from uxTaskGetSystemState(), which exists because
 * configUSE_TRACE_FACILITY is 1.  The CPU share cannot: it needs
 * configGENERATE_RUN_TIME_STATS, which is 0, and the struct documents -1 as
 * "unknown" for exactly this case.  The stack figure is the high-water mark,
 * which is a count of stack entries, not bytes. */
static int sys_tasks(td_task_info_t *out, int max)
{
    static TaskStatus_t status[24];
    const UBaseType_t room = (UBaseType_t)(sizeof(status) / sizeof(status[0]));

    if (!out || max <= 0) {
        return -1;
    }
    const UBaseType_t want = (max < (int)room) ? (UBaseType_t)max : room;
    const UBaseType_t count = uxTaskGetSystemState(status, want, NULL);

    for (UBaseType_t i = 0; i < count; i++) {
        snprintf(out[i].name, sizeof(out[i].name), "%s",
                 status[i].pcTaskName ? status[i].pcTaskName : "?");
        out[i].state = task_state_char(status[i].eCurrentState);
        out[i].priority = (uint8_t)status[i].uxCurrentPriority;
        out[i].core = 0;                    /* one core */
        out[i].stack_free = (uint32_t)status[i].usStackHighWaterMark * sizeof(StackType_t);
        out[i].cpu_tenths = -1;             /* no run-time statistics */
    }
    return (int)count;
}

/* -------------------------------------------------------------- the entry */

/* Runs the desktop until it quits.  Called from the shell command, so the
 * shell task parks here for the duration and gets the prompt back after. */
static void desktop_run(void)
{
    static char platform[48];

    snprintf(platform, sizeof(platform), "%s", TD_PLATFORM_NAME);

    memset(&s_info, 0, sizeof(s_info));
    s_info.platform = platform;
    s_info.chip = "BL616";
    s_info.sdk_version = "bouffalo_sdk";
    s_info.settings_load = settings_load;
    s_info.settings_save = settings_save;
    s_info.fs = td_fs_stdio(TD_DESKTOP_ROOT);
    s_info.time_now = time_now;
    s_info.get_tz = get_tz;
    s_info.set_tz = set_tz;

    /* What System Monitor and Task Manager read.  Without these both apps
     * draw empty frames. */
    s_info.free_heap = heap_free;
    s_info.min_free_heap = heap_min_free;
    s_info.total_heap = heap_total;
    s_info.task_count = sys_task_count;
    s_info.cpu_mhz = sys_cpu_mhz;
    s_info.tasks = sys_tasks;

    s_info.extra = "Shell:     TinyDesk Shell " TDSH_VERSION;
    td_set_sysinfo(&s_info);

    td_init(&s_hal);
    td_logf('I', "TinyDesk %s on %s", TD_VERSION, TD_PLATFORM_NAME);
    td_logf('I', "terminal size %dx%d", td_stats()->cols, td_stats()->rows);
    td_logf('I', "files root %s", TD_DESKTOP_ROOT);

    /* The Terminal app has no backend of its own: without this it draws
     * "No shell backend in this build." and nothing else. */
    td_terminal_set_backend(td_bridge_bl616_backend());

    /* Everything else -- Files, Editor, System Monitor, Settings, Log, About
     * -- works against the card. */
    td_apps_register_all();

    td_run();               /* returns after td_quit() */
    td_shutdown();

    /* The window is gone but the shell task behind it is not: end it before
     * the redirect is cleared, or it would read the console alongside the
     * outer shell. */
    td_bridge_bl616_stop();
}

static int cmd_desktop(tdsh_session_t *session, int argc, char **argv)
{
    (void)session; (void)argc; (void)argv;

    /* The desktop takes the console: it reads raw keys and writes ANSI.  Say
     * this first, then let it reach the host before the screen is redrawn. */
    tdsh_printf("desktop: starting TinyDesk. F10 for the start menu, F6 switches\r\n");
    tdsh_printf("windows, F11 full screen. Quitting returns to this prompt.\r\n");
    vTaskDelay(pdMS_TO_TICKS(200));

    /* TinyTang: the layer is this command's to enable, not the boot script's.
     * The desktop is the only thing that draws into the wide grid, so enabling
     * it here is the only way to be sure that grid is never shown enabled and
     * empty -- which is the same uninitialised-store problem osd clear fixes
     * for the legacy page, one layer up.  boot.tdsh no longer sends it. */
    if (tang_osd_desk_set(true) != 0) {
        tdsh_printf("desktop: the desktop layer did not start; not bringing it up\r\n");
        return 1;
    }

    /* The pointer is the desktop's, and only for as long as it runs. */
    tang_osd_desk_set_pointer(true);
    tdsh_bl616_console_set_desktop(true);
    desktop_run();
    tdsh_bl616_console_set_desktop(false);
    tang_osd_desk_set_pointer(false);

    tdsh_printf("\r\ndesktop: exited\r\n");
    return 0;
}

static const tdsh_command_t s_desktop_commands[] = {
    { "desktop", "desktop", "Run TinyDesk, the desktop, on this console", cmd_desktop, 0 },
};

int tang_desktop_register(void)
{
    return tdsh_register_commands(s_desktop_commands,
                                  sizeof(s_desktop_commands) / sizeof(s_desktop_commands[0]));
}
