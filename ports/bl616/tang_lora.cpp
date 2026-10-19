// TinyTang — the PMOD UART, and the RYLR998 LoRa module on it.
//
// Personality 7 (`i2c_uart`) gives one socket an I2C bus on the upper row and
// an 8N1 UART on the lower row: RXD on pin 9, TXD on pin 10, NRST on pin 8.
// RYLR998 speaks AT commands at 115200; this driver is only the transport and
// a raw passthrough, so the module's own command set stays the module's.
//
// Register map (desktop_regs, desktop ABI 1.4):
//   0x170 write: one byte into the transmit FIFO
//   0x174 read:  one byte from the receive FIFO; the read pops it
//   0x178 read:  status, [0] rx available, [1] tx full, [2] rx overrun,
//                [3] byte in flight, [19:8] rx count, [31:20] tx count
//   0x17c write: bit 0 NRST low, bit 1 flush (a pulse), bit 2 loopback
//
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstring>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "tdsh.h"
#include "bflb_mtimer.h"
#include "tang_fpga_link.h"
int tdsh_printf(const char *fmt, ...);
bool tang_ini_uart_declared(void);
}

#include "fpga_debug.h"

namespace {

constexpr uint32_t REG_TX     = 0x170;
constexpr uint32_t REG_RX     = 0x174;
constexpr uint32_t REG_STATUS = 0x178;
constexpr uint32_t REG_CTRL   = 0x17c;

constexpr uint32_t ST_RX_AVAIL = 1u << 0;
constexpr uint32_t ST_TX_FULL  = 1u << 1;
constexpr uint32_t ST_OVERRUN  = 1u << 2;
constexpr uint32_t ST_BUSY     = 1u << 3;

constexpr uint32_t CTRL_NRST_LOW = 1u << 0;
constexpr uint32_t CTRL_FLUSH    = 1u << 1;
constexpr uint32_t CTRL_LOOPBACK = 1u << 2;
constexpr uint32_t CTRL_HOLD     = 1u << 3;

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

// The socket must have been declared i2c_uart to a desktop core, and the core
// must still be there; the register reads report the second failure.
bool ready(void)
{
    if (!tang_ini_uart_declared()) {
        tdsh_printf("pmodlora: no i2c_uart socket declared on the loaded core\r\n");
        return false;
    }
    return true;
}

// Queue one byte.  The command is written with the transmitter held, so the
// FIFO (256 bytes) takes a whole line without a per-byte status poll.  Polling
// was the defect: a register round trip is slower than a character at 115200,
// so the line idled between bytes and the module parsed each byte as its own
// command and answered +ERR=1 per byte.
bool send_byte(uint8_t b)
{
    return write32(REG_TX, b);
}

// Drain received bytes for `window_ms`.  The module's own output is ASCII;
// anything else is shown as '.' so a binary answer cannot scramble the shell.
int drain(uint32_t window_ms)
{
    const uint32_t start = bflb_mtimer_get_time_ms();
    int count = 0;
    while (bflb_mtimer_get_time_ms() - start < window_ms) {
        uint32_t status;
        if (!read32(REG_STATUS, &status)) {
            return -1;
        }
        if (status & ST_RX_AVAIL) {
            uint32_t byte;
            if (!read32(REG_RX, &byte)) {
                return -1;
            }
            const unsigned char c = (unsigned char)(byte & 0xffu);
            tdsh_printf("%c", (c >= 0x20 && c < 0x7f) || c == '\r' || c == '\n' ? c : '.');
            count++;
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return count;
}

bool read_ctrl(uint32_t *ctrl)
{
    uint32_t word;
    if (!read32(REG_CTRL, &word)) {
        return false;
    }
    *ctrl = word & (CTRL_NRST_LOW | CTRL_LOOPBACK);
    return true;
}

int command_status(void)
{
    uint32_t status;
    if (!read32(REG_STATUS, &status)) {
        tdsh_printf("pmodlora: the core did not answer at 0x%03lx\r\n",
                    static_cast<unsigned long>(REG_STATUS));
        return 1;
    }
    tdsh_printf("pmodlora: status 0x%08lx, rx %lu, tx %lu%s%s%s\r\n",
                static_cast<unsigned long>(status),
                static_cast<unsigned long>((status >> 8) & 0xfffu),
                static_cast<unsigned long>((status >> 20) & 0xfffu),
                (status & ST_TX_FULL) ? ", tx full" : "",
                (status & ST_OVERRUN) ? ", rx overrun" : "",
                (status & ST_BUSY) ? ", sending" : "");
    return 0;
}

int command_at(int argc, char **argv)
{
    char line[96];
    size_t used = 0;
    for (int i = 2; i < argc; i++) {
        const size_t n = std::strlen(argv[i]);
        if (used + n + 2 >= sizeof(line)) {
            tdsh_printf("pmodlora: the command is longer than %u bytes\r\n",
                        static_cast<unsigned>(sizeof(line) - 2));
            return 1;
        }
        if (used != 0) {
            line[used++] = ' ';
        }
        std::memcpy(line + used, argv[i], n);
        used += n;
    }
    line[used] = '\0';
    // Fill the FIFO with the transmitter held, then release it: the module
    // must see one back-to-back line.
    uint32_t ctrl;
    if (!read_ctrl(&ctrl) || !write32(REG_CTRL, ctrl | CTRL_FLUSH | CTRL_HOLD)) {
        tdsh_printf("pmodlora: cannot hold the transmitter\r\n");
        return 1;
    }
    for (size_t i = 0; i < used; i++) {
        if (!send_byte(static_cast<uint8_t>(line[i]))) {
            tdsh_printf("pmodlora: the transmit FIFO did not drain\r\n");
            return 1;
        }
    }
    if (!send_byte('\r') || !send_byte('\n')) {
        tdsh_printf("pmodlora: the transmit FIFO did not take the line\r\n");
        return 1;
    }
    if (!write32(REG_CTRL, ctrl & ~CTRL_HOLD)) {
        tdsh_printf("pmodlora: cannot release the transmitter\r\n");
        return 1;
    }
    const int got = drain(800);
    if (got < 0) {
        tdsh_printf("pmodlora: the core did not answer during the read\r\n");
        return 1;
    }
    tdsh_printf("\r\npmodlora: %d byte%s\r\n", got, got == 1 ? "" : "s");
    return 0;
}

int command_reset(void)
{
    uint32_t ctrl;
    if (!read_ctrl(&ctrl) || !write32(REG_CTRL, ctrl | CTRL_NRST_LOW)) {
        tdsh_printf("pmodlora: cannot drive NRST\r\n");
        return 1;
    }
    vTaskDelay(pdMS_TO_TICKS(120));   // the module needs 100 ms
    if (!write32(REG_CTRL, ctrl & ~CTRL_NRST_LOW)) {
        tdsh_printf("pmodlora: cannot release NRST\r\n");
        return 1;
    }
    tdsh_printf("pmodlora: module reset\r\n");
    return 0;
}

int command_loopback(int argc, char **argv)
{
    uint32_t ctrl;
    if (argc != 3 ||
        (std::strcmp(argv[2], "on") != 0 && std::strcmp(argv[2], "off") != 0)) {
        tdsh_printf("usage: pmodlora loopback on|off\r\n");
        return 1;
    }
    if (!read_ctrl(&ctrl)) {
        tdsh_printf("pmodlora: the core did not answer at 0x%03lx\r\n",
                    static_cast<unsigned long>(REG_CTRL));
        return 1;
    }
    if (std::strcmp(argv[2], "on") == 0) {
        ctrl |= CTRL_LOOPBACK;
    } else {
        ctrl &= ~CTRL_LOOPBACK;
    }
    if (!write32(REG_CTRL, ctrl)) {
        tdsh_printf("pmodlora: cannot set loopback\r\n");
        return 1;
    }
    tdsh_printf("pmodlora: loopback %s\r\n", (ctrl & CTRL_LOOPBACK) ? "on" : "off");
    return 0;
}

int command_flush(void)
{
    uint32_t ctrl;
    if (!read_ctrl(&ctrl) || !write32(REG_CTRL, ctrl | CTRL_FLUSH)) {
        tdsh_printf("pmodlora: cannot flush the FIFOs\r\n");
        return 1;
    }
    tdsh_printf("pmodlora: FIFOs flushed\r\n");
    return 0;
}

int command(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    if (!ready()) {
        return 1;
    }
    const char *what = argc >= 2 ? argv[1] : "status";
    if (std::strcmp(what, "status") == 0) {
        return command_status();
    }
    if (std::strcmp(what, "at") == 0) {
        if (argc < 3) {
            tdsh_printf("usage: pmodlora at <text>\r\n");
            return 1;
        }
        return command_at(argc, argv);
    }
    if (std::strcmp(what, "recv") == 0) {
        const int got = drain(500);
        if (got < 0) {
            tdsh_printf("pmodlora: the core did not answer during the read\r\n");
            return 1;
        }
        tdsh_printf("\r\npmodlora: %d byte%s\r\n", got, got == 1 ? "" : "s");
        return 0;
    }
    if (std::strcmp(what, "reset") == 0) {
        return command_reset();
    }
    if (std::strcmp(what, "loopback") == 0) {
        return command_loopback(argc, argv);
    }
    if (std::strcmp(what, "flush") == 0) {
        return command_flush();
    }
    tdsh_printf("usage: pmodlora at <text>|recv|reset|loopback on|off|flush|status\r\n");
    return 1;
}

const tdsh_command_t s_commands[] = {
    { "pmodlora", "pmodlora at <text>|recv|reset|loopback on|off|flush|status",
      "LoRa UART: AT passthrough, receive, reset, loopback", command, 0 },
};

} // namespace

extern "C" int tang_lora_register(void)
{
    return tdsh_register_commands(s_commands, sizeof(s_commands) / sizeof(s_commands[0]));
}
