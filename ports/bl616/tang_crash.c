// TinyTang — the crash and hang recorder.  See tang_crash.h.
//
// SPDX-License-Identifier: MIT

#include "tang_crash.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bflb_flash.h"
#include "bflb_wdg.h"
#include "bl616_glb.h"
#include "ff.h"

#include "tdsh.h"
#include "tinytang_build_id.h"

int tdsh_printf(const char *fmt, ...);

#define CRASH_MAGIC        0x52435454u   /* "TTCR" */
#define CRASH_VERSION      1u
#define CRASH_TRAIL        32            /* samples kept */
#define CRASH_TRAIL_TICKS  4             /* one sample every 4 ms */
#define CRASH_WDT_MS       4000          /* the watchdog's timeout */
#define CRASH_FEED_MS      500
#define CRASH_STARVE_MS    30000         /* heartbeat silence that counts as a hang */
#define CRASH_LOG_PATH     "/sd/crash.log"

/* Where a record is kept across the reset.  A warm reset on this board -- the
 * watchdog's or a software one -- lands in the vendor loader, which starts the
 * FT2232 debugger instead of this firmware (FLS-001), so only a power cycle
 * brings it back, and that scrambles RAM.  So the record is written to the
 * last 4 KB sector of the 4 MiB flash before resetting: far above the
 * application slot, the staging area and the vendor data record at 0x200000,
 * in a region the full read-back of FLS-002 found erased. */
#define CRASH_FLASH_ADDR   0x003FF000u
#define CRASH_FLASH_SECTOR 0x1000u

enum { RUN_RUNNING = 1, RUN_CLEAN = 2, RUN_CRASHED = 3 };
enum {
    KIND_NONE, KIND_TRAP, KIND_STACK, KIND_MALLOC, KIND_STARVED, KIND_TEST_HANG,
    KIND_CONTROLLER, KIND_ASSERT, KIND_WDOG_STARVED,
};

static const char *const s_kind_names[] = {
    "", "an exception", "a stack overflow", "a failed malloc", "a task that never yielded",
    "a deliberate hang (crash test hang)", "a Bluetooth controller fatal error",
    "a FreeRTOS assert", "the watchdog task being starved",
};

/* The watchdog task's last run, checked from the tick hook. */
#define CRASH_WDOG_LATE_MS 2000
static volatile TickType_t s_wdog_seen;

typedef struct {
    uint32_t tick;
    uint32_t pc;
    char task[8];
} crumb_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t boots;          /* resets that kept RAM, since the last power-up */
    uint32_t state;
    uint32_t kind;
    uint32_t mcause, mepc, mtval, ra;
    uint32_t tick;           /* tick count when it was recorded */
    char task[16];
    char build[40];
    uint32_t sum;            /* over every field above */
} crash_rec_t;

/* What goes to flash: the record, then the trail. */
typedef struct {
    crash_rec_t rec;
    uint32_t head;
    crumb_t trail[CRASH_TRAIL];
} crash_flash_t;

static crash_flash_t s_flash_buf;
static bool s_last_from_flash;

/* Not cleared at startup: a watchdog or software reset leaves it as it was. */
static crash_rec_t s_rec __attribute__((section(".noinit_data")));
static crumb_t s_trail[CRASH_TRAIL] __attribute__((section(".noinit_data")));
static uint32_t s_trail_head __attribute__((section(".noinit_data")));

/* The previous run's record and trail, copied aside at boot. */
static crash_rec_t s_last;
static crumb_t s_last_trail[CRASH_TRAIL];
static uint32_t s_last_head;
static bool s_last_valid;
static uint8_t s_reset_sts;

static struct bflb_device_s *s_wdg;
static volatile bool s_wdt_running;
static volatile bool s_stop_feeding;
static volatile TickType_t s_heartbeat;

extern void *volatile pxCurrentTCB;

static uint32_t rec_sum(const crash_rec_t *r)
{
    const uint8_t *p = (const uint8_t *)r;
    uint32_t s = 0x811C9DC5u;                 /* FNV-1a */
    for (size_t i = 0; i < offsetof(crash_rec_t, sum); i++) {
        s = (s ^ p[i]) * 0x01000193u;
    }
    return s;
}

static void rec_seal(void)
{
    s_rec.sum = rec_sum(&s_rec);
}

static void copy_name(char *dst, size_t n, const char *src)
{
    size_t i = 0;
    for (; src && i + 1 < n && src[i]; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = '\0';
    }
}

static uint32_t csr_mcause(void) { uint32_t v; __asm__ volatile("csrr %0, mcause" : "=r"(v)); return v; }
static uint32_t csr_mepc(void)   { uint32_t v; __asm__ volatile("csrr %0, mepc" : "=r"(v));   return v; }
static uint32_t csr_mtval(void)  { uint32_t v; __asm__ volatile("csrr %0, mtval" : "=r"(v));  return v; }

void tang_crash_init(void)
{
    GLB_RESET_RECORD_Type r;
    memset(&r, 0, sizeof(r));
    (void)GLB_Get_Reset_Reason(&r);
    s_reset_sts = (uint8_t)(r.reset_recorder_ana_powb | (r.reset_recorder_ext_rst_n << 1) |
                            (r.reset_recorder_pds_reset << 2) | (r.reset_recorder_wdt_rst_n << 3) |
                            (r.reset_recorder_cpu_porst_n << 4) |
                            (r.reset_recorder_sys_reset_n << 5) |
                            (r.reset_recorder_cpu_sys_rstreq_n << 6));

    uint32_t boots = 0;
    if (s_rec.magic == CRASH_MAGIC && s_rec.version == CRASH_VERSION &&
        s_rec.sum == rec_sum(&s_rec)) {
        s_last = s_rec;
        memcpy(s_last_trail, s_trail, sizeof(s_last_trail));
        s_last_head = s_trail_head;
        s_last_valid = true;
        boots = s_rec.boots + 1;
    }
    /* A record in flash is the one a crash wrote before its reset, and the
     * one that survives the power cycle that follows; it wins. */
    if (bflb_flash_read(CRASH_FLASH_ADDR, (uint8_t *)&s_flash_buf, sizeof(s_flash_buf)) == 0 &&
        s_flash_buf.rec.magic == CRASH_MAGIC && s_flash_buf.rec.version == CRASH_VERSION &&
        s_flash_buf.rec.sum == rec_sum(&s_flash_buf.rec)) {
        s_last = s_flash_buf.rec;
        memcpy(s_last_trail, s_flash_buf.trail, sizeof(s_last_trail));
        s_last_head = s_flash_buf.head;
        s_last_valid = true;
        s_last_from_flash = true;
    }

    memset(&s_rec, 0, sizeof(s_rec));
    memset(s_trail, 0, sizeof(s_trail));
    s_trail_head = 0;
    s_rec.magic = CRASH_MAGIC;
    s_rec.version = CRASH_VERSION;
    s_rec.boots = boots;
    s_rec.state = RUN_RUNNING;
    copy_name(s_rec.build, sizeof(s_rec.build), TINYTANG_BUILD_ID);
    rec_seal();
}

/* Arm the watchdog: 32 kHz / 32 = 1 kHz, so the compare value is in ms. */
static void wdt_start(void)
{
    if (!s_wdg) {
        s_wdg = bflb_device_get_by_name("watchdog");
    }
    if (!s_wdg) {
        return;
    }
    struct bflb_wdg_config_s cfg = {
        .clock_source = WDG_CLKSRC_32K,
        .clock_div = 31,
        .comp_val = CRASH_WDT_MS,
        .mode = WDG_MODE_RESET,
    };
    bflb_wdg_init(s_wdg, &cfg);
    bflb_wdg_start(s_wdg);
    s_wdt_running = true;
}

/* Write the record and trail to flash.  Interrupts must be off: the SDK's
 * flash routines suspend execute-in-place while they run, as tangflash's
 * commit relies on. */
static void persist_to_flash(void)
{
    s_flash_buf.rec = s_rec;
    s_flash_buf.head = s_trail_head;
    memcpy(s_flash_buf.trail, s_trail, sizeof(s_flash_buf.trail));
    (void)bflb_flash_erase(CRASH_FLASH_ADDR, CRASH_FLASH_SECTOR);
    (void)bflb_flash_write(CRASH_FLASH_ADDR, (uint8_t *)&s_flash_buf, sizeof(s_flash_buf));
}

/* Record the end of this run in RAM and flash, then reset.  Called from trap
 * and hook context, so it uses nothing that can block. */
static void record_and_wait(uint32_t kind, uint32_t mcause, uint32_t mepc, uint32_t mtval,
                            uint32_t ra, const char *task)
{
    __asm__ volatile("csrc mstatus, 8");      /* MIE off */
    s_rec.state = RUN_CRASHED;
    s_rec.kind = kind;
    s_rec.mcause = mcause;
    s_rec.mepc = mepc;
    s_rec.mtval = mtval;
    s_rec.ra = ra;
    s_rec.tick = (uint32_t)xTaskGetTickCountFromISR();
    copy_name(s_rec.task, sizeof(s_rec.task), task);
    rec_seal();
    if (s_wdg && s_wdt_running) {
        bflb_wdg_stop(s_wdg);                 /* not in the middle of the write */
    }
    persist_to_flash();
    GLB_SW_POR_Reset();
    for (;;) {
    }
}

static const char *current_task_name(void)
{
    return pxCurrentTCB ? pcTaskGetName(NULL) : "(no task)";
}

/* The Bluetooth controller library's fatal-error exit, renamed by the linker
 * (--wrap=platform_reset).  The library's own version turns interrupts off,
 * stores the error code and jumps to address 0, restarting the chip through
 * the boot ROM: it looks like a reset, lands in the vendor loader and passes
 * no handler on the way.  Two codes are its ordinary returns and go on as
 * before; anything else is recorded, with the code in mtval and the caller in
 * ra. */
void __real_platform_reset(uint32_t error);

void __wrap_platform_reset(uint32_t error)
{
    if (error == 0xC3C3C3C3u || error == 0xA5A5A5A5u) {
        __real_platform_reset(error);
        return;
    }
    record_and_wait(KIND_CONTROLLER, 0, 0, error, (uint32_t)__builtin_return_address(0),
                    current_task_name());
}

/* configASSERT's end (FreeRTOSConfig.h); the SDK's weak version spins. */
void vAssertCalled(void)
{
    record_and_wait(KIND_ASSERT, 0, 0, 0, (uint32_t)__builtin_return_address(0),
                    current_task_name());
}

/* The SDK's handler, renamed by the linker (--wrap=exception_entry): environment
 * calls go on to it as before; anything else is a crash. */
void __real_exception_entry(uintptr_t *regs);

void __wrap_exception_entry(uintptr_t *regs)
{
    static volatile bool inside;
    const uint32_t cause = csr_mcause();
    if ((cause & 0x3FFu) == 8u || (cause & 0x3FFu) == 11u) {
        __real_exception_entry(regs);
        return;
    }
    if (inside) {                             /* a fault while recording one */
        for (;;) {
        }
    }
    inside = true;
    record_and_wait(KIND_TRAP, cause, csr_mepc(), csr_mtval(),
                    regs ? (uint32_t)regs[1] : 0, current_task_name());
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    record_and_wait(KIND_STACK, 0, 0, 0, 0, name);
}

void vApplicationMallocFailedHook(void)
{
    record_and_wait(KIND_MALLOC, 0, 0, 0, (uint32_t)__builtin_return_address(0),
                    current_task_name());
}

/* Every few ticks: which task was running, and where.  The tick interrupt has
 * just saved that task's context, and the FreeRTOS port puts the interrupted
 * PC in the first word of the saved frame, which pxCurrentTCB points at. */
void vApplicationTickHook(void)
{
    static uint32_t n;
    /* A task above the watchdog task -- the timer task or the Bluetooth
     * controller -- spinning would starve it, and with it the only task-level
     * code that could record anything.  Interrupts are still on in that case,
     * so it is caught here, ahead of the watchdog's own timeout. */
    if (s_wdt_running && !s_stop_feeding &&
        (TickType_t)(xTaskGetTickCountFromISR() - s_wdog_seen) > pdMS_TO_TICKS(CRASH_WDOG_LATE_MS)) {
        record_and_wait(KIND_WDOG_STARVED, 0, 0, 0, 0, current_task_name());
    }
    if (++n < CRASH_TRAIL_TICKS || !pxCurrentTCB) {
        return;
    }
    n = 0;
    const uint32_t *frame = *(uint32_t *const *)pxCurrentTCB;
    crumb_t *c = &s_trail[s_trail_head % CRASH_TRAIL];
    c->tick = (uint32_t)xTaskGetTickCountFromISR();
    c->pc = frame ? frame[0] : 0;
    copy_name(c->task, sizeof(c->task), pcTaskGetName(NULL));
    s_trail_head++;
}

static void heartbeat_task(void *arg)
{
    (void)arg;
    for (;;) {
        s_heartbeat = xTaskGetTickCount();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void watchdog_task(void *arg)
{
    (void)arg;
    s_heartbeat = xTaskGetTickCount();
    s_wdog_seen = s_heartbeat;
    wdt_start();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(CRASH_FEED_MS));
        s_wdog_seen = xTaskGetTickCount();
        if (s_stop_feeding || !s_wdt_running) {
            continue;
        }
        const TickType_t quiet = xTaskGetTickCount() - s_heartbeat;
        if (quiet > pdMS_TO_TICKS(CRASH_STARVE_MS)) {
            /* Something has held the CPU for half a minute without yielding.
             * Record it and stop feeding; the trail shows what was running. */
            record_and_wait(KIND_STARVED, 0, 0, 0, 0, "(see the trail)");
        }
        bflb_wdg_reset_countervalue(s_wdg);
    }
}

void tang_crash_start(void)
{
    /* Just below the timer task and the Bluetooth controller, so only
     * something that stops the scheduler starves it. */
    (void)xTaskCreate(watchdog_task, "wdog", 512, NULL, configMAX_PRIORITIES - 2, NULL);
    (void)xTaskCreate(heartbeat_task, "beat", 256, NULL, 1, NULL);
}

void tang_crash_suspend(void)
{
    s_stop_feeding = true;
    if (s_wdg && s_wdt_running) {
        bflb_wdg_stop(s_wdg);
        s_wdt_running = false;
    }
    taskENTER_CRITICAL();
    s_rec.state = RUN_CLEAN;
    rec_seal();
    taskEXIT_CRITICAL();
}

/* ------------------------------------------------------------- reporting */

typedef int (*emit_fn)(void *ctx, const char *line);

static int emit_console(void *ctx, const char *line)
{
    (void)ctx;
    return tdsh_printf("%s\r\n", line);
}

static int emit_file(void *ctx, const char *line)
{
    UINT put;
    f_write((FIL *)ctx, line, (UINT)strlen(line), &put);
    return (int)f_write((FIL *)ctx, "\n", 1, &put);
}

static const char *cause_name(uint32_t mcause)
{
    static const char *const names[] = {
        "instruction address misaligned", "instruction access fault", "illegal instruction",
        "breakpoint", "load address misaligned", "load access fault",
        "store address misaligned", "store access fault",
    };
    const uint32_t c = mcause & 0x3FFu;
    return c < sizeof(names) / sizeof(names[0]) ? names[c] : "other";
}

/* The previous run, in lines, oldest trail sample last. */
static void report_lines(emit_fn emit, void *ctx)
{
    char line[120];
    const crash_rec_t *r = &s_last;
    /* The reset recorder's bits are active low: bit 3, wdt_rst_n, is 0 after
     * a watchdog reset and 1 after a power-up (0x7d). */
    const bool wdt = (s_reset_sts & 0x08u) == 0;

    if (r->state == RUN_CRASHED) {
        snprintf(line, sizeof(line), "crash: the last run (%s) ended with %s, in task %s, at tick %lu",
                 r->build, r->kind < sizeof(s_kind_names) / sizeof(s_kind_names[0])
                               ? s_kind_names[r->kind] : "?",
                 r->task[0] ? r->task : "?", (unsigned long)r->tick);
        emit(ctx, line);
        if (r->kind == KIND_CONTROLLER) {
            snprintf(line, sizeof(line), "crash:   controller error code 0x%08lx, called from 0x%08lx",
                     (unsigned long)r->mtval, (unsigned long)r->ra);
            emit(ctx, line);
        } else if (r->kind == KIND_ASSERT) {
            snprintf(line, sizeof(line), "crash:   assert failed in the function containing 0x%08lx",
                     (unsigned long)r->ra);
            emit(ctx, line);
        } else if (r->kind == KIND_TRAP) {
            snprintf(line, sizeof(line), "crash:   %s: mcause 0x%08lx mepc 0x%08lx mtval 0x%08lx ra 0x%08lx",
                     cause_name(r->mcause), (unsigned long)r->mcause, (unsigned long)r->mepc,
                     (unsigned long)r->mtval, (unsigned long)r->ra);
            emit(ctx, line);
        } else if (r->kind == KIND_MALLOC) {
            snprintf(line, sizeof(line), "crash:   called from 0x%08lx", (unsigned long)r->ra);
            emit(ctx, line);
        }
    } else if (r->state == RUN_RUNNING) {
        snprintf(line, sizeof(line),
                 "crash: the last run (%s) was reset while running%s, at about tick %lu: "
                 "a hang with interrupts off, or a reset from outside",
                 r->build, wdt ? " by the watchdog" : "",
                 (unsigned long)s_last_trail[(s_last_head + CRASH_TRAIL - 1) % CRASH_TRAIL].tick);
        emit(ctx, line);
    } else {
        return;                                /* RUN_CLEAN: a deliberate reset */
    }
    snprintf(line, sizeof(line), "crash:   record from %s; the trail, newest first (tick task pc):",
             s_last_from_flash ? "flash" : "RAM");
    emit(ctx, line);
    const uint32_t n = s_last_head < CRASH_TRAIL ? s_last_head : CRASH_TRAIL;
    for (uint32_t i = 0; i < n && i < 12; i++) {
        const crumb_t *c = &s_last_trail[(s_last_head - 1 - i) % CRASH_TRAIL];
        char task[9];
        copy_name(task, sizeof(task), c->task);
        snprintf(line, sizeof(line), "crash:     %8lu  %-8s 0x%08lx", (unsigned long)c->tick,
                 task, (unsigned long)c->pc);
        emit(ctx, line);
    }
}

void tang_crash_report(void)
{
    static bool done;
    if (done) {
        return;
    }
    done = true;
    if (s_last_from_flash) {
        /* Reported once: the sector is cleared so the next boot is clean. */
        taskENTER_CRITICAL();
        (void)bflb_flash_erase(CRASH_FLASH_ADDR, CRASH_FLASH_SECTOR);
        taskEXIT_CRITICAL();
    }
    if (!s_last_valid || s_last.state == RUN_CLEAN) {
        return;
    }
    report_lines(emit_console, NULL);

    static FIL f;
    if (f_open(&f, CRASH_LOG_PATH, FA_OPEN_APPEND | FA_WRITE) == FR_OK) {
        report_lines(emit_file, &f);
        UINT put;
        f_write(&f, "\n", 1, &put);
        f_close(&f);
        tdsh_printf("crash: saved to %s\r\n", CRASH_LOG_PATH);
    }
}

/* --------------------------------------------------------------- command */

static int cmd_crash(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    if (argc >= 3 && strcmp(argv[1], "test") == 0) {
        if (strcmp(argv[2], "trap") == 0) {
            tdsh_printf("crash: executing an illegal instruction\r\n");
            vTaskDelay(pdMS_TO_TICKS(100));
            __asm__ volatile(".word 0");
        } else if (strcmp(argv[2], "hang") == 0) {
            tdsh_printf("crash: interrupts off, spinning; the watchdog should reset in %d s. "
                        "Nothing can run to record this one, so expect no record.\r\n",
                        CRASH_WDT_MS / 1000);
            vTaskDelay(pdMS_TO_TICKS(100));
            taskENTER_CRITICAL();
            for (;;) {
            }
        } else if (strcmp(argv[2], "spin") == 0) {
            tdsh_printf("crash: spinning without yielding; the heartbeat check should "
                        "reset in about %d s\r\n", (CRASH_STARVE_MS + CRASH_WDT_MS) / 1000);
            vTaskDelay(pdMS_TO_TICKS(100));
            for (;;) {
            }
        }
        tdsh_printf("usage: crash test trap | hang | spin\r\n");
        return 1;
    }

    tdsh_printf("crash: this run %s, watchdog %s, resets kept since power-up %lu, "
                "reset status 0x%02x (bit 3 low = watchdog)\r\n",
                TINYTANG_BUILD_ID, s_wdt_running ? "on" : "off",
                (unsigned long)s_rec.boots, s_reset_sts);
    if (!s_last_valid) {
        tdsh_printf("crash: no record from a previous run (the last reset was a power-up)\r\n");
    } else if (s_last.state == RUN_CLEAN) {
        tdsh_printf("crash: the previous run ended in a deliberate reset\r\n");
    } else {
        report_lines(emit_console, NULL);
    }
    return 0;
}

static const tdsh_command_t s_crash_commands[] = {
    { "crash", "crash [test trap|hang|spin]",
      "Show how the previous run ended (crash or hang record), or cause one to test it",
      cmd_crash, 0 },
};

int tang_crash_register(void)
{
    return tdsh_register_commands(s_crash_commands,
                                  sizeof(s_crash_commands) / sizeof(s_crash_commands[0]));
}
