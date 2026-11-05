/* TinyTang — host test for the core link (ports/bl616/tang_fpga_uart.c).
 *
 * Every register access, every OSD command and every byte of a core load in
 * this project passes through this file, so it is the one module whose
 * failure would be invisible everywhere at once.  It writes through a UART
 * FIFO and reads through an interrupt, both of which are hardware; the bench
 * takes their place:
 *
 *   - uart_write() reads the TX FIFO's free count from one register and
 *     pushes bytes to another, so a small register file captures the exact
 *     byte stream the core would see, and can refuse room to provoke the
 *     100 ms timeout;
 *   - the RX ring is fed directly, which is what the interrupt would do, so
 *     the frame path is exercised without an interrupt;
 *   - the transmit lock's owner is the "current task", which the bench can
 *     switch to make a second task contend for it.
 *
 * What is pinned here is the wire format and the locking rule, because both
 * fail silently: a frame whose length field is off by one desynchronises the
 * core's decoder, and a lock that is not re-entrant by owner deadlocks the
 * task against itself.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bflb_irq.h"
#include "bflb_mtimer.h"
#include "bflb_uart.h"
#include "bflb_gpio.h"
#include "ff.h"
#include "../../third_party/tinydesk-shell/include/tdsh.h"

/* The module's own headers: their prototypes fix the stub signatures. */
#include "../../ports/bl616/tdsh_bl616.h"
#include "tang_clock.h"
#include "tang_osd.h"
#include "tang_osd_desk.h"
#include "tang_ae350_run.h"
#include "hardware/uart_reg.h"

/* ------------------------------------------------------------ the stubs -- */

static uint32_t now_ms;
static int critical_depth;
static int critical_imbalance;

/* A polling loop that spins on the clock is how this module waits: uart_write
 * has no yield of its own, so its 100 ms bound advances only because real time
 * passes.  The virtual clock therefore advances a millisecond per read, which
 * is what makes that loop terminate here as it does on the board. */
static void pump_deferred(void);

uint32_t bflb_mtimer_get_time_ms(void) { pump_deferred(); return ++now_ms; }

/* A reply that arrives *during* a call, the way the RX interrupt delivers one
 * on the board.  tang_fpga_core_id() drains the ring before it transmits, so a
 * frame queued before the call would be thrown away; this is how a reply is
 * made available mid-wait instead. */
static unsigned char deferred[64];
static size_t deferred_len, deferred_after, deferred_reads;
static void (*deferred_feed)(const unsigned char *, size_t);

static void pump_deferred(void)
{
    if (!deferred_len) return;
    if (++deferred_reads >= deferred_after) {
        deferred_feed(deferred, deferred_len);
        deferred_len = 0;
    }
}
TickType_t xTaskGetTickCount(void) { return (TickType_t)now_ms; }
void vTaskDelay(TickType_t ticks) { now_ms += (uint32_t)ticks; }
void taskYIELD(void) { now_ms += 1; }

void host_enter_critical(void) { critical_depth++; }
void host_exit_critical(void)
{
    critical_depth--;
    if (critical_depth < 0) critical_imbalance++;
}
int host_critical_depth(void) { return critical_depth; }

/* The current task is whoever the test says it is, so contention between two
 * tasks is a switch rather than a second thread. */
static TaskHandle_t current_task = (TaskHandle_t)1;
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return current_task; }

/* One mutex, with a waiter count so a bounded take can be made to fail. */
static int mutex_held;
static int mutex_created;
static int mutex_timeouts;
SemaphoreHandle_t xSemaphoreCreateMutex(void) { mutex_created++; return (SemaphoreHandle_t)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait)
{
    (void)s;
    if (!mutex_held) { mutex_held = 1; return pdTRUE; }
    if (wait == portMAX_DELAY) {
        /* A blocking take would hang the test; the module only does this when
         * it is not the owner, which the checks arrange not to happen. */
        printf("FAIL: unbounded take while held\n");
        abort();
    }
    mutex_timeouts++;
    return pdFALSE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t s)
{
    (void)s;
    mutex_held = 0;
    return pdTRUE;
}

/* ---- the UART register file tang_fpga_uart.c writes through ---- */

static unsigned char wire[65536];
static size_t wire_len;
static uint32_t intstatus;
static int fifo_room = 32;          /* bytes the TX FIFO will accept at once */
static int fifo_stuck;              /* report no room for ever */
static int uart_inited;
static uint32_t uart_baud_seen;
static int uart_deinits;
static int rx_available;
static unsigned char rx_fifo[4096];
static size_t rx_head, rx_tail;

static host_isr_t attached_isr;

void bflb_irq_attach(int irq, host_isr_t isr, void *arg)
{
    (void)irq; (void)arg;
    attached_isr = isr;
}
void bflb_irq_enable(int irq) { (void)irq; }
uintptr_t bflb_irq_save(void) { return 0; }
void bflb_irq_restore(uintptr_t s) { (void)s; }
host_isr_t host_attached_isr(void) { return attached_isr; }

static struct bflb_device_s s_uart1 = { .irq_num = 40, .reg_base = 0x5000 };

struct bflb_device_s *bflb_device_get_by_name(const char *name)
{
    if (strcmp(name, "uart1") == 0) return &s_uart1;
    return &s_uart1;   /* the gpio device handle is only passed through */
}

int bflb_uart_init(struct bflb_device_s *dev, const struct bflb_uart_config_s *cfg)
{
    (void)dev;
    uart_inited++;
    uart_baud_seen = cfg->baudrate;
    return 0;
}
int bflb_uart_deinit(struct bflb_device_s *dev) { (void)dev; uart_deinits++; return 0; }
int bflb_uart_get_intstatus(struct bflb_device_s *dev) { (void)dev; return (int)intstatus; }
bool bflb_uart_rxavailable(struct bflb_device_s *dev) { (void)dev; return rx_tail < rx_head; }
int bflb_uart_getchar(struct bflb_device_s *dev)
{
    (void)dev;
    return rx_tail < rx_head ? rx_fifo[rx_tail++] : -1;
}
void bflb_uart_int_clear(struct bflb_device_s *dev, uint32_t mask) { (void)dev; intstatus &= ~mask; }
void bflb_uart_rxint_mask(struct bflb_device_s *dev, bool mask) { (void)dev; (void)mask; }
int bflb_uart_txempty(struct bflb_device_s *dev) { (void)dev; return 1; }
int bflb_uart_putchar(struct bflb_device_s *dev, int ch) { (void)dev; (void)ch; return 0; }
void bflb_gpio_uart_init(void *dev, uint32_t pin, int func) { (void)dev; (void)pin; (void)func; }
void host_set_intstatus(uint32_t bits) { intstatus = bits; }

uint32_t host_reg_read(uint32_t address)
{
    if (address == s_uart1.reg_base + UART_FIFO_CONFIG_1_OFFSET) {
        const uint32_t room = fifo_stuck ? 0u : (uint32_t)fifo_room;
        return (room << UART_TX_FIFO_CNT_SHIFT) & UART_TX_FIFO_CNT_MASK;
    }
    return 0;
}

void host_reg_write(uint32_t address, uint8_t value)
{
    if (address == s_uart1.reg_base + UART_FIFO_WDATA_OFFSET) {
        if (wire_len < sizeof wire) wire[wire_len++] = value;
    }
}

/* The shell and the console. */
static char out_text[65536];
static size_t out_len;
int tdsh_printf(const char *fmt, ...);
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

static int registered;
static const tdsh_command_t *reg_cmds;
int tdsh_register_commands(const tdsh_command_t *cmds, size_t count)
{
    reg_cmds = cmds;
    registered = (int)count;
    return 0;
}

/* The shell's path resolver, which maps a session path onto the card.  The
 * module only needs it to turn an argument into a FatFS path, so this stub
 * does that and records nothing else. */
int tdsh_path_to_real(tdsh_session_t *session, const char *input,
                      char *real_out, size_t real_out_size,
                      char *logical_out, size_t logical_out_size);
int tdsh_path_to_real(tdsh_session_t *session, const char *input,
                      char *real_out, size_t real_out_size,
                      char *logical_out, size_t logical_out_size)
{
    (void)session; (void)logical_out; (void)logical_out_size;
    const int n = snprintf(real_out, real_out_size, "/sd%s",
                           input[0] == '/' ? input : "");
    return (n > 0 && (size_t)n < real_out_size) ? 0 : -1;
}

static int raw_mode_calls;
void tdsh_bl616_console_set_raw(bool on) { (void)on; raw_mode_calls++; }
bool tdsh_bl616_console_write_raw(const void *data, size_t n) { (void)data; (void)n; return true; }

/* A host on the far side of `fpgapipe`: its bytes come from a script, and it
 * "closes the port" once the script is used up, which ends the pipe. */
static const unsigned char *host_script;
static size_t host_script_len, host_script_at;
bool tdsh_bl616_console_connected(void)
{
    return host_script == NULL || host_script_at < host_script_len;
}
int tdsh_bl616_console_read_byte(void)
{
    if (host_script == NULL || host_script_at >= host_script_len) {
        return -1;                       /* no host typing */
    }
    return host_script[host_script_at++];
}

/* The clock and the OSD, which other commands in this file call. */
static int64_t s_now_utc;
bool tang_clock_now(int64_t *utc) { *utc = s_now_utc; return true; }
long tang_clock_zone(void) { return 0; }
int tang_osd_set(bool on) { (void)on; return 0; }
int tang_osd_desk_set(bool on) { (void)on; return 0; }
void tang_osd_desk_core_reloaded(void) { }
bool tang_osd_shown(void) { return true; }
static int osd_notes;
static bool osd_noted;
static int osd_noted_locked;
void tang_osd_note_locked(bool on)
{
    osd_notes++;
    osd_noted = on;
    osd_noted_locked = mutex_held;
}

/* The AE350 runner lives in this module too (tang_ae350_run.h declares it and
 * tang_fpga_uart.c defines it), so nothing is stubbed for it. */

/* ---- FatFS: a tree the save path can write into ---- */

struct fake_file { const char *path; unsigned char data[8192]; size_t len; size_t at; int open; int append; };
static struct fake_file files[8];
static int file_count;

static struct fake_file *find_file(const char *path)
{
    for (int i = 0; i < file_count; i++)
        if (strcmp(files[i].path, path) == 0) return &files[i];
    return NULL;
}

FRESULT f_open(FIL *fp, const char *path, uint8_t mode)
{
    struct fake_file *f = find_file(path);
    if (!f) {
        if (!(mode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW))) return FR_NO_FILE;
        if (file_count >= 8) return FR_DENIED;
        f = &files[file_count];
        f->path = path;
        f->len = 0;
        f->open = 1;
        file_count++;
    }
    f->at = (mode & FA_OPEN_APPEND) ? f->len : 0;
    f->append = (mode & FA_OPEN_APPEND) != 0;
    f->open = 1;
    fp->fd = (int)(f - files) + 1;
    return FR_OK;
}
FRESULT f_read(FIL *fp, void *buf, UINT n, UINT *got)
{
    struct fake_file *f = &files[fp->fd - 1];
    UINT i = 0;
    while (i < n && f->at < f->len) ((unsigned char *)buf)[i++] = f->data[f->at++];
    if (got) *got = i;
    return FR_OK;
}
FRESULT f_write(FIL *fp, const void *buf, UINT n, UINT *wrote)
{
    struct fake_file *f = &files[fp->fd - 1];
    UINT i = 0;
    while (i < n && f->at < sizeof f->data) f->data[f->at++] = ((const unsigned char *)buf)[i++];
    if (f->at > f->len) f->len = f->at;
    if (wrote) *wrote = i;
    return FR_OK;
}
FRESULT f_close(FIL *fp) { files[fp->fd - 1].open = 0; return FR_OK; }
FRESULT f_unlink(const char *path)
{
    struct fake_file *f = find_file(path);
    if (f) f->len = 0;
    return FR_OK;
}
FSIZE_t f_size(FIL *fp) { return (FSIZE_t)files[fp->fd - 1].len; }
FRESULT f_lseek(FIL *fp, FSIZE_t ofs) { files[fp->fd - 1].at = (size_t)ofs; return FR_OK; }
FRESULT f_mkdir(const char *path) { (void)path; return FR_OK; }
FRESULT f_sync(FIL *fp) { (void)fp; return FR_OK; }
FRESULT f_rename(const char *from, const char *to) { (void)from; (void)to; return FR_OK; }

/* The link's own headers carry the constants. */
#include "tang_fpga_link.h"
#include "fpga_frames.h"

/* The module under test. */
#include "../../ports/bl616/tang_fpga_uart.c"

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

static void reset_world(void)
{
    now_ms = 100000;
    critical_depth = 0;
    critical_imbalance = 0;
    current_task = (TaskHandle_t)1;
    mutex_held = 0;
    mutex_created = 0;
    mutex_timeouts = 0;
    wire_len = 0;
    intstatus = 0;
    fifo_room = 32;
    fifo_stuck = 0;
    uart_inited = 0;
    uart_baud_seen = 0;
    uart_deinits = 0;
    rx_head = rx_tail = 0;
    rx_available = 0;
    attached_isr = NULL;
    out_len = 0;
    out_text[0] = '\0';
    raw_mode_calls = 0;
    file_count = 0;
    deferred_len = deferred_after = deferred_reads = 0;
    deferred_feed = NULL;
    /* The module's own statics. */
    s_uart_ready = false;
    s_uart = NULL;
    s_baud = FPGA_UART_BAUD;
    s_tx_lock = NULL;
    s_lock_owner = NULL;
    s_lock_depth = 0;
    s_rx_head = s_rx_tail = 0;
    s_rx_drops = 0;
    s_pipe = false;
    fpga_frames_reset();
    if (s_tx_lock) s_tx_lock = NULL;
}

/* Put a frame in the RX ring the way the interrupt would. */
static void feed(const uint8_t *bytes, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        const uint32_t next = (s_rx_head + 1) & (FPGA_RX_RING_SIZE - 1);
        if (next != s_rx_tail) { s_rx_ring[s_rx_head] = bytes[i]; s_rx_head = next; }
        else s_rx_drops++;
    }
}

static void feed_thunk(const unsigned char *bytes, size_t n)
{
    feed(bytes, n);
}

/* Arrange for `bytes` to arrive after `reads` polls of the clock. */
static void arrive_later(const uint8_t *bytes, size_t n, size_t reads)
{
    memcpy(deferred, bytes, n);
    deferred_len = n;
    deferred_after = reads;
    deferred_reads = 0;
    deferred_feed = feed_thunk;
}

/* One complete frame, with the length counting the type byte. */
static size_t make_frame(uint8_t *out, uint8_t type, const uint8_t *payload, size_t n)
{
    const size_t len = n + 1;
    out[0] = FPGA_FRAME_MAGIC;
    out[1] = (uint8_t)(len >> 8);
    out[2] = (uint8_t)len;
    out[3] = type;
    if (n) memcpy(out + 4, payload, n);
    return 4 + n;
}

int main(void)
{
    /* ------------------------------------------------------------ open ---- */
    reset_world();
    CHECK_EQ(tang_fpga_link_open(), 0);
    CHECK(s_uart_ready);
    CHECK_EQ(uart_inited, 1);
    CHECK_EQ(uart_baud_seen, 2000000u);
    CHECK(attached_isr != NULL);         /* the RX interrupt was wired */
    CHECK_EQ(tang_fpga_baud(), 2000000u);
    /* A second open is a no-op, not a re-init that would drop the interrupt. */
    CHECK_EQ(tang_fpga_link_open(), 0);
    CHECK_EQ(uart_inited, 1);

    /* -------------------------------------------------- frame construction --
     * AA len_hi len_lo type payload, where len counts the type byte. An
     * off-by-one here desynchronises the core's decoder for everything after
     * it, which is exactly the kind of fault that looks like a dead core. */
    reset_world();
    (void)tang_fpga_link_open();
    CHECK_EQ(tang_fpga_frame(FPGA_CMD_CORE_ID, NULL, 0), 0);
    CHECK_EQ(wire_len, 4);
    CHECK_EQ(wire[0], 0xAA);
    CHECK_EQ(wire[1], 0x00);
    CHECK_EQ(wire[2], 0x01);         /* length 1: the type byte alone */
    CHECK_EQ(wire[3], FPGA_CMD_CORE_ID);

    reset_world();
    (void)tang_fpga_link_open();
    {
        const uint8_t payload[3] = {0x11, 0x22, 0x33};
        CHECK_EQ(tang_fpga_frame(FPGA_CMD_OSD_CURSOR, payload, 3), 0);
        CHECK_EQ(wire_len, 7);
        CHECK_EQ(wire[2], 4);        /* 3 payload + the type byte */
        CHECK_EQ(wire[3], FPGA_CMD_OSD_CURSOR);
        CHECK(wire[4] == 0x11 && wire[5] == 0x22 && wire[6] == 0x33);
    }

    /* A big-endian length above 255 sets the high byte. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        static uint8_t big[300];
        memset(big, 0x5a, sizeof big);
        CHECK_EQ(tang_fpga_frame(FPGA_CMD_ROM_DATA, big, 300), 0);
        CHECK_EQ(wire_len, 304);
        CHECK_EQ(wire[1], 301 >> 8);
        CHECK_EQ(wire[2], 301 & 0xff);
    }

    /* The frame limit counts the type byte: 2046 payload is legal, 2047 is
     * not, because the length field would be 2048. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        static uint8_t payload[FPGA_FRAME_MAX];
        CHECK_EQ(tang_fpga_frame(FPGA_CMD_ROM_DATA, payload, FPGA_FRAME_MAX - 1), 0);
        CHECK_EQ(wire_len, 3u + FPGA_FRAME_MAX);
        wire_len = 0;
        CHECK_EQ(tang_fpga_frame(FPGA_CMD_ROM_DATA, payload, FPGA_FRAME_MAX), -1);
        CHECK_EQ(wire_len, 0);       /* nothing left on the wire */
    }

    /* A stuck transmitter is reported rather than spinning for ever. */
    reset_world();
    (void)tang_fpga_link_open();
    fifo_stuck = 1;
    CHECK_EQ(tang_fpga_frame(FPGA_CMD_CORE_ID, NULL, 0), -1);
    fifo_stuck = 0;

    /* ---------------------------------------------------------- the lock --
     * Re-entrant by owner: a caller holding it across a whole request and its
     * reply calls the register helpers inside that hold, and a plain mutex
     * would deadlock the task against itself. */
    reset_world();
    (void)tang_fpga_link_open();
    current_task = (TaskHandle_t)1;
    tang_fpga_lock();
    CHECK_EQ(s_lock_depth, 1);
    tang_fpga_lock();                    /* nested by the same task */
    CHECK_EQ(s_lock_depth, 2);
    tang_fpga_lock();
    CHECK_EQ(s_lock_depth, 3);
    tang_fpga_unlock();
    CHECK_EQ(s_lock_depth, 2);
    CHECK(mutex_held);                   /* still ours */
    tang_fpga_unlock();
    tang_fpga_unlock();
    CHECK_EQ(s_lock_depth, 0);
    CHECK(s_lock_owner == NULL);
    CHECK(!mutex_held);                  /* given back exactly once */

    /* Another task's unlock is not ours to give: the holder keeps it. */
    reset_world();
    (void)tang_fpga_link_open();
    current_task = (TaskHandle_t)1;
    tang_fpga_lock();
    current_task = (TaskHandle_t)2;
    tang_fpga_unlock();
    CHECK(mutex_held);                   /* untouched */
    CHECK(s_lock_owner == (TaskHandle_t)1);
    CHECK_EQ(s_lock_depth, 1);
    current_task = (TaskHandle_t)1;
    tang_fpga_unlock();
    CHECK(!mutex_held);

    /* A bounded take succeeds when free and nests for the owner. */
    reset_world();
    (void)tang_fpga_link_open();
    current_task = (TaskHandle_t)1;
    CHECK(tang_fpga_lock_within(100));
    CHECK_EQ(s_lock_depth, 1);
    CHECK(tang_fpga_lock_within(100));   /* same owner: nest, never block */
    CHECK_EQ(s_lock_depth, 2);
    tang_fpga_unlock();
    tang_fpga_unlock();
    CHECK(!mutex_held);

    /* A bounded take by another task fails rather than blocking for ever,
     * which is what keeps a host pipe or a stuck load from wedging a caller. */
    reset_world();
    (void)tang_fpga_link_open();
    current_task = (TaskHandle_t)1;
    tang_fpga_lock();
    current_task = (TaskHandle_t)2;
    CHECK(!tang_fpga_lock_within(100));
    CHECK_EQ(mutex_timeouts, 1);
    current_task = (TaskHandle_t)1;
    tang_fpga_unlock();

    /* With no lock created yet -- before the link is opened -- the calls are
     * harmless rather than crashing. */
    reset_world();
    tang_fpga_lock();
    tang_fpga_unlock();
    CHECK(!tang_fpga_lock_within(10));

    /* -------------------------------------------------- the RX ring ----
     * The interrupt moves bytes into a ring; drain and decode move them on. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        uint8_t f[16];
        const size_t n = make_frame(f, FPGA_RESP_CORE_ID, (const uint8_t *)"\x54", 1);
        feed(f, n);
        uint8_t id = 0;
        CHECK_EQ(tang_fpga_wait(FPGA_RESP_CORE_ID, &id, 1, 50), 1);
        CHECK_EQ(id, 0x54);
    }

    /* A frame of another type is parsed and discarded, not left to
     * desynchronise the reader -- the core sends joypads unprompted. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        uint8_t f[16];
        size_t n = make_frame(f, FPGA_RESP_JOYPAD, (const uint8_t *)"\x01\x02\x03\x04", 4);
        feed(f, n);
        n = make_frame(f, FPGA_RESP_CORE_ID, (const uint8_t *)"\x54", 1);
        feed(f, n);
        uint8_t id = 0;
        CHECK_EQ(tang_fpga_wait(FPGA_RESP_CORE_ID, &id, 1, 50), 1);
        CHECK_EQ(id, 0x54);
    }

    /* A frame that does not fit the caller's buffer is refused at once:
     * waiting out the timeout for it would only delay the answer. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        uint8_t f[32];
        const size_t n = make_frame(f, FPGA_RESP_CORE_ID, (const uint8_t *)"\x01\x02\x03\x04\x05", 5);
        feed(f, n);
        uint8_t small[1];
        CHECK_EQ(tang_fpga_wait(FPGA_RESP_CORE_ID, small, sizeof small, 50), -1);
    }

    /* Nothing at all times out. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        uint8_t id = 0;
        const uint32_t t0 = now_ms;
        CHECK_EQ(tang_fpga_wait(FPGA_RESP_CORE_ID, &id, 1, 30), -1);
        CHECK(now_ms - t0 >= 30);        /* it actually waited */
    }

    /* tang_fpga_drain throws away a half-seen frame so the next one parses. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        const uint8_t half[2] = {FPGA_FRAME_MAGIC, 0x00};
        feed(half, 2);
        tang_fpga_drain();
        uint8_t f[16];
        const size_t n = make_frame(f, FPGA_RESP_CORE_ID, (const uint8_t *)"\x54", 1);
        feed(f, n);
        uint8_t id = 0;
        CHECK_EQ(tang_fpga_wait(FPGA_RESP_CORE_ID, &id, 1, 50), 1);
    }

    /* ------------------------------------------------- core id request --
     * The command is sent and the reply taken from the ring. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        uint8_t f[16];
        const size_t n = make_frame(f, FPGA_RESP_CORE_ID, (const uint8_t *)"\x54", 1);
        /* The reply arrives during the wait: core_id() drains the ring before
         * it transmits, so a frame queued first would be discarded -- and that
         * drain is deliberate, it is what stops a stale answer being taken for
         * this one. */
        arrive_later(f, n, 3);
        uint8_t id = 0;
        CHECK(tang_fpga_core_id(&id, 200));
        CHECK_EQ(id, 0x54);
        /* And the request on the wire was the core-id command. */
        CHECK_EQ(wire_len, 4);
        CHECK_EQ(wire[3], FPGA_CMD_CORE_ID);
    }

    /* With nothing answering, core_id reports failure rather than blocking. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        uint8_t id = 0;
        CHECK(!tang_fpga_core_id(&id, 40));
        CHECK_EQ(wire_len, 4);           /* the request still went out */
    }

    /* ---------------------------------------------- extended read32 -----
     * 14-byte payload under frame type 0x10: version, opcode, seq, address,
     * value, CRC-16 over the type byte and the first twelve payload bytes. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        /* Feed the reply the core would give for address 0x140, value
         * 0x00000042, status 0. */
        uint8_t rep[15] = {0};
        rep[0] = 1; rep[1] = 0x81; rep[2] = 0; rep[3] = 0; rep[4] = 0;
        /* sequence bytes 7..8 */
        rep[7] = 0x00; rep[8] = 0x01;
        /* address 0x00000140 */
        rep[9] = 0x00; rep[10] = 0x00; rep[11] = 0x01; rep[12] = 0x40;
        /* the core fills bytes 5..6 and 13..14; the reader takes 9..12 */
        uint8_t f[32];
        const size_t n = make_frame(f, 0x10, rep, 15);
        feed(f, n);

        uint32_t value = 0;
        CHECK(tang_fpga_read32(0x140u, &value));
        CHECK_EQ(value, 0x140);         /* bytes 9..12, big-endian */

        /* The request: magic, len 15 (14 payload + type), type 0x10. */
        CHECK_EQ(wire_len, 18);
        CHECK_EQ(wire[0], 0xAA);
        CHECK_EQ(wire[2], 15);
        CHECK_EQ(wire[3], 0x10);
        CHECK_EQ(wire[4], 1);           /* version */
        CHECK_EQ(wire[5], 1);           /* opcode: read32 */
        CHECK_EQ(wire[6], 0);           /* seq hi */
        CHECK_EQ(wire[7], 0);           /* seq lo */
        /* the address, big-endian, at payload 4..7 (wire 8..11) */
        CHECK_EQ(wire[8], 0x00);
        CHECK_EQ(wire[9], 0x00);
        CHECK_EQ(wire[10], 0x01);
        CHECK_EQ(wire[11], 0x40);
        /* the value field is zero for a read (wire 12..15) */
        CHECK(wire[12] == 0 && wire[13] == 0 && wire[14] == 0 && wire[15] == 0);
        /* and the CRC covers 0x10 plus those twelve bytes */
        {
            uint16_t crc = 0xffffu;
            crc = fpga_crc16(crc, 0x10u);
            for (int i = 0; i < 12; i++) crc = fpga_crc16(crc, wire[4 + i]);
            CHECK_EQ(wire[16], crc >> 8);
            CHECK_EQ(wire[17], crc & 0xff);
        }
    }

    /* A non-zero status is an answer, not a transport failure, but the public
     * helper reports it as false because the register did not give its value. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        uint8_t rep[15] = {0};
        rep[0] = 1; rep[1] = 0x81; rep[2] = 6;   /* status 6: refused */
        uint8_t f[32];
        const size_t n = make_frame(f, 0x10, rep, 15);
        feed(f, n);
        uint32_t value = 0;
        CHECK(!tang_fpga_read32(0xc0u, &value));
    }

    /* A reply of the wrong length is a failure, not a partial read. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        uint8_t rep[10] = {0};
        rep[0] = 1; rep[1] = 0x81;
        uint8_t f[32];
        const size_t n = make_frame(f, 0x10, rep, 10);
        feed(f, n);
        uint32_t value = 0;
        CHECK(!tang_fpga_read32(0x140u, &value));
    }

    /* ------------------------------------------------- extended write32 --
     * Opcode 2, the value at payload bytes 8..11. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        uint8_t rep[15] = {0};
        rep[0] = 1; rep[1] = 0x82; rep[2] = 0;
        uint8_t f[32];
        const size_t n = make_frame(f, 0x10, rep, 15);
        feed(f, n);
        CHECK(tang_fpga_write32(0x17cu, 0x00000008u));
        CHECK_EQ(wire_len, 18);
        CHECK_EQ(wire[5], 2);           /* opcode: write32 */
        CHECK_EQ(wire[11], 0x7c);       /* address low byte */
        /* the value, big-endian, at wire 12..15 */
        CHECK_EQ(wire[12], 0x00);
        CHECK_EQ(wire[13], 0x00);
        CHECK_EQ(wire[14], 0x00);
        CHECK_EQ(wire[15], 0x08);
    }

    /* A refused write is false, and the status is what says why. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        uint8_t rep[15] = {0};
        rep[0] = 1; rep[1] = 0x82; rep[2] = 4;
        uint8_t f[32];
        const size_t n = make_frame(f, 0x10, rep, 15);
        feed(f, n);
        CHECK(!tang_fpga_write32(0x123u, 1u));
    }

    /* A register access while another task holds the link fails at once
     * rather than blocking; that is the bounded acquisition's whole point. */
    reset_world();
    (void)tang_fpga_link_open();
    current_task = (TaskHandle_t)1;
    tang_fpga_lock();
    current_task = (TaskHandle_t)2;
    {
        uint32_t value = 0;
        CHECK(!tang_fpga_read32(0x140u, &value));
    }
    current_task = (TaskHandle_t)1;
    tang_fpga_unlock();
    CHECK_EQ(wire_len, 0);              /* nothing was sent */

    /* ------------------------------------------------------- baud switch -- */
    reset_world();
    (void)tang_fpga_link_open();
    CHECK_EQ(tang_fpga_set_baud(5000000u), 0);
    CHECK_EQ(tang_fpga_baud(), 5000000u);
    CHECK_EQ(uart_deinits, 1);          /* the divider moved under a de-init */
    CHECK_EQ(uart_inited, 2);
    CHECK_EQ(uart_baud_seen, 5000000u);
    /* Before the link is open there is nothing to switch. */
    reset_world();
    CHECK_EQ(tang_fpga_set_baud(5000000u), -1);

    /* --------------------------------------------------------- the pipe -- */
    reset_world();
    (void)tang_fpga_link_open();
    CHECK(!tang_fpga_pipe_open());
    s_pipe = true;
    CHECK(tang_fpga_pipe_open());
    /* While a pipe owns the ring, decoding must leave the bytes to it. */
    {
        uint8_t f[16];
        const size_t n = make_frame(f, FPGA_RESP_JOYPAD, (const uint8_t *)"\x01\x02\x03\x04", 4);
        feed(f, n);
        rx_decode();
        /* Nothing was consumed: the bytes are the pipe's to read. */
        CHECK_EQ(s_rx_tail, 0);
        CHECK_EQ(s_rx_head, 8);
    }
    s_pipe = false;
    reset_world();

    /* ------------------------------------------------ joypad, drain, drops -- */
    reset_world();
    (void)tang_fpga_link_open();
    {
        uint8_t f[16];
        const size_t n = make_frame(f, FPGA_RESP_JOYPAD,
                                    (const uint8_t *)"\x34\x12\x78\x56", 4);
        feed(f, n);
        uint16_t j1 = 0, j2 = 0;
        tang_fpga_joypad(&j1, &j2);
        /* The frame carries each word big-endian (fpga_frames.c), so bytes
         * 34 12 are 0x3412 and not 0x1234. */
        CHECK_EQ(j1, 0x3412);
        CHECK_EQ(j2, 0x7856);
    }

    /* A full ring drops bytes and counts them, rather than overwriting the
     * oldest: a save that comes back short has to say which buffer lost them. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        static uint8_t flood[FPGA_RX_RING_SIZE + 64];
        memset(flood, 0x55, sizeof flood);
        feed(flood, sizeof flood);
        CHECK(s_rx_drops > 0);
        /* And the drain clears what is left. */
        tang_fpga_drain();
        CHECK_EQ(s_rx_head, s_rx_tail);
    }

    /* Critical sections are left balanced: an unbalanced one would leave
     * interrupts off, which on this board stops the link's own receive. */
    CHECK_EQ(critical_depth, 0);
    CHECK_EQ(critical_imbalance, 0);

    /* ------------------------------------------------ fpgapipe's overlay -- */
    /* A host loader ends with an overlay frame of its own; the OSD module's
     * record has to follow it, under the lock, or the next L / HOME tap after
     * a load does nothing.  The bytes still go to the core unchanged. */
    reset_world();
    (void)tang_fpga_link_open();
    {
        static unsigned char script[2048];
        size_t n = 0;
        const unsigned char reset_on[] = { 0xAA, 0x00, 0x02, 0x06, 0x01 };
        memcpy(&script[n], reset_on, sizeof reset_on); n += sizeof reset_on;
        /* A ROM chunk longer than one pipe read, full of overlay-on lookalikes. */
        script[n++] = 0xAA; script[n++] = 0x04; script[n++] = 0x01; script[n++] = 0x07;
        static const unsigned char fake[5] = { 0xAA, 0x00, 0x02, 0x08, 0x01 };
        for (size_t i = 0; i < 0x400; i++) {
            script[n++] = fake[i % 5];
        }
        const unsigned char tail[] = { 0xAA, 0x00, 0x02, 0x06, 0x00,
                                       0xAA, 0x00, 0x02, 0x08, 0x00 };
        memcpy(&script[n], tail, sizeof tail); n += sizeof tail;

        osd_notes = 0; osd_noted = true; osd_noted_locked = 0;
        host_script = script; host_script_len = n; host_script_at = 0;
        const int rc = cmd_fpgapipe(NULL, 1, NULL);
        host_script = NULL;
        CHECK_EQ(rc, 0);
        CHECK_EQ(wire_len, n);
        CHECK(memcmp(wire, script, n) == 0);
        CHECK_EQ(osd_notes, 1);
        CHECK(!osd_noted);
        CHECK_EQ(osd_noted_locked, 1);
        CHECK_EQ(mutex_held, 0);

        /* Someone else's protocol: nothing in it is believed. */
        static const unsigned char foreign[] = { 0x55, 0xAA, 0x00, 0x02, 0x08, 0x01 };
        wire_len = 0;
        osd_notes = 0;
        host_script = foreign; host_script_len = sizeof foreign; host_script_at = 0;
        CHECK_EQ(cmd_fpgapipe(NULL, 1, NULL), 0);
        host_script = NULL;
        CHECK_EQ(wire_len, sizeof foreign);
        CHECK_EQ(osd_notes, 0);
    }

    /* ------------------------------------------------------- registration -- */
    reset_world();
    CHECK_EQ(tdsh_bl616_fpga_register(), 0);
    CHECK(registered >= 7);
    CHECK(reg_cmds != NULL);

    if (failures) {
        printf("tb_tang_fpga_uart: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_tang_fpga_uart: all checks passed\n");
    return 0;
}
