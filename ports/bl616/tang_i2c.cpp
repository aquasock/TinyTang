// TinyTang — the generic I2C bus on a PMOD socket, from the shell.
//
// The FPGA's engine (fpga/desktop/i2c_master.sv) runs one transaction at a
// time: a device address, a register, 1..16 bytes and a direction.  The first
// client is the desktop clock's RTC (Adafruit DS3231 at 0x68, or MCP79410 at
// 0x6F); `i2c scan` is how a new device is found.  The engine also refreshes
// the RTC once a second when `i2c auto on` has configured the address.
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
#include "tdsh.h"
int tdsh_printf(const char *fmt, ...);
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

constexpr uint8_t DS3231 = 0x68;
constexpr uint8_t MCP79410 = 0x6F;

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

bool wait_idle(unsigned prev_seq, uint32_t timeout_ms, status_t *s)
{
    const TickType_t start = xTaskGetTickCount();
    for (;;) {
        if (!get_status(s)) {
            return false;
        }
        if (!s->busy && (s->seq != prev_seq || s->nack)) {
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
uint32_t auto_saved;
bool auto_was_on;
bool auto_read(uint32_t *word, bool *on)
{
    if (!read32(REG_AUTO, word)) {
        return false;
    }
    *on = (*word & (1u << 31)) != 0;
    return true;
}

bool transaction(uint32_t op, uint32_t addr, uint32_t reg, uint32_t len,
                 const uint8_t *wdata, uint8_t *rdata)
{
    uint32_t saved_auto;
    bool on = false;
    (void)auto_read(&saved_auto, &on);
    if (on) {
        (void)write32(REG_AUTO, saved_auto & ~(1u << 31));
        vTaskDelay(pdMS_TO_TICKS(2));
    }
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
    if (!wait_idle(before.seq, 1000, &after)) {
        return false;
    }
    if (op == OP_READ && rdata != nullptr) {
        if (!read_window(rdata)) {
            return false;
        }
    }
    if (on) {
        (void)write32(REG_AUTO, saved_auto);
    }
    return !after.nack;
}

int bcd(unsigned v)
{
    return (int)((v >> 4) * 10 + (v & 0x0f));
}

void print_status(const status_t &s)
{
    tdsh_printf("i2c: %s%s%s seq %u, scan %02x/%u\r\n",
                s.valid ? "valid" : "no data",
                s.busy ? ", busy" : "",
                s.nack ? ", nack" : "",
                s.seq, s.scan_addr, s.scan_count);
}

// The DS3231's time block: 0x00 sec (bit7 EOSC), 0x01 min, 0x02 hour
// (bit6 12/24), 0x03 day, 0x04 date, 0x05 month (bit7 century), 0x06 year.
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
    tdsh_printf("ds3231: %04d-%02d-%02d %02d:%02d:%02d, day %u, %s\r\n",
                2000 + bcd(b[6]), bcd(b[5] & 0x1f), bcd(b[4] & 0x3f),
                hour, bcd(b[1] & 0x7f), bcd(b[0] & 0x7f),
                bcd(b[3] & 0x07), (b[0] & 0x80) ? "oscillator off" : "running");
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

int command(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    uint32_t raw;
    status_t s;
    if (!get_status(&s)) {
        tdsh_printf("i2c: the core did not answer at 0x140\r\n");
        return 1;
    }

    const char *what = argc >= 2 ? argv[1] : "status";

    if (std::strcmp(what, "status") == 0) {
        print_status(s);
        return 0;
    }

    if (std::strcmp(what, "scan") == 0) {
        if (!write32(REG_CMD, cmd_word(OP_SCAN, 0, 0, 1))) {
            tdsh_printf("i2c: cannot start a scan\r\n");
            return 1;
        }
        const TickType_t start = xTaskGetTickCount();
        for (;;) {
            if (!get_status(&s)) {
                tdsh_printf("i2c: the core did not answer during the scan\r\n");
                return 1;
            }
            if (s.scan_done) break;
            if ((xTaskGetTickCount() - start) * portTICK_PERIOD_MS >= 1000) {
                tdsh_printf("i2c: the scan did not finish\r\n");
                return 1;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        tdsh_printf("i2c: scan found %u device%s, first at 0x%02x\r\n",
                    s.scan_count, s.scan_count == 1 ? "" : "s", s.scan_addr);
        return 0;
    }

    if (std::strcmp(what, "read") == 0) {
        if (argc < 5) {
            tdsh_printf("usage: i2c read <addr> <reg> <n>\r\n");
            return 1;
        }
        uint32_t addr, reg, len;
        if (!parse_hex(argv[2], &addr) || !parse_hex(argv[3], &reg) ||
            !parse_hex(argv[4], &len) || len == 0 || len > 8) {
            tdsh_printf("i2c: address, register and length are hex; length 1..8\r\n");
            return 1;
        }
        uint8_t data[8] = {0};
        if (!transaction(OP_READ, addr, reg, len, nullptr, data)) {
            tdsh_printf("i2c: read from 0x%02x failed (nack or timeout)\r\n", addr);
            return 1;
        }
        tdsh_printf("i2c: %02x/%02x:", addr, reg);
        for (uint32_t i = 0; i < len; i++) tdsh_printf(" %02x", data[i]);
        tdsh_printf("\r\n");
        return 0;
    }

    if (std::strcmp(what, "write") == 0) {
        if (argc < 5) {
            tdsh_printf("usage: i2c write <addr> <reg> <byte> [byte...]\r\n");
            return 1;
        }
        uint32_t addr, reg;
        if (!parse_hex(argv[2], &addr) || !parse_hex(argv[3], &reg)) {
            tdsh_printf("i2c: address and register are hex\r\n");
            return 1;
        }
        uint8_t data[8] = {0};
        const int n = argc - 4;
        if (n > 8) {
            tdsh_printf("i2c: at most 8 bytes\r\n");
            return 1;
        }
        for (int i = 0; i < n; i++) {
            uint32_t v;
            if (!parse_hex(argv[4 + i], &v) || v > 0xff) {
                tdsh_printf("i2c: bad byte '%s'\r\n", argv[4 + i]);
                return 1;
            }
            data[i] = (uint8_t)v;
        }
        if (!transaction(OP_WRITE, addr, reg, (uint32_t)n, data, nullptr)) {
            tdsh_printf("i2c: write to 0x%02x failed (nack or timeout)\r\n", addr);
            return 1;
        }
        tdsh_printf("i2c: wrote %d byte%s to %02x/%02x\r\n", n, n == 1 ? "" : "s", addr, reg);
        return 0;
    }

    if (std::strcmp(what, "rtc") == 0) {
        uint8_t data[8] = {0};
        if (!transaction(OP_READ, DS3231, 0x00, 7, nullptr, data)) {
            tdsh_printf("i2c: no RTC at 0x%02x\r\n", DS3231);
            return 1;
        }
        print_ds3231(data);
        uint8_t osf = 0;
        if (transaction(OP_READ, DS3231, 0x0f, 1, nullptr, &osf)) {
            tdsh_printf("ds3231: status 0x0f = %02x%s\r\n", osf,
                        (osf & 0x80) ? " (OSF: time invalid)" : "");
        }
        return 0;
    }

    if (std::strcmp(what, "auto") == 0) {
        const bool on = argc >= 3 && std::strcmp(argv[2], "on") == 0;
        if (argc >= 3 && !on && std::strcmp(argv[2], "off") != 0) {
            tdsh_printf("usage: i2c auto [on|off]\r\n");
            return 1;
        }
        if (!write32(REG_AUTO, auto_word(DS3231, 0x00, 7, on))) {
            tdsh_printf("i2c: cannot set the automatic read\r\n");
            return 1;
        }
        tdsh_printf("i2c: automatic RTC read %s\r\n", on ? "on" : "off");
        return 0;
    }

    tdsh_printf("usage: i2c [status|scan|read <a> <r> <n>|write <a> <r> <b...>|rtc|auto [on|off]]\r\n");
    return 1;
}

} // namespace

extern "C" int tang_i2c_register(void)
{
    static const tdsh_command_t commands[] = {
        {"i2c", "i2c scan|read <a> <r> <n>|write <a> <r> <b...>|rtc|auto",
         "I2C bus: scan, read, write, RTC", command, 0},
    };
    return tdsh_register_commands(commands, sizeof(commands) / sizeof(commands[0]));
}
