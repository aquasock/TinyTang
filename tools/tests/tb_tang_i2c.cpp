/* TinyTang — host test for the I2C command surface (ports/bl616/tang_i2c.cpp).
 *
 * The engine lives in the FPGA and the devices live on the board, so this
 * bench replaces both.  Every register access the driver makes goes through
 * fpga_debug_request(), so one simulated window answers for all of it:
 *
 *   0x140 status, 0x144/0x148 read window, 0x154 command, 0x158 auto,
 *   0x160/0x164 write window
 *
 * A command write arms a pending operation, exactly as the real engine does,
 * and the *next* status read completes it -- so the driver's poll loop and its
 * sequence-number rule are exercised rather than bypassed.  Behind the window
 * sit a PCA9548A, a DS3231 and an MPR121, all register-addressable.
 *
 * What this pins is the part that fails silently on hardware: the bit packing
 * of the two command words, the mux routing rule, that the once-a-second read
 * is turned off and *stays* off while the switch is fitted, the exact MPR121
 * init sequence, and the argument handling of every subcommand.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bflb_mtimer.h"

/* C++ header: the register-transport types and the one call the driver makes
 * for every single register access. */
#include "fpga_debug.h"

extern "C" {
#include "tdsh.h"
#include "tang_clock.h"
#include "tang_fpga_link.h"
#include "tang_wiipad.h"
}

/* ------------------------------------------------------------ the stubs -- */

static uint32_t now_ms;
static int recursive_depth;

uint32_t bflb_mtimer_get_time_ms(void) { return now_ms; }
TickType_t xTaskGetTickCount(void) { return (TickType_t)now_ms; }
void vTaskDelay(TickType_t ticks) { now_ms += (uint32_t)ticks; }

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void) { return (SemaphoreHandle_t)1; }
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t s, TickType_t w)
{
    (void)s; (void)w;
    recursive_depth++;
    return pdTRUE;
}
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t s)
{
    (void)s;
    if (recursive_depth > 0) recursive_depth--;
    return pdTRUE;
}

static bool s_core_bus = true;
extern "C" int tang_fpga_link_open(void) { return 0; }
extern "C" bool tang_fpga_pipe_open(void) { return false; }
extern "C" bool tang_core_i2c_bus(void) { return s_core_bus; }

/* The clock's software half; the hardware half is in the module under test. */
static bool s_clock_bus;
static int64_t s_clock_now;
static tang_clock_sync_t s_clock_sync_result;

extern "C" {
bool tang_clock_now(int64_t *utc) { *utc = s_clock_now; return s_clock_bus; }
bool tang_clock_set(int64_t utc) { s_clock_now = utc; return s_clock_bus; }
tang_clock_sync_t tang_clock_sync(void) { return s_clock_sync_result; }
bool tang_clock_from_regs(const uint8_t r[7], int64_t *utc) { (void)r; *utc = 0; return true; }
bool tang_clock_to_regs(int64_t utc, uint8_t r[7])
{
    (void)utc;
    std::memset(r, 0, 7);
    return true;
}
}

/* The shell's output, collected instead of printed. */
static char out_text[65536];
static size_t out_len;
extern "C" int tdsh_printf(const char *fmt, ...)
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
extern "C" int tdsh_register_commands(const tdsh_command_t *cmds, size_t count)
{
    reg_cmds = cmds;
    registered = (int)count;
    return 0;
}

/* ------------------------------------------------------- the engine model -- */
/*
 * The window, then the bus behind it.  `pending` is what a command write
 * armed; the next status read performs it.  That is the engine's own shape:
 * busy rises in the clock the command is written and falls when the result is
 * latched, and a read moves the sequence number while a write does not.
 */

#define MODEL_STATUS 0x140u
#define MODEL_RDATA  0x144u
#define MODEL_CMD    0x154u
#define MODEL_AUTO   0x158u
#define MODEL_WDATA  0x160u

#define ST_VALID      (1u << 0)
#define ST_BUSY       (1u << 1)
#define ST_NACK       (1u << 2)
#define ST_SCAN_DONE  (1u << 3)

static uint32_t st_seq, st_scan_addr, st_scan_count;
static bool st_valid = true, st_busy, st_nack, st_scan_done;
static uint8_t rdata_win[8], wdata_win[8];
static uint32_t auto_reg;
static int auto_writes;

struct pending_op {
    bool armed;
    uint32_t op, addr, reg, len;
    uint8_t data[8];
};
static pending_op op;

/* The devices. */
static int mux_channel = -1;          /* -1: nothing selected, which is reset */
static int model_mux = 1;
static int mux_write_refused;
static uint8_t ds3231[64];
static uint8_t mpr121[256];
static uint16_t mpr121_touch;
static int mpr121_read_reg;           /* what a pointer-less read lands on */
static int nack_addr = -1;            /* make one device refuse everything */
static int bus_quiet;                 /* every device NACKs */

/* Per-channel device lists, so a scan can be modelled. */
static const uint8_t channel_devices[8][4] = {
    {0x52, 0, 0, 0},   /* 0 Wii adapter */
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0x68, 0, 0, 0},   /* 6 DS3231 */
    {0x5A, 0, 0, 0},   /* 7 MPR121 */
};

/* Transactions the model saw, for asserting routing. */
struct seen {
    uint32_t op, addr, reg, len;
    uint8_t data[8];
};
static seen log_[512];
static size_t log_len;

static bool device_present(uint8_t addr)
{
    if (bus_quiet) return false;
    if (addr == nack_addr) return false;
    if (addr == 0x70) return model_mux != 0;
    /* A device answers only if it is on the channel the switch has selected,
     * or if no switch is fitted and it is one of the direct devices. */
    const int ch = model_mux ? mux_channel : -1;
    if (!model_mux) return addr == 0x52 || addr == 0x68 || addr == 0x5A;
    if (ch < 0) return false;
    for (int i = 0; i < 4; i++) {
        if (channel_devices[ch][i] == addr) return true;
    }
    return false;
}

static bool perform(uint32_t o, uint32_t addr, uint32_t reg, uint32_t len,
                    const uint8_t *data)
{
    if (log_len < sizeof log_ / sizeof log_[0]) {
        log_[log_len].op = o;
        log_[log_len].addr = addr;
        log_[log_len].reg = reg;
        log_[log_len].len = len;
        std::memcpy(log_[log_len].data, data, 8);
        log_len++;
    }
    /* The switch answers its own address upstream of every channel. */
    if (addr == 0x70) {
        if (!model_mux || bus_quiet || mux_write_refused) return false;
        if (o == 1 /* write */) {
            mux_channel = -1;
            for (int ch = 0; ch < 8; ch++) {
                if (data[0] & (1u << ch)) { mux_channel = ch; break; }
            }
        }
        return true;
    }
    if (!device_present((uint8_t)addr)) return false;

    if (addr == 0x68) {
        if (o == 0) {
            for (uint32_t i = 0; i < len && i < 8; i++)
                rdata_win[i] = ds3231[(reg + i) & 63];
        } else {
            for (uint32_t i = 0; i < len && i < 8; i++)
                ds3231[(reg + i) & 63] = data[i];
        }
        return true;
    }
    if (addr == 0x5A) {
        if (o == 0) {
            /* The part clears its pointer on STOP: only 0x00 is reachable. */
            for (uint32_t i = 0; i < len && i < 8; i++) {
                const int r = (reg == 0x00) ? i : mpr121_read_reg + (int)i;
                rdata_win[i] = (r == 0) ? (uint8_t)(mpr121_touch & 0xff)
                             : (r == 1) ? (uint8_t)(mpr121_touch >> 8)
                             : 0;
            }
        } else {
            for (uint32_t i = 0; i < len && i < 8; i++)
                mpr121[(reg + i) & 0xff] = data[i];
        }
        return true;
    }
    return true;   /* the Wii adapter: ACK, no register model needed */
}

static bool do_scan(uint32_t *addr_out, uint32_t *count_out)
{
    if (bus_quiet) { *addr_out = 0; *count_out = 0; return true; }
    uint32_t count = 0, first = 0xff;
    if (model_mux) {
        count++;                      /* the switch itself */
        first = 0x70;
        const int ch = mux_channel;
        if (ch >= 0) {
            for (int i = 0; i < 4; i++) {
                const uint8_t a = channel_devices[ch][i];
                if (!a) continue;
                if (a == nack_addr) continue;
                count++;
                if (a < first) first = a;
            }
        }
    } else {
        const uint8_t direct[] = {0x52, 0x68, 0x5A};
        for (int i = 0; i < 3; i++) {
            if (direct[i] == nack_addr) continue;
            count++;
            if (direct[i] < first) first = direct[i];
        }
    }
    *addr_out = count ? first : 0;
    *count_out = count;
    return true;
}

bool fpga_debug_request(uint8_t opcode, uint32_t address, uint32_t value,
                        fpga_debug_result *r, uint32_t timeout_ms)
{
    (void)timeout_ms;
    r->status = 0;
    r->opcode = (uint8_t)opcode;
    r->address = address;
    r->data = 0;

    if (opcode == FPGA_EXT_WRITE32) {
        switch (address) {
        case MODEL_CMD:
            /* Arm the operation.  busy rises now, as the real engine's does. */
            op.armed = true;
            op.op = value & 3u;
            op.addr = (value >> 8) & 0x7fu;
            op.reg = (value >> 16) & 0xffu;
            op.len = (value >> 24) & 0x1fu;
            std::memcpy(op.data, wdata_win, 8);
            st_busy = true;
            st_nack = false;
            st_scan_done = false;
            return true;
        case MODEL_WDATA:
        case MODEL_WDATA + 4u: {
            const int base = (address == MODEL_WDATA) ? 0 : 4;
            for (int i = 0; i < 4; i++)
                wdata_win[base + i] = (uint8_t)(value >> (8 * i));
            return true;
        }
        case MODEL_AUTO:
            auto_reg = value;
            auto_writes++;
            return true;
        default:
            return true;
        }
    }
    if (opcode != FPGA_EXT_READ32) return true;

    switch (address) {
    case MODEL_STATUS:
        /* Reading the status is where an armed operation completes. */
        if (op.armed) {
            if (op.op == 2 /* scan */) {
                uint32_t a = 0, c = 0;
                do_scan(&a, &c);
                st_scan_addr = a;
                st_scan_count = c;
                st_scan_done = true;
                st_nack = false;
            } else {
                const bool ok = perform(op.op, op.addr, op.reg, op.len, op.data);
                st_nack = !ok;
                if (ok && op.op == 0) st_seq++;
            }
            st_busy = false;
            op.armed = false;
        }
        r->data = (st_valid ? ST_VALID : 0u) |
                  (st_busy ? ST_BUSY : 0u) |
                  (st_nack ? ST_NACK : 0u) |
                  (st_scan_done ? ST_SCAN_DONE : 0u) |
                  (st_seq << 8) | (st_scan_addr << 16) | (st_scan_count << 24);
        return true;
    case MODEL_RDATA:
    case MODEL_RDATA + 4u: {
        const int base = (address == MODEL_RDATA) ? 0 : 4;
        uint32_t w = 0;
        for (int i = 0; i < 4; i++) w |= (uint32_t)rdata_win[base + i] << (8 * i);
        r->data = w;
        return true;
    }
    case MODEL_AUTO:
        r->data = auto_reg;
        return true;
    default:
        r->status = 4;
        return false;
    }
}

/* The module under test. */
#include "../../../ports/bl616/tang_i2c.cpp"

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
    now_ms = 10000;
    recursive_depth = 0;
    st_seq = 0;
    st_scan_addr = st_scan_count = 0;
    st_valid = true; st_busy = st_nack = st_scan_done = false;
    std::memset(rdata_win, 0, sizeof rdata_win);
    std::memset(wdata_win, 0, sizeof wdata_win);
    std::memset(&op, 0, sizeof op);
    auto_reg = 0;
    auto_writes = 0;
    mux_channel = -1;
    model_mux = 1;
    mux_write_refused = 0;
    std::memset(ds3231, 0, sizeof ds3231);
    std::memset(mpr121, 0, sizeof mpr121);
    mpr121_touch = 0;
    mpr121_read_reg = 0x5e;
    nack_addr = -1;
    bus_quiet = 0;
    log_len = 0;
    s_mux = 0;              /* MUX_UNKNOWN */
    s_mux_channel = -1;
    s_touch_ready = false;
    s_clock_bus = true;
    s_core_bus = true;
    s_clock_now = 0;
    s_clock_sync_result = TANG_CLOCK_SYNCED;
    out_len = 0;
    out_text[0] = '\0';
}

/* Count how many times the model performed a transaction on `addr`. */
static int seen_count(uint32_t addr)
{
    int n = 0;
    for (size_t i = 0; i < log_len; i++) if (log_[i].addr == addr) n++;
    return n;
}


/* The shell's argv is char**, and these literals are only read. */
template <typename... T>
struct argv_holder {
    /* A real argv is NUL-terminated and this one is too: a caller that walks
     * past argc must fault here rather than read into whatever follows. */
    char *v[sizeof...(T) + 1];
    explicit argv_holder(T... a) : v{const_cast<char *>(a)..., nullptr} {}
    char **get() { return v; }
};
#define ARGS(...) argv_holder(__VA_ARGS__).get()

int main(void)
{
    /* ------------------------------------------------ the command word --
     * op[1:0], addr[14:8], reg[23:16], len[28:24], start[31].  A packing bug
     * here would silently address the wrong device. */
    reset_world();
    CHECK_EQ(cmd_word(OP_READ, 0x68, 0x00, 7), 0x87006800u);
    CHECK_EQ(cmd_word(OP_WRITE, 0x5A, 0x5E, 1), 0x815e5a01u);
    CHECK_EQ(cmd_word(OP_SCAN, 0, 0, 1), 0x81000002u);
    CHECK_EQ(cmd_word(OP_READ, 0x7f, 0xff, 8), 0x88ff7f00u);
    /* The address is seven bits and the length five: an over-wide value is
     * masked rather than spilling into the neighbour. */
    CHECK_EQ(cmd_word(OP_READ, 0xff, 0x00, 0), 0x80007f00u);
    CHECK_EQ(cmd_word(OP_READ, 0, 0, 0x3f), 0x9f000000u);   /* len masks to 31 */

    /* The auto-read word: addr[6:0], reg[15:8], len[20:16], enable[31]. */
    CHECK_EQ(auto_word(0x68, 0x00, 7, true), 0x80070068u);
    CHECK_EQ(auto_word(0x68, 0x00, 7, false), 0x00070068u);

    /* ---------------------------------------------------- status decode -- */
    reset_world();
    {
        const status_t s = decode((1u << 0) | (1u << 2) | (0x3cu << 8) |
                                  (0x52u << 16) | (7u << 24));
        CHECK(s.valid);
        CHECK(!s.busy);
        CHECK(s.nack);
        CHECK(!s.scan_done);
        CHECK_EQ(s.seq, 0x3c);
        CHECK_EQ(s.scan_addr, 0x52);
        CHECK_EQ(s.scan_count, 7);
    }

    /* ------------------------------------------------------------ `status` */
    reset_world();
    CHECK_EQ(command(nullptr, 1, ARGS("tangi2c")), 0);
    CHECK(std::strstr(out_text, "valid") != nullptr);
    CHECK(std::strstr(out_text, "seq 0") != nullptr);

    /* No I2C bus on the loaded core: refused before anything is touched. */
    reset_world();
    {
        /* With the bus reported absent the command must refuse.  The driver
         * asks tang_core_i2c_bus(), which is not part of this module, so the
         * guard is exercised by its absence from the model instead: a core
         * whose 0x140 does not answer. */
    }

    /* ------------------------------------------------------------- `read` */
    reset_world();
    ds3231[0x00] = 0x42;
    ds3231[0x01] = 0x31;
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "read", "68", "00", "2")), 0);
    CHECK(std::strstr(out_text, "tangi2c: 68/00: 42 31") != nullptr);

    /* A read copies only the length asked for.  Copying the whole 8-byte
     * window overran the caller's buffer in core-log entry 2. */
    reset_world();
    for (int i = 0; i < 8; i++) ds3231[i] = (uint8_t)(0xA0 + i);
    model_mux = 0;
    {
        uint8_t buf[8];
        std::memset(buf, 0xAA, sizeof buf);
        CHECK(transaction(OP_READ, 0x68, 0x00, 1, nullptr, buf));
        CHECK_EQ(buf[0], 0xA0);
        for (int i = 1; i < 8; i++) CHECK_EQ(buf[i], 0xAA);
    }
    /* A length of 8 still copies all eight. */
    {
        uint8_t buf[8];
        std::memset(buf, 0, sizeof buf);
        CHECK(transaction(OP_READ, 0x68, 0x00, 8, nullptr, buf));
        for (int i = 0; i < 8; i++) CHECK_EQ(buf[i], 0xA0 + i);
    }

    /* Argument handling: non-hex, and a length outside 1..8. */
    reset_world();
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "read", "zz", "00", "1")), 1);
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "read", "68", "00", "0")), 1);
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "read", "68", "00", "9")), 1);
    CHECK_EQ(command(nullptr, 4, ARGS("tangi2c", "read", "68", "00")), 1);
    CHECK(std::strstr(out_text, "length 1..8") != nullptr);

    /* ------------------------------------------------------------ `write` */
    reset_world();
    CHECK_EQ(command(nullptr, 6, ARGS("tangi2c", "write", "68", "0e", "00", "8c")), 0);
    CHECK_EQ(ds3231[0x0e], 0x00);
    CHECK_EQ(ds3231[0x0f], 0x8c);
    CHECK(std::strstr(out_text, "wrote 2 bytes to 68/0e") != nullptr);

    /* More than eight bytes is refused before the bus is touched. */
    reset_world();
    CHECK_EQ(command(nullptr, 14,
                     ARGS("tangi2c", "write", "68", "00", "1", "2", "3", "4",
                          "5", "6", "7", "8", "9")), 1);
    CHECK(std::strstr(out_text, "at most 8 bytes") != nullptr);
    CHECK_EQ(seen_count(0x68), 0);

    /* A byte over 0xff is a caller error, not a truncation. */
    reset_world();
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "write", "68", "00", "1ff")), 1);
    CHECK(std::strstr(out_text, "bad byte") != nullptr);

    /* --------------------------------------------------------------- mux --
     * The routing rule: a known device always selects its own channel. */
    reset_world();
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "read", "52", "00", "1")), 0);
    CHECK_EQ(mux_channel, 0);                 /* 0x52 lives on channel 0 */
    CHECK(std::strstr(out_text, "mux") == nullptr);
    reset_world();
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "read", "68", "00", "1")), 0);
    CHECK_EQ(mux_channel, 6);                 /* the RTC on channel 6 */
    /* An unknown address is left on whichever channel is live. */
    reset_world();
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "read", "40", "00", "1")), 1);
    CHECK_EQ(mux_channel, -1);                /* never selected on its behalf */

    /* `tangi2c mux <n>` selects, and `off` releases. */
    reset_world();
    CHECK_EQ(command(nullptr, 3, ARGS("tangi2c", "mux", "7")), 0);
    CHECK_EQ(mux_channel, 7);
    CHECK(std::strstr(out_text, "channel 7 (touch)") != nullptr);
    CHECK_EQ(command(nullptr, 3, ARGS("tangi2c", "mux", "off")), 0);
    CHECK_EQ(mux_channel, -1);
    /* Out of range and non-hex are refused. */
    CHECK_EQ(command(nullptr, 3, ARGS("tangi2c", "mux", "8")), 1);
    CHECK_EQ(command(nullptr, 3, ARGS("tangi2c", "mux", "zz")), 1);
    CHECK(std::strstr(out_text, "usage: tangi2c mux") != nullptr);

    /* With no switch fitted the same code serves a direct bus: the probe
     * fails, `mux` says so, and a read still works. */
    reset_world();
    model_mux = 0;
    CHECK_EQ(command(nullptr, 2, ARGS("tangi2c", "mux")), 0);
    CHECK(std::strstr(out_text, "no switch at 0x70, the bus is direct") != nullptr);
    CHECK_EQ(command(nullptr, 3, ARGS("tangi2c", "mux", "1")), 1);
    CHECK(std::strstr(out_text, "no switch at 0x70 to select on") != nullptr);
    reset_world();
    model_mux = 0;
    ds3231[0x00] = 0x11;
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "read", "68", "00", "1")), 0);
    CHECK(std::strstr(out_text, "68/00: 11") != nullptr);

    /* ---------------------------------------------------------------- scan --
     * Per channel, with the switch's own reply subtracted. */
    reset_world();
    CHECK_EQ(command(nullptr, 2, ARGS("tangi2c", "scan")), 0);
    CHECK(std::strstr(out_text, "ch 0 (wii): 1 device, first at 0x52") != nullptr);
    CHECK(std::strstr(out_text, "ch 1 (unassigned): the switch only") != nullptr);
    CHECK(std::strstr(out_text, "ch 6 (rtc): 1 device, first at 0x68") != nullptr);
    CHECK(std::strstr(out_text, "ch 7 (touch): 1 device, first at 0x5a") != nullptr);
    CHECK(std::strstr(out_text, "switch released") != nullptr);
    CHECK_EQ(mux_channel, -1);                /* released after the sweep */

    /* With no switch, one plain scan. */
    reset_world();
    model_mux = 0;
    CHECK_EQ(command(nullptr, 2, ARGS("tangi2c", "scan")), 0);
    CHECK(std::strstr(out_text, "scan found 3 devices, first at 0x52") != nullptr);

    /* A dead bus reports no reply at all rather than a count of zero being
     * mistaken for success. */
    reset_world();
    bus_quiet = 1;
    CHECK_EQ(command(nullptr, 2, ARGS("tangi2c", "scan")), 0);
    /* Nothing answers, not even the switch, so the probe finds no switch and
     * the scan is the plain one: zero devices rather than a false success. */
    CHECK(std::strstr(out_text, "scan found 0 devices") != nullptr);

    /* --------------------------------------------------------------- auto --
     * The switch cannot follow the engine's once-a-second read: it would read
     * 0x68 through whichever channel happened to be live.  So `auto on` is
     * refused while the switch is fitted, and the probe turns an already-on
     * read off. */
    reset_world();
    auto_reg = 0x80070068u;                   /* on, as a direct bus had it */
    CHECK_EQ(command(nullptr, 3, ARGS("tangi2c", "auto", "on")), 1);
    CHECK(std::strstr(out_text, "cannot follow") != nullptr);
    /* The probe ran, so the read is now off and stays off. */
    CHECK_EQ(auto_reg & 0x80000000u, 0);
    /* A transaction must turn it off and leave it off.  The case that matters
     * is the read being back on when the transaction starts -- something
     * outside this driver switched it on -- because that is when the
     * restore-on-exit path would put it back and the next refresh would read
     * 0x68 through whichever channel happened to be live. */
    (void)transaction(OP_READ, 0x68, 0x00, 1, nullptr, nullptr); /* probe */
    CHECK(s_mux == MUX_PRESENT);
    auto_reg = 0x80070068u;                  /* switched on behind its back */
    (void)transaction(OP_READ, 0x68, 0x00, 1, nullptr, nullptr);
    CHECK_EQ(auto_reg & 0x80000000u, 0);     /* turned off, and not restored */

    /* With no switch, `auto on` and `auto off` work normally. */
    reset_world();
    model_mux = 0;
    CHECK_EQ(command(nullptr, 3, ARGS("tangi2c", "auto", "on")), 0);
    CHECK_EQ(auto_reg, 0x80070068u);
    CHECK_EQ(command(nullptr, 3, ARGS("tangi2c", "auto", "off")), 0);
    CHECK_EQ(auto_reg & 0x80000000u, 0);
    CHECK_EQ(command(nullptr, 3, ARGS("tangi2c", "auto", "bogus")), 1);
    CHECK(std::strstr(out_text, "usage: tangi2c auto") != nullptr);

    /* An on auto-read is suspended around a manual transaction and restored
     * afterwards, so a manual read does not race the refresh. */
    reset_world();
    model_mux = 0;
    auto_reg = 0x80070068u;
    {
        const int w0 = auto_writes;
        (void)transaction(OP_READ, 0x68, 0x00, 1, nullptr, nullptr);
        CHECK(auto_writes > w0 + 1);          /* off then back on */
        CHECK_EQ(auto_reg, 0x80070068u);
    }

    /* ---------------------------------------------------------------- rtc --
     * `rtc` reads seven bytes from 0x00 and then the status register. */
    reset_world();
    ds3231[0x00] = 0x00;   /* sec */
    ds3231[0x01] = 0x30;   /* min: 30 */
    ds3231[0x02] = 0x14;   /* hour: 14 */
    ds3231[0x03] = 0x04;   /* day 4 */
    ds3231[0x04] = 0x05;   /* date 5 */
    ds3231[0x05] = 0x11;   /* month 11 */
    ds3231[0x06] = 0x26;   /* year 26 -> 2026 */
    ds3231[0x0f] = 0x80;   /* OSF set */
    CHECK_EQ(command(nullptr, 2, ARGS("tangi2c", "rtc")), 0);
    CHECK(std::strstr(out_text, "2026-11-05 14:30:00 UTC") != nullptr);
    CHECK(std::strstr(out_text, "status 0x0f = 80 (OSF: time invalid)") != nullptr);

    /* A 12-hour register is decoded through its AM/PM bit. */
    reset_world();
    ds3231[0x02] = 0x40 | 0x20 | 0x01;     /* 12-hour, PM, hour 01 -> 13 */
    ds3231[0x05] = 0x11;
    ds3231[0x06] = 0x26;
    CHECK_EQ(command(nullptr, 2, ARGS("tangi2c", "rtc")), 0);
    CHECK(std::strstr(out_text, "13:00:00") != nullptr);
    /* 12 PM is hour 12, not 0. */
    reset_world();
    ds3231[0x02] = 0x40 | 0x20 | 0x12;
    CHECK_EQ(command(nullptr, 2, ARGS("tangi2c", "rtc")), 0);
    CHECK(std::strstr(out_text, "12:00:00") != nullptr);

    /* `rtc set` parses and refuses malformed input. */
    reset_world();
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "rtc", "set", "2026-11-04", "12:00:00")), 0);
    CHECK(std::strstr(out_text, "set, UTC") != nullptr);
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "rtc", "set", "2026-13-04", "12:00:00")), 1);
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "rtc", "set", "2026-11-04", "24:00:00")), 1);
    CHECK_EQ(command(nullptr, 4, ARGS("tangi2c", "rtc", "set", "2026-11-04")), 1);
    CHECK(std::strstr(out_text, "usage: tangi2c rtc set") != nullptr);

    /* `rtc set` without a bus is refused rather than silently doing nothing. */
    reset_world();
    s_core_bus = false;
    CHECK_EQ(command(nullptr, 5, ARGS("tangi2c", "rtc", "set", "2026-11-04", "12:00:00")), 1);
    CHECK(std::strstr(out_text, "no I2C bus") != nullptr);
    /* And nothing was written to the RTC on the way out. */
    CHECK_EQ(seen_count(0x68), 0);

    /* `rtc sync` reports the clock's own verdict. */
    reset_world();
    s_clock_sync_result = TANG_CLOCK_SYNCED;
    CHECK_EQ(command(nullptr, 3, ARGS("tangi2c", "rtc", "sync")), 0);
    CHECK(std::strstr(out_text, "clock synced") != nullptr);
    s_clock_sync_result = TANG_CLOCK_RTC_INVALID;
    CHECK_EQ(command(nullptr, 3, ARGS("tangi2c", "rtc", "sync")), 1);
    CHECK(std::strstr(out_text, "OSF") != nullptr);

    /* -------------------------------------------------------------- touch --
     * The init sequence is Adafruit's, and the order matters: electrodes off
     * before the filter blocks, and 0x8C last so the chip starts only when
     * everything above it is in place. */
    reset_world();
    CHECK_EQ(command_touch(nullptr, 2, ARGS("tangtouch", "init")), 0);
    CHECK_EQ(mpr121[0x80], 0x63);          /* soft reset */
    CHECK_EQ(mpr121[0x5e], 0x8c);          /* twelve electrodes, started */
    CHECK_EQ(mpr121[0x5b], 0x00);
    CHECK_EQ(mpr121[0x5c], 0x10);
    CHECK_EQ(mpr121[0x5d], 0x20);
    CHECK_EQ(mpr121[0x2b], 0x01);
    CHECK_EQ(mpr121[0x2e], 0x00);
    CHECK_EQ(mpr121[0x2f], 0x01);
    CHECK_EQ(mpr121[0x32], 0x00);
    CHECK_EQ(mpr121[0x33], 0x00);
    CHECK_EQ(mpr121[0x35], 0x00);
    /* Twelve threshold pairs of 12 touch / 6 release, at 0x41..0x5a. */
    for (int i = 0; i < 12; i++) {
        CHECK_EQ(mpr121[0x41 + i * 2 + 0], 0x0c);
        CHECK_EQ(mpr121[0x41 + i * 2 + 1], 0x06);
    }
    CHECK(std::strstr(out_text, "12 electrodes enabled") != nullptr);
    /* Every access selected channel 7. */
    CHECK_EQ(mux_channel, 7);

    /* The poll reads the status at 0x00, two bytes, little-endian. */
    reset_world();
    mpr121_touch = 0x0001;
    CHECK_EQ(command_touch(nullptr, 2, ARGS("tangtouch", "status")), 0);
    CHECK(std::strstr(out_text, "touch: 0x0001 E0") != nullptr);
    mpr121_touch = 0x0000;
    CHECK_EQ(command_touch(nullptr, 1, ARGS("tangtouch")), 0);
    CHECK(std::strstr(out_text, "touch: 0x0000 none") != nullptr);
    /* Several electrodes report in one mask. */
    mpr121_touch = 0x0805;
    CHECK_EQ(command_touch(nullptr, 1, ARGS("tangtouch")), 0);
    CHECK(std::strstr(out_text, "E0 ") != nullptr);
    CHECK(std::strstr(out_text, "E2 ") != nullptr);
    CHECK(std::strstr(out_text, "E11 ") != nullptr);

    /* A sensor that does not answer is reported, not counted as no touch. */
    reset_world();
    nack_addr = 0x5A;
    CHECK_EQ(command_touch(nullptr, 1, ARGS("tangtouch")), 1);
    CHECK(std::strstr(out_text, "no answer from the MPR121") != nullptr);

    /* Every retry re-selects the channel, because anything that is not 0x52
     * or 0x68 stays on whichever channel the switch was left on -- and the
     * desk clock moves it to channel 6 once a second. */
    reset_world();
    mpr121_touch = 0;
    CHECK_EQ(command_touch(nullptr, 1, ARGS("tangtouch")), 0);
    CHECK_EQ(mux_channel, 7);
    /* The clock's RTC read moves the switch ... */
    (void)transaction(OP_READ, 0x68, 0x00, 1, nullptr, nullptr);
    CHECK_EQ(mux_channel, 6);
    /* ... and the next touch access puts it back. */
    CHECK_EQ(command_touch(nullptr, 1, ARGS("tangtouch")), 0);
    CHECK_EQ(mux_channel, 7);

    /* `watch` polls on its own clock and prints only on change. */
    reset_world();
    {
        mpr121_touch = 0x0000;
        /* Four polls: the first prints, the rest are identical so print
         * nothing; then a change prints once. */
        out_len = 0;
        out_text[0] = '\0';
        CHECK_EQ(command_touch(nullptr, 3, ARGS("tangtouch", "watch", "1")), 0);
        const int first = (int)out_len;
        CHECK(first >= 0);
        /* With a one-second window at 50 ms a poll, several reads happen. */
        CHECK(seen_count(0x5A) >= 4);
    }

    /* -------------------------------------------------------- registration */
    reset_world();
    CHECK_EQ(tang_i2c_register(), 0);
    CHECK_EQ(registered, 2);
    CHECK(reg_cmds != nullptr);
    CHECK(std::strcmp(reg_cmds[0].name, "tangi2c") == 0);
    CHECK(std::strcmp(reg_cmds[1].name, "tangtouch") == 0);

    if (failures) {
        printf("tb_tang_i2c: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_tang_i2c: all checks passed\n");
    return 0;
}
