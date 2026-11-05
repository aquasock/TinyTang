/* TinyTang — host test for the crash and hang recorder (ports/bl616/tang_crash.c).
 *
 * This is the module that has to work when nothing else did, so the parts
 * worth testing are the decisions it makes: which events are recorded and
 * which are passed through, whether the record survives a reset intact, and
 * what it says afterwards.  Its hardware -- the CSRs, the watchdog, the flash
 * sector and the reset itself -- is replaced here:
 *
 *   - the three CSRs are settable, so a trap's cause can be chosen;
 *   - the flash sector is a buffer, so the persist-and-reload path runs;
 *   - GLB_SW_POR_Reset() leaves through longjmp instead of resetting, which is
 *     the only way a test can observe what was written before the reset.
 *
 * Built with -DTINYTANG_HOST_TEST, which is the switch tang_crash.c uses to
 * replace four RISC-V-only instructions; on the target that macro is absent
 * and the same code is built as before.
 *
 * SPDX-License-Identifier: MIT
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "bflb_flash.h"
#include "bflb_name.h"
#include "bflb_wdg.h"
#include "bl616_glb.h"
#include "ff.h"
#include "tdsh.h"
#include "tang_heap.h"

/* ------------------------------------------------------------ the stubs -- */

static uint32_t csr_values[3];
uint32_t tang_crash_host_csr(int which) { return csr_values[which]; }

static jmp_buf reset_jmp;
static int reset_calls;
static int critical_depth;

void host_enter_critical(void) { critical_depth++; }
void host_exit_critical(void) { critical_depth--; }
int host_critical_depth(void) { return critical_depth; }

void vTaskDelay(TickType_t ticks) { (void)ticks; }
TickType_t xTaskGetTickCount(void) { return 0; }
TickType_t xTaskGetTickCountFromISR(void) { return 0; }
TaskHandle_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t stack,
                         void *arg, unsigned prio, TaskHandle_t *out)
{
    (void)fn; (void)name; (void)stack; (void)arg; (void)prio; (void)out;
    return (TaskHandle_t)1;
}
static const char *task_name = "shell";
const char *pcTaskGetName(TaskHandle_t task) { (void)task; return task_name; }
UBaseType_t uxTaskGetNumberOfTasks(void) { return 1; }
UBaseType_t uxTaskGetSystemState(TaskStatus_t *status, UBaseType_t room, uint32_t *total)
{
    if (room && status) {
        static const char n[] = "shell";
        status[0].pcTaskName = n;
        status[0].uxCurrentPriority = 1;
        status[0].usStackHighWaterMark = 64;
    }
    if (total) *total = 1;
    return 1;
}

/* The two symbols tang_crash.c expects the linker to rename. */
static uint32_t last_real_error;
static int real_reset_calls, real_exception_calls;
void __real_platform_reset(uint32_t error)
{
    last_real_error = error;
    real_reset_calls++;
}
void __real_exception_entry(uintptr_t *regs)
{
    (void)regs;
    real_exception_calls++;
}
void *volatile pxCurrentTCB = (void *)1;

/* The flash sector the record lives in. */
static uint8_t flash_sector[4096];
static int flash_erases, flash_writes;

int bflb_flash_read(uint32_t addr, void *buf, uint32_t len)
{
    if (addr != 0x003FF000u) return -1;
    memcpy(buf, flash_sector, len > sizeof flash_sector ? sizeof flash_sector : len);
    return 0;
}
int bflb_flash_erase(uint32_t addr, uint32_t len)
{
    (void)len;
    if (addr != 0x003FF000u) return -1;
    memset(flash_sector, 0xff, sizeof flash_sector);
    flash_erases++;
    return 0;
}
int bflb_flash_write(uint32_t addr, const void *buf, uint32_t len)
{
    if (addr != 0x003FF000u) return -1;
    memcpy(flash_sector, buf, len > sizeof flash_sector ? sizeof flash_sector : len);
    flash_writes++;
    return 0;
}

/* The reset the recorder ends with.  A test cannot survive one, so it leaves
 * through longjmp and the record stays observable. */
void GLB_SW_POR_Reset(void)
{
    reset_calls++;
    longjmp(reset_jmp, 1);
}

static GLB_RESET_RECORD_Type reset_reason;
int GLB_Get_Reset_Reason(GLB_RESET_RECORD_Type *r)
{
    if (r) *r = reset_reason;
    return 0;
}

static struct bflb_device_s wdg_dev;
static int wdg_started, wdg_stopped, wdg_feeds;
struct bflb_device_s *bflb_device_get_by_name(const char *name)
{
    (void)name;
    return &wdg_dev;
}
int bflb_wdg_init(struct bflb_device_s *dev, struct bflb_wdg_config_s *cfg)
{
    (void)dev; (void)cfg;
    return 0;
}
int bflb_wdg_start(struct bflb_device_s *dev) { (void)dev; wdg_started++; return 0; }
int bflb_wdg_stop(struct bflb_device_s *dev) { (void)dev; wdg_stopped++; return 0; }
int bflb_wdg_reset_countervalue(struct bflb_device_s *dev) { (void)dev; wdg_feeds++; return 0; }

static char out_text[65536];
static size_t out_len;
int tdsh_printf(const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(out_text + out_len, sizeof out_text - out_len, fmt, ap);
    va_end(ap);
    if (n > 0) out_len += (size_t)n;
    return n;
}
void tang_heap_print(const char *who) { (void)who; }
bool tdsh_bl616_worker_stack(size_t *total, size_t *free_bytes)
{
    (void)total; (void)free_bytes;
    return false;
}

/* FatFS: the crash log is appended to one in-memory file. */
static char log_text[8192];
static size_t log_len;
static int log_opened;
FRESULT f_open(FIL *f, const char *path, uint8_t mode)
{
    (void)path; (void)mode;
    log_opened++;
    f->fd = 1;
    return FR_OK;
}
FRESULT f_write(FIL *f, const void *buf, unsigned n, unsigned *put)
{
    (void)f;
    if (log_len + n < sizeof log_text) {
        memcpy(log_text + log_len, buf, n);
        log_len += n;
    }
    if (put) *put = n;
    return FR_OK;
}
FRESULT f_close(FIL *f) { (void)f; return FR_OK; }

/* The shell's command table. */
static int registered;
static const tdsh_command_t *reg_cmds;
int tdsh_register_commands(const tdsh_command_t *cmds, size_t count)
{
    reg_cmds = cmds;
    registered = (int)count;
    return 0;
}

/* The module under test. */
#include "../../ports/bl616/tang_crash.c"

/* --------------------------------------------------------------- checks -- */

static int failures;

#define CHECK(x)                                                              \
    do {                                                                      \
        if (!(x)) {                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                \
            failures++;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        long _a = (long)(a), _b = (long)(b);                                   \
        if (_a != _b) {                                                       \
            printf("FAIL %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, \
                   #a, _a, _b);                                               \
            failures++;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_STR(hay, needle)                                                \
    do {                                                                      \
        if (strstr((hay), (needle)) == NULL) {                                 \
            printf("FAIL %s:%d: output does not contain \"%s\"\n",             \
                   __FILE__, __LINE__, (needle));                              \
            failures++;                                                       \
        }                                                                     \
    } while (0)

/* Run a call that is expected *not* to end in a reset, and report whether one
 * happened anyway.  Without this the ordinary-return check cannot fail: the
 * reset would longjmp to a stale target and the test would carry on as though
 * nothing had gone wrong. */
static bool reset_from_call(void (*fn)(uint32_t), uint32_t arg)
{
    if (setjmp(reset_jmp) == 0) {
        fn(arg);
        return false;                   /* returned normally, as it must */
    }
    return true;                        /* it reset, which is the failure */
}

static void call_platform_reset(uint32_t error) { __wrap_platform_reset(error); }

static void reset_world(void)
{
    csr_values[0] = csr_values[1] = csr_values[2] = 0;
    reset_calls = 0;
    critical_depth = 0;
    memset(flash_sector, 0xff, sizeof flash_sector);
    flash_erases = flash_writes = 0;
    memset(&reset_reason, 0, sizeof reset_reason);
    wdg_started = wdg_stopped = wdg_feeds = 0;
    out_len = 0; out_text[0] = '\0';
    log_len = 0; log_text[0] = '\0';
    log_opened = 0;
    /* The module's own state. */
    s_rec = (crash_rec_t){0};
    memset(s_trail, 0, sizeof s_trail);
    s_trail_head = 0;
    s_last = (crash_rec_t){0};
    memset(s_last_trail, 0, sizeof s_last_trail);
    s_last_head = 0;
    s_last_valid = false;
    s_last_from_flash = false;
    s_reset_sts = 0;
    s_wdg = NULL;
    s_wdt_running = false;
    s_stop_feeding = false;
    s_heartbeat = 0;
    s_wdog_seen = 0;
    memset(&s_flash_buf, 0, sizeof s_flash_buf);
}

int main(void)
{
    /* ------------------------------------------------------- checksum ----
     * The sum is what makes a half-written or scrambled record detectable, so
     * it must cover every field before it and nothing after. */
    reset_world();
    {
        crash_rec_t a = {0};
        crash_rec_t b = a;
        const uint32_t sum = rec_sum(&a);
        CHECK(sum != 0);
        /* A change in any covered field changes the sum. */
        b.kind = 1;
        CHECK(rec_sum(&b) != sum);
        b = a; b.tick = 12345;
        CHECK(rec_sum(&b) != sum);
        b = a; b.task[0] = 'x';
        CHECK(rec_sum(&b) != sum);
        b = a; b.build[39] = 'z';
        CHECK(rec_sum(&b) != sum);
        /* The sum field itself is not covered: that is what lets a record be
         * sealed and then verified. */
        b = a; b.sum = 0xdeadbeef;
        CHECK_EQ(rec_sum(&b), sum);
        /* And it is order-sensitive, so a transposition is not invisible. */
        crash_rec_t c = {0}, d = {0};
        c.mcause = 1; c.mepc = 2;
        d.mcause = 2; d.mepc = 1;
        CHECK(rec_sum(&c) != rec_sum(&d));
    }

    /* ----------------------------------------------------- copy_name ---- */
    reset_world();
    {
        char dst[8];
        memset(dst, 'X', sizeof dst);
        copy_name(dst, sizeof dst, "abcd");
        CHECK_EQ(strcmp(dst, "abcd"), 0);
        /* The rest is zero-filled, so a shorter name cannot leave stale bytes
         * that a checksum or a report would pick up. */
        for (int i = 4; i < 8; i++) CHECK_EQ(dst[i], 0);
        /* Too long is truncated to n-1 and still terminated. */
        memset(dst, 'X', sizeof dst);
        copy_name(dst, sizeof dst, "abcdefghij");
        CHECK_EQ(strlen(dst), 7);
        CHECK_EQ(memcmp(dst, "abcdefg", 7), 0);
        /* A NULL source is an empty name, not a crash. */
        memset(dst, 'X', sizeof dst);
        copy_name(dst, sizeof dst, NULL);
        CHECK_EQ(dst[0], 0);
    }

    /* ---------------------------------------------------- cause_name ---- */
    reset_world();
    CHECK_EQ(strcmp(cause_name(0), "instruction address misaligned"), 0);
    CHECK_EQ(strcmp(cause_name(2), "illegal instruction"), 0);
    CHECK_EQ(strcmp(cause_name(7), "store access fault"), 0);
    /* Only the low ten bits name the cause; the interrupt bit and the high
     * bits are not part of it. */
    CHECK_EQ(strcmp(cause_name(2 | 0x80000000u), "illegal instruction"), 0);
    CHECK_EQ(strcmp(cause_name(2 | (1u << 10)), "illegal instruction"), 0);
    CHECK_EQ(strcmp(cause_name(9), "other"), 0);
    CHECK_EQ(strcmp(cause_name(0x3ff), "other"), 0);

    /* ----------------------------------------------------------- init ----
     * A power-up leaves nothing valid; a sealed RAM record is taken; a sealed
     * flash record wins over the RAM one and is marked as coming from flash. */
    reset_world();
    tang_crash_init();
    CHECK(!s_last_valid);
    CHECK_EQ(s_rec.magic, CRASH_MAGIC);
    CHECK_EQ(s_rec.state, RUN_RUNNING);
    CHECK_EQ(s_rec.sum, rec_sum(&s_rec));
    /* The build identity is copied in, so a record names the image that
     * made it. */
    CHECK_EQ(strcmp(s_rec.build, TINYTANG_BUILD_ID), 0);

    /* A RAM record from a previous run, sealed correctly, is taken. */
    reset_world();
    {
        crash_rec_t prev = {0};
        prev.magic = CRASH_MAGIC;
        prev.version = CRASH_VERSION;
        prev.state = RUN_CRASHED;
        prev.kind = KIND_TRAP;
        prev.boots = 3;
        prev.sum = rec_sum(&prev);
        memcpy(&s_rec, &prev, sizeof prev);
        tang_crash_init();
        CHECK(s_last_valid);
        CHECK(!s_last_from_flash);
        CHECK_EQ(s_last.kind, KIND_TRAP);
        /* The boot count carries forward. */
        CHECK_EQ(s_rec.boots, 4);
    }

    /* A corrupt record is not taken: a scrambled sum means it did not finish
     * being written. */
    reset_world();
    {
        crash_rec_t prev = {0};
        prev.magic = CRASH_MAGIC;
        prev.version = CRASH_VERSION;
        prev.state = RUN_CRASHED;
        prev.sum = rec_sum(&prev);
        prev.sum ^= 1;                     /* one bit wrong */
        memcpy(&s_rec, &prev, sizeof prev);
        tang_crash_init();
        CHECK(!s_last_valid);
        CHECK_EQ(s_rec.boots, 0);
    }
    /* So is a record with the wrong magic or a future version. */
    reset_world();
    {
        s_rec.magic = 0x12345678u;
        s_rec.version = CRASH_VERSION;
        s_rec.sum = rec_sum(&s_rec);
        tang_crash_init();
        CHECK(!s_last_valid);
    }
    reset_world();
    {
        s_rec.magic = CRASH_MAGIC;
        s_rec.version = CRASH_VERSION + 1;
        s_rec.sum = rec_sum(&s_rec);
        tang_crash_init();
        CHECK(!s_last_valid);
    }

    /* A flash record wins over a RAM one: it is the crash that survived the
     * power cycle, and the RAM contents after a power cycle are meaningless. */
    reset_world();
    {
        crash_flash_t fb;
        memset(&fb, 0, sizeof fb);
        fb.rec.magic = CRASH_MAGIC;
        fb.rec.version = CRASH_VERSION;
        fb.rec.state = RUN_CRASHED;
        fb.rec.kind = KIND_STACK;
        fb.head = 2;
        fb.trail[0].tick = 11;
        fb.trail[0].pc = 0x1000;
        fb.trail[1].tick = 22;
        fb.trail[1].pc = 0x2000;
        strcpy(fb.trail[1].task, "shell");
        fb.rec.sum = rec_sum(&fb.rec);
        memcpy(flash_sector, &fb, sizeof fb);

        crash_rec_t ram = {0};
        ram.magic = CRASH_MAGIC;
        ram.version = CRASH_VERSION;
        ram.state = RUN_CRASHED;
        ram.kind = KIND_ASSERT;
        ram.sum = rec_sum(&ram);
        memcpy(&s_rec, &ram, sizeof ram);

        tang_crash_init();
        CHECK(s_last_valid);
        CHECK(s_last_from_flash);
        CHECK_EQ(s_last.kind, KIND_STACK);
        CHECK_EQ(s_last_head, 2);
        CHECK_EQ(s_last_trail[1].pc, 0x2000);
    }

    /* -------------------------------------------------- suspend ---------- */
    reset_world();
    tang_crash_init();
    tang_crash_suspend();
    CHECK_EQ(s_rec.state, RUN_CLEAN);
    CHECK_EQ(s_rec.sum, rec_sum(&s_rec));   /* re-sealed after the change */

    /* ------------------------------------- the trap dispatch policy -------
     * Environment calls (mcause 8 and 11) are ordinary and go on to the SDK's
     * handler; anything else is a crash and is recorded.  Recording ends in a
     * reset, which the stub turns into a longjmp.
     *
     * Only one real trap can run per process: the handler keeps a static
     * "already recording" flag so that a fault during a recording spins rather
     * than recursing, and that flag is correct behaviour to leave set.  So the
     * pass-throughs come first -- they never set it -- and then the single
     * fault, chosen to cover the most: a code argument rather than a frame,
     * which also carries the kind, the cause, the return state and the
     * persistence that the next section reads back. */
    reset_world();
    tang_crash_init();
    {
        uintptr_t frame[8] = {0xAAAA, 0xBBBB};
        /* These two return normally, so they need no setjmp. */
        csr_values[0] = 8;                  /* environment call from U-mode */
        __wrap_exception_entry(frame);
        CHECK_EQ(reset_calls, 0);           /* passed straight through */
        CHECK_EQ(real_exception_calls, 1);
        csr_values[0] = 11;                 /* machine-mode environment call */
        __wrap_exception_entry(frame);
        CHECK_EQ(reset_calls, 0);
        CHECK_EQ(real_exception_calls, 2);

        /* An illegal instruction is a crash.  Here the "frame" pointer is
         * really the exception code, as 2.3.36's trap handler passes it, so no
         * callee register is taken: treating the code as a frame is what
         * faulted a second time before the record could be made. */
        csr_values[0] = 2;
        csr_values[1] = 0x80001234;         /* mepc */
        csr_values[2] = 0xdead;             /* mtval */
        const uintptr_t code = csr_values[0] & 0x807fffffu;
        /* The crash ends in a reset, so the call runs only on the first pass;
         * longjmp returns here, to the else branch. */
        if (setjmp(reset_jmp) == 0) {
            __wrap_exception_entry((uintptr_t *)code);
            CHECK(0);                       /* a crash must not return */
        }
        CHECK_EQ(reset_calls, 1);
        CHECK_EQ(s_rec.kind, KIND_TRAP);
        CHECK_EQ(s_rec.mcause, 2);
        CHECK_EQ(s_rec.mepc, 0x80001234);
        CHECK_EQ(s_rec.mtval, 0xdead);
        CHECK_EQ(s_rec.ra, 0);              /* a code argument, not a frame */
        CHECK_EQ(s_rec.state, RUN_CRASHED);
        CHECK_EQ(s_rec.sum, rec_sum(&s_rec));
        CHECK_EQ(strcmp(s_rec.task, task_name), 0);
        CHECK_EQ(flash_erases, 1);
        CHECK_EQ(flash_writes, 1);
    }

    /* ------------------------------------------- persist and reload ------
     * The record written before the reset is the one the next boot reads, so
     * the trap above must survive the round trip through the sector.  This is
     * a fresh power-up: RAM is gone, the sector is not. */
    {
        uint8_t saved[sizeof flash_sector];
        memcpy(saved, flash_sector, sizeof saved);
        reset_world();
        memcpy(flash_sector, saved, sizeof saved);
        tang_crash_init();
        CHECK(s_last_valid);
        CHECK(s_last_from_flash);
        CHECK_EQ(s_last.kind, KIND_TRAP);
        CHECK_EQ(s_last.mcause, 2);
        CHECK_EQ(s_last.mepc, 0x80001234);
        CHECK_EQ(s_last.mtval, 0xdead);
        CHECK_EQ(s_last.state, RUN_CRASHED);
    }

    /* --------------------------------- the controller's fatal-error exit --
     * Two codes are the library's ordinary returns and must go on as before;
     * a board reset on every ordinary return would be a permanent boot loop.
     * Anything else is recorded, with the code in mtval. */
    reset_world();
    tang_crash_init();
    {
        CHECK(!reset_from_call(call_platform_reset, 0xC3C3C3C3u));
        CHECK_EQ(real_reset_calls, 1);
        CHECK_EQ(last_real_error, 0xC3C3C3C3u);
        CHECK_EQ(reset_calls, 0);
        CHECK(!reset_from_call(call_platform_reset, 0xA5A5A5A5u));
        CHECK_EQ(real_reset_calls, 2);
        CHECK_EQ(reset_calls, 0);

        /* Anything else is a crash, recorded with the code in mtval.  The
         * longjmp returns here, past the call. */
        if (setjmp(reset_jmp) == 0) {
            __wrap_platform_reset(0x00000042u);
            CHECK(0);                    /* a crash must not return */
        }
        CHECK_EQ(reset_calls, 1);
        CHECK_EQ(s_rec.kind, KIND_CONTROLLER);
        CHECK_EQ(s_rec.mtval, 0x42);
    }

    /* ------------------------------------------------------ reporting ---
     * `tang_crash_report` prints once, appends to the card's log, and clears
     * the flash sector so the next boot starts clean. */
    reset_world();
    tang_crash_init();
    {
        crash_rec_t *r = &s_last;
        memset(r, 0, sizeof *r);
        r->state = RUN_CRASHED;
        r->kind = KIND_ASSERT;
        r->ra = 0x80001111;
        strcpy(r->build, "abc1234");
        s_last_valid = true;
        s_last_from_flash = true;
        out_len = 0; out_text[0] = '\0';
        tang_crash_report();
        CHECK_STR(out_text, "assert failed");
        CHECK_STR(out_text, "saved to /sd/crash.log");
        CHECK(log_opened == 1);
        CHECK(log_len > 0);
        CHECK(flash_erases >= 1);           /* the sector was cleared */
        /* A second call is silent: it is reported once per boot. */
        out_len = 0; out_text[0] = '\0';
        tang_crash_report();
        CHECK_EQ(out_len, 0);
    }

    /* ------------------------------------------------------ registration */
    reset_world();
    CHECK_EQ(tang_crash_register(), 0);
    CHECK_EQ(registered, 1);
    CHECK(reg_cmds != NULL);
    CHECK_EQ(strcmp(reg_cmds[0].name, "crash"), 0);

    CHECK_EQ(critical_depth, 0);

    if (failures) {
        printf("tb_tang_crash: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_tang_crash: all checks passed\n");
    return 0;
}
