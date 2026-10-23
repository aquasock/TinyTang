// TinyTang — the desktop core's I2C bus on the USB-A port, from the shell.
//
// The FPGA's engine (fpga/desktop/i2c_master.sv) runs one transaction at a
// time: a device address, a register, 1..16 bytes and a direction.  The bus
// is the populated USB-A port's data pair, SDA on D+ and SCL on D-, through
// the same STEMMA QT chain as the Wii controller adapter, from desktop ABI
// 1.5 on.  The first client is the desktop clock's RTC (Adafruit DS3231 at
// 0x68); `tangi2c scan` is how a new device is found.  The engine also
// refreshes the RTC once a second when `tangi2c auto on` has configured the
// address.
// The desktop's wall clock (tang_clock.c) reads the RTC through the hooks at
// the end of this file, so every transaction holds one recursive bus lock: the
// clock's sync and a shell command must not interleave one transaction's
// register steps.
//
// Register map (desktop_regs):
//   0x140 status: valid, busy, nack, scan_done, seq, scan_addr, scan_count
//   0x144..0x150 read window, 16 bytes
//   0x154 command: op[1:0], addr[14:8], reg[23:16], len[28:24], start[31]
//   0x158 auto:    addr[6:0], reg[15:8], len[20:16], enable[31]
//   0x160..0x16c write window, 16 bytes
//
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "tdsh.h"
#include "bflb_mtimer.h"
#include "tang_clock.h"
#include "tang_fpga_link.h"
int tdsh_printf(const char *fmt, ...);
bool tang_core_i2c_bus(void);
}

#include "fpga_debug.h"

namespace {

constexpr uint32_t REG_STATUS = 0x140;
constexpr uint32_t REG_RDATA  = 0x144;
constexpr uint32_t REG_CMD    = 0x154;
constexpr uint32_t REG_AUTO   = 0x158;
constexpr uint32_t REG_WDATA  = 0x160;

constexpr uint32_t OP_READ  = 0;
constexpr uint32_t OP_WRITE = 1;
constexpr uint32_t OP_SCAN  = 2;

constexpr uint8_t DS3231 = TANG_CLOCK_RTC_ADDR;

SemaphoreHandle_t s_bus;

// Held for one whole transaction or command; recursive, because `tangi2c rtc
// set` holds it and calls into the clock, which takes it again.
struct bus_lock {
    bus_lock() { if (s_bus) xSemaphoreTakeRecursive(s_bus, portMAX_DELAY); }
    ~bus_lock() { if (s_bus) xSemaphoreGiveRecursive(s_bus); }
};

bool read32(uint32_t address, uint32_t *value)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        fpga_debug_result r;
        if (fpga_debug_request(FPGA_EXT_READ32, address, 0, &r) && r.status == 0) {
            *value = r.data;
            return true;
        }
    }
    return false;
}

bool write32(uint32_t address, uint32_t value)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        fpga_debug_result r;
        if (fpga_debug_request(FPGA_EXT_WRITE32, address, value, &r) && r.status == 0) {
            return true;
        }
    }
    return false;
}

struct status_t {
    bool valid, busy, nack, scan_done;
    unsigned seq, scan_addr, scan_count;
};

status_t decode(uint32_t w)
{
    status_t s;
    s.valid      = (w & (1u << 0)) != 0;
    s.busy       = (w & (1u << 1)) != 0;
    s.nack       = (w & (1u << 2)) != 0;
    s.scan_done  = (w & (1u << 3)) != 0;
    s.seq        = (w >> 8) & 0xff;
    s.scan_addr  = (w >> 16) & 0xff;
    s.scan_count = (w >> 24) & 0xff;
    return s;
}

bool get_status(status_t *s)
{
    uint32_t raw;
    if (!read32(REG_STATUS, &raw)) {
        return false;
    }
    *s = decode(raw);
    return true;
}

uint32_t cmd_word(uint32_t op, uint32_t addr, uint32_t reg, uint32_t len)
{
    return (op & 3u) | ((addr & 0x7fu) << 8) | ((reg & 0xffu) << 16) |
           ((len & 0x1fu) << 24) | (1u << 31);
}

uint32_t auto_word(uint32_t addr, uint32_t reg, uint32_t len, bool enable)
{
    return (addr & 0x7fu) | ((reg & 0xffu) << 8) | ((len & 0x1fu) << 16) |
           (enable ? (1u << 31) : 0u);
}

// A read is done when the sequence number moves (or it was refused); a write
// or a scan does not move it, so it is done when busy falls.  The core raises
// busy in the same clock the command register is written and holds it until
// the result is latched, so the first poll after the command sees it.
bool wait_idle(unsigned prev_seq, bool want_seq, uint32_t timeout_ms, status_t *s)
{
    const TickType_t start = xTaskGetTickCount();
    for (;;) {
        if (!get_status(s)) {
            return false;
        }
        if (!s->busy && (!want_seq || s->seq != prev_seq || s->nack)) {
            return true;
        }
        if ((xTaskGetTickCount() - start) * portTICK_PERIOD_MS >= timeout_ms) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

bool read_window(uint8_t out[8])
{
    for (int i = 0; i < 2; i++) {
        uint32_t w;
        if (!read32(REG_RDATA + 4u * i, &w)) {
            return false;
        }
        out[i * 4 + 0] = (uint8_t)(w);
        out[i * 4 + 1] = (uint8_t)(w >> 8);
        out[i * 4 + 2] = (uint8_t)(w >> 16);
        out[i * 4 + 3] = (uint8_t)(w >> 24);
    }
    return true;
}

bool write_window(const uint8_t in[8])
{
    for (int i = 0; i < 2; i++) {
        const uint32_t w = (uint32_t)in[i * 4] |
                           ((uint32_t)in[i * 4 + 1] << 8) |
                           ((uint32_t)in[i * 4 + 2] << 16) |
                           ((uint32_t)in[i * 4 + 3] << 24);
        if (!write32(REG_WDATA + 4u * i, w)) {
            return false;
        }
    }
    return true;
}

// Turn the once-a-second refresh off before a manual transaction, and back on
// afterwards if it was on.
bool auto_read(uint32_t *word, bool *on)
{
    if (!read32(REG_AUTO, word)) {
        return false;
    }
    *on = (*word & (1u << 31)) != 0;
    return true;
}

bool transaction_body(uint32_t op, uint32_t addr, uint32_t reg, uint32_t len,
                      const uint8_t *wdata, uint8_t *rdata)
{
    if (wdata != nullptr) {
        uint8_t buf[8] = {0};
        std::memcpy(buf, wdata, len > 8 ? 8 : len);
        if (!write_window(buf)) {
            return false;
        }
    }
    status_t before;
    if (!get_status(&before)) {
        return false;
    }
    if (!write32(REG_CMD, cmd_word(op, addr, reg, len))) {
        return false;
    }
    status_t after;
    if (!wait_idle(before.seq, op == OP_READ, 1000, &after)) {
        return false;
    }
    if (op == OP_READ && rdata != nullptr) {
        // The window is always 8 bytes; the caller's buffer is only `len`.
        uint8_t window[8];
        if (!read_window(window)) {
            return false;
        }
        std::memcpy(rdata, window, len > 8 ? 8 : len);
    }
    return !after.nack;
}

bool transaction(uint32_t op, uint32_t addr, uint32_t reg, uint32_t len,
                 const uint8_t *wdata, uint8_t *rdata)
{
    bus_lock lock;
    uint32_t saved_auto;
    bool on = false;
    (void)auto_read(&saved_auto, &on);
    if (on) {
        (void)write32(REG_AUTO, saved_auto & ~(1u << 31));
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    const bool ok = transaction_body(op, addr, reg, len, wdata, rdata);
    // Turned back on whatever the outcome; a failed transaction used to
    // leave the once-a-second read off.
    if (on) {
        (void)write32(REG_AUTO, saved_auto);
    }
    return ok;
}

int bcd(unsigned v)
{
    return (int)((v >> 4) * 10 + (v & 0x0f));
}

void print_status(const status_t &s)
{
    tdsh_printf("tangi2c: %s%s%s seq %u, scan %02x/%u\r\n",
                s.valid ? "valid" : "no data",
                s.busy ? ", busy" : "",
                s.nack ? ", nack" : "",
                s.seq, s.scan_addr, s.scan_count);
}

// The DS3231's time block: 0x00 sec, 0x01 min, 0x02 hour (bit6 12/24),
// 0x03 day, 0x04 date, 0x05 month (bit7 century), 0x06 year.  Whether the time
// is valid is OSF, bit 7 of the status register 0x0F, not anything here.
void print_ds3231(const uint8_t b[7])
{
    const bool twelves = (b[2] & 0x40) != 0;
    unsigned hour = b[2] & 0x3f;
    if (twelves) {
        hour = bcd(hour & 0x1f);
        if (b[2] & 0x20) hour = (hour % 12) + 12;
    } else {
        hour = bcd(hour);
    }
    tdsh_printf("ds3231: %04d-%02d-%02d %02d:%02d:%02d UTC, day %u\r\n",
                2000 + bcd(b[6]) + ((b[5] & 0x80) ? 100 : 0), bcd(b[5] & 0x1f),
                bcd(b[4] & 0x3f), hour, bcd(b[1] & 0x7f), bcd(b[0] & 0x7f),
                bcd(b[3] & 0x07));
    tdsh_printf("ds3231: raw %02x %02x %02x %02x %02x %02x %02x\r\n",
                b[0], b[1], b[2], b[3], b[4], b[5], b[6]);
}

bool parse_hex(const char *s, uint32_t *out)
{
    char *end = nullptr;
    const unsigned long v = std::strtoul(s, &end, 16);
    if (end == s || *end != '\0') {
        return false;
    }
    *out = (uint32_t)v;
    return true;
}

// The desktop's time zone, seconds east of UTC (td_desktop_bl616.c's file).
long desktop_tz(void)
{
    long v = 0;
    FILE *f = std::fopen("/sd/tinydesk_tz", "r");
    if (f) {
        if (std::fscanf(f, "%ld", &v) != 1) v = 0;
        std::fclose(f);
    }
    return v;
}

void print_time(const char *label, int64_t utc)
{
    uint8_t r[7];
    if (!tang_clock_to_regs(utc, r)) {
        tdsh_printf("%s: out of range\r\n", label);
        return;
    }
    tdsh_printf("%s: %04d-%02d-%02d %02d:%02d:%02d\r\n", label,
                2000 + bcd(r[6]) + ((r[5] & 0x80) ? 100 : 0), bcd(r[5] & 0x1f),
                bcd(r[4]), bcd(r[2]), bcd(r[1]), bcd(r[0]));
}

// `YYYY-MM-DD` `HH:MM:SS` to UTC seconds, through the RTC's own register
// check so the range rules are the clock's.
bool parse_utc(const char *date, const char *time, int64_t *utc)
{
    unsigned y, mo, d, h, mi, se;
    char tail;
    if (std::sscanf(date, "%4u-%2u-%2u%c", &y, &mo, &d, &tail) != 3 ||
        std::sscanf(time, "%2u:%2u:%2u%c", &h, &mi, &se, &tail) != 3 ||
        y < 2000 || y > 2199 || mo > 12 || d > 31 || h > 23 || mi > 59 || se > 59) {
        return false;
    }
    auto b = [](unsigned v) { return (uint8_t)(((v / 10u) << 4) | (v % 10u)); };
    const uint8_t r[7] = {b(se), b(mi), b(h), 1, b(d),
                          (uint8_t)(b(mo) | (y >= 2100 ? 0x80 : 0)), b(y % 100u)};
    return tang_clock_from_regs(r, utc);
}

int command_rtc(int argc, char **argv)
{
    if (argc >= 3 && std::strcmp(argv[2], "set") == 0) {
        int64_t utc;
        if (argc != 5 || !parse_utc(argv[3], argv[4], &utc)) {
            tdsh_printf("usage: tangi2c rtc set YYYY-MM-DD HH:MM:SS  (UTC)\r\n");
            return 1;
        }
        if (!tang_clock_hw_ready()) {
            tdsh_printf("tangi2c: the loaded core has no I2C bus\r\n");
            return 1;
        }
        if (!tang_clock_set(utc)) {
            tdsh_printf("tangi2c: the RTC at 0x%02x did not take the time\r\n", DS3231);
            return 1;
        }
        print_time("tangi2c: set, UTC", utc);
        return 0;
    }
    if (argc >= 3 && std::strcmp(argv[2], "sync") == 0) {
        static const char *const why[] = {
            "synced", "the loaded core has no I2C bus, or the link is busy",
            "no answer from the RTC", "the RTC holds no valid time (OSF)"};
        const tang_clock_sync_t got = tang_clock_sync();
        tdsh_printf("tangi2c: clock %s\r\n", why[got]);
        return got == TANG_CLOCK_SYNCED ? 0 : 1;
    }
    if (argc != 2) {
        tdsh_printf("usage: tangi2c rtc [set YYYY-MM-DD HH:MM:SS | sync]\r\n");
        return 1;
    }
    uint8_t data[8] = {0};
    if (!transaction(OP_READ, DS3231, 0x00, 7, nullptr, data)) {
        tdsh_printf("tangi2c: no RTC at 0x%02x\r\n", DS3231);
        return 1;
    }
    print_ds3231(data);
    uint8_t osf = 0;
    if (transaction(OP_READ, DS3231, 0x0f, 1, nullptr, &osf)) {
        tdsh_printf("ds3231: status 0x0f = %02x%s\r\n", osf,
                    (osf & 0x80) ? " (OSF: time invalid)" : "");
    }
    int64_t now;
    if (tang_clock_now(&now)) {
        const long tz = desktop_tz();
        print_time("clock: UTC  ", now);
        tdsh_printf("clock: zone  %+ld s\r\n", tz);
        print_time("clock: local", now + tz);
    } else {
        tdsh_printf("clock: not set\r\n");
    }
    return 0;
}

int command(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    bus_lock lock;
    // Only the desktop core at ABI 1.5 or later has the bus; register 0x140
    // means something else, or nothing, on any other core.
    if (!tang_core_i2c_bus()) {
        tdsh_printf("tangi2c: the loaded core has no I2C bus (the desktop core, ABI 1.5)\r\n");
        return 1;
    }
    status_t s;
    if (!get_status(&s)) {
        tdsh_printf("tangi2c: the core did not answer at 0x140\r\n");
        return 1;
    }

    const char *what = argc >= 2 ? argv[1] : "status";

    if (std::strcmp(what, "status") == 0) {
        print_status(s);
        return 0;
    }

    if (std::strcmp(what, "scan") == 0) {
        if (!write32(REG_CMD, cmd_word(OP_SCAN, 0, 0, 1))) {
            tdsh_printf("tangi2c: cannot start a scan\r\n");
            return 1;
        }
        const TickType_t start = xTaskGetTickCount();
        for (;;) {
            if (!get_status(&s)) {
                tdsh_printf("tangi2c: the core did not answer during the scan\r\n");
                return 1;
            }
            if (s.scan_done) break;
            if ((xTaskGetTickCount() - start) * portTICK_PERIOD_MS >= 1000) {
                tdsh_printf("tangi2c: the scan did not finish\r\n");
                return 1;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        tdsh_printf("tangi2c: scan found %u device%s, first at 0x%02x\r\n",
                    s.scan_count, s.scan_count == 1 ? "" : "s", s.scan_addr);
        return 0;
    }

    if (std::strcmp(what, "read") == 0) {
        if (argc < 5) {
            tdsh_printf("usage: tangi2c read <addr> <reg> <n>\r\n");
            return 1;
        }
        uint32_t addr, reg, len;
        if (!parse_hex(argv[2], &addr) || !parse_hex(argv[3], &reg) ||
            !parse_hex(argv[4], &len) || len == 0 || len > 8) {
            tdsh_printf("tangi2c: address, register and length are hex; length 1..8\r\n");
            return 1;
        }
        uint8_t data[8] = {0};
        if (!transaction(OP_READ, addr, reg, len, nullptr, data)) {
            tdsh_printf("tangi2c: read from 0x%02x failed (nack or timeout)\r\n", addr);
            return 1;
        }
        tdsh_printf("tangi2c: %02x/%02x:", addr, reg);
        for (uint32_t i = 0; i < len; i++) tdsh_printf(" %02x", data[i]);
        tdsh_printf("\r\n");
        return 0;
    }

    if (std::strcmp(what, "write") == 0) {
        if (argc < 5) {
            tdsh_printf("usage: tangi2c write <addr> <reg> <byte> [byte...]\r\n");
            return 1;
        }
        uint32_t addr, reg;
        if (!parse_hex(argv[2], &addr) || !parse_hex(argv[3], &reg)) {
            tdsh_printf("tangi2c: address and register are hex\r\n");
            return 1;
        }
        uint8_t data[8] = {0};
        const int n = argc - 4;
        if (n > 8) {
            tdsh_printf("tangi2c: at most 8 bytes\r\n");
            return 1;
        }
        for (int i = 0; i < n; i++) {
            uint32_t v;
            if (!parse_hex(argv[4 + i], &v) || v > 0xff) {
                tdsh_printf("tangi2c: bad byte '%s'\r\n", argv[4 + i]);
                return 1;
            }
            data[i] = (uint8_t)v;
        }
        if (!transaction(OP_WRITE, addr, reg, (uint32_t)n, data, nullptr)) {
            tdsh_printf("tangi2c: write to 0x%02x failed (nack or timeout)\r\n", addr);
            return 1;
        }
        tdsh_printf("tangi2c: wrote %d byte%s to %02x/%02x\r\n", n, n == 1 ? "" : "s", addr, reg);
        return 0;
    }

    if (std::strcmp(what, "rtc") == 0) {
        return command_rtc(argc, argv);
    }

    if (std::strcmp(what, "auto") == 0) {
        const bool on = argc >= 3 && std::strcmp(argv[2], "on") == 0;
        if (argc >= 3 && !on && std::strcmp(argv[2], "off") != 0) {
            tdsh_printf("usage: tangi2c auto [on|off]\r\n");
            return 1;
        }
        if (!write32(REG_AUTO, auto_word(DS3231, 0x00, 7, on))) {
            tdsh_printf("tangi2c: cannot set the automatic read\r\n");
            return 1;
        }
        tdsh_printf("tangi2c: automatic RTC read %s\r\n", on ? "on" : "off");
        return 0;
    }

    tdsh_printf("usage: tangi2c [status|scan|read <a> <r> <n>|write <a> <r> <b...>|\r\n"
                "                rtc [set YYYY-MM-DD HH:MM:SS|sync]|auto [on|off]]\r\n");
    return 1;
}

} // namespace

// `i2c` is a name TinyDesk Shell's STANDARDS.md reserves for an upstream
// command; this one drives the loaded core's engine, not the BL616's own I2C,
// so it carries the port's prefix.
// ---- the wall clock's hardware (tang_clock.h) ----

extern "C" bool tang_clock_hw_ready(void)
{
    // tang_fpga_link.h's rule for a caller that is not the link's owner: a
    // host pipe or ae350run holds the link for its whole run.
    return tang_core_i2c_bus() && tang_fpga_link_open() == 0 && !tang_fpga_pipe_open();
}

extern "C" bool tang_clock_hw_read(uint8_t reg, uint8_t *buf, unsigned n)
{
    // transaction() copies only `n` bytes out of the 8-byte result window;
    // when it copied all 8, the clock's 1-byte status read overwrote the 7
    // time bytes beside it on the stack (core-log entry 2).
    return n >= 1 && n <= 8 && transaction(OP_READ, DS3231, reg, n, nullptr, buf);
}

extern "C" bool tang_clock_hw_write(uint8_t reg, const uint8_t *buf, unsigned n)
{
    return n >= 1 && n <= 8 && transaction(OP_WRITE, DS3231, reg, n, buf, nullptr);
}

extern "C" uint64_t tang_clock_hw_ms(void)
{
    return bflb_mtimer_get_time_ms();
}

extern "C" int tang_i2c_register(void)
{
    if (s_bus == nullptr) {
        s_bus = xSemaphoreCreateRecursiveMutex();
    }
    static const tdsh_command_t commands[] = {
        {"tangi2c", "tangi2c scan|read <a> <r> <n>|write <a> <r> <b...>|rtc [set|sync]|auto",
         "USB-A I2C bus: scan, read, write, RTC", command, 0},
    };
    return tdsh_register_commands(commands, sizeof(commands) / sizeof(commands[0]));
}
