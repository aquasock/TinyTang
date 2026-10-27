// TinyTang — the PMOD UART, and the RYLR998 LoRa module on it.
//
// Desktop ABI 1.6 gives a socket personality 8: an 8N1 UART on the lower row,
// NRST on pin 8, TX on pin 9, RX on pin 10.  That is the wiring the module was
// proven against when the socket carried i2c_uart (PMOD-008), kept because the
// module's five-wire header goes on by name.
//
// Register map (desktop_regs.sv):
//   0x170 write: one byte into the transmit FIFO
//   0x174 read:  one byte from the receive FIFO; the read pops it
//   0x178 read:  status, [0] rx available, [1] tx full, [2] rx overrun,
//                [3] byte in flight, [19:8] rx count, [31:20] tx count
//   0x17c rw:    bit 0 NRST low, bit 1 flush (a pulse), bit 2 loopback,
//                bit 3 hold
//
// The hold bit is not a convenience.  A register round trip over the BL616
// link is slower than one character at 115200, so writing a command a byte at
// a time leaves the line idle between bytes and the module parses each byte as
// its own command, answering +ERR=1 once per byte (PMOD-008, core-log entry 3).
// Every write here therefore fills the FIFO with the transmitter held and then
// releases it, so the module sees one back-to-back line.
//
// This driver is only the transport: the module's own command set stays the
// module's, and `lora at <text>` passes it through untouched.
//
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "bflb_mtimer.h"

#include "tdsh.h"
#include "tang_fpga_link.h"

int tdsh_printf(const char *fmt, ...);

#define REG_TX      0x170u
#define REG_RX      0x174u
#define REG_STATUS  0x178u
#define REG_CTRL    0x17cu

#define ST_RX_AVAIL (1u << 0)
#define ST_TX_FULL  (1u << 1)
#define ST_OVERRUN  (1u << 2)
#define ST_BUSY     (1u << 3)

#define CTRL_NRST_LOW (1u << 0)
#define CTRL_FLUSH    (1u << 1)
#define CTRL_LOOPBACK (1u << 2)
#define CTRL_HOLD     (1u << 3)

/* --------------------------------------------------------------- helpers */

static bool read_reg(uint32_t address, uint32_t *value)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        if (tang_fpga_read32(address, value)) {
            return true;
        }
    }
    return false;
}

static bool write_reg(uint32_t address, uint32_t value)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        if (tang_fpga_write32(address, value)) {
            return true;
        }
    }
    return false;
}

static bool send_byte(uint8_t byte)
{
    return write_reg(REG_TX, byte);
}

/* Drain received bytes for `window_ms`.  The module's own output is ASCII;
 * anything else prints as '.' so a binary answer cannot scramble the shell. */
static int drain(uint32_t window_ms)
{
    const uint32_t start = bflb_mtimer_get_time_ms();
    int count = 0;
    while (bflb_mtimer_get_time_ms() - start < window_ms) {
        uint32_t status;
        if (!read_reg(REG_STATUS, &status)) {
            return -1;
        }
        if (status & ST_RX_AVAIL) {
            uint32_t byte;
            if (!read_reg(REG_RX, &byte)) {
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

static bool read_ctrl(uint32_t *ctrl)
{
    uint32_t word;
    if (!read_reg(REG_CTRL, &word)) {
        return false;
    }
    *ctrl = word & (CTRL_NRST_LOW | CTRL_LOOPBACK);
    return true;
}

/* -------------------------------------------------------------- commands */

static int cmd_lora(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;

    if (argc < 2) {
        tdsh_printf("usage: lora at <AT command>   send one line and show the reply\r\n");
        tdsh_printf("       lora ctrl              read and write the control register\r\n");
        tdsh_printf("       lora loop on|off       the UART's internal loopback\r\n");
        tdsh_printf("       lora read [ms]         drain the receive FIFO\r\n");
        tdsh_printf("       lora flush             empty both FIFOs\r\n");
        tdsh_printf("       lora status            the UART's FIFO counters\r\n");
        tdsh_printf("       lora reset             pulse the module's NRST\r\n");
        tdsh_printf("       lora ver               AT+VER?, the module's identity\r\n");
        return 1;
    }

    if (strcmp(argv[1], "status") == 0) {
        uint32_t status;
        if (!read_reg(REG_STATUS, &status)) {
            tdsh_printf("lora: the core did not answer at 0x%03lx; is a core with\r\n",
                        (unsigned long)REG_STATUS);
            tdsh_printf("lora: ABI 1.6 and personality 8 loaded?\r\n");
            return 1;
        }
        tdsh_printf("lora: status 0x%08lx, rx %lu, tx %lu%s%s%s\r\n",
                    (unsigned long)status,
                    (unsigned long)((status >> 8) & 0xfffu),
                    (unsigned long)((status >> 20) & 0xfffu),
                    (status & ST_TX_FULL) ? ", tx full" : "",
                    (status & ST_OVERRUN) ? ", rx overrun" : "",
                    (status & ST_BUSY) ? ", sending" : "");
        return 0;
    }

    if (strcmp(argv[1], "ctrl") == 0) {
        /* Separate the two halves: a read that fails points at the address
         * decode, a write that fails points at the write path, and both are
         * inside the FPGA so the wiring cannot be the cause. */
        uint32_t word = 0;
        const bool read_ok = tang_fpga_read32(REG_CTRL, &word);
        tdsh_printf("lora: read  0x%03lx -> %s 0x%08lx\r\n",
                    (unsigned long)REG_CTRL, read_ok ? "ok" : "FAIL",
                    (unsigned long)word);
        const bool write_ok = tang_fpga_write32(REG_CTRL, word);
        tdsh_printf("lora: write 0x%03lx -> %s\r\n",
                    (unsigned long)REG_CTRL, write_ok ? "ok" : "FAIL");
        uint32_t status = 0;
        if (tang_fpga_read32(REG_STATUS, &status)) {
            tdsh_printf("lora: status 0x%08lx\r\n", (unsigned long)status);
        }
        return (read_ok && write_ok) ? 0 : 1;
    }

    if (strcmp(argv[1], "loop") == 0) {
        /* The UART's internal loopback: a byte written comes straight back
         * without any wiring involved, which is the only way to tell an RTL
         * fault from a wiring fault. */
        if (argc < 3 || (strcmp(argv[2], "on") != 0 && strcmp(argv[2], "off") != 0)) {
            tdsh_printf("usage: lora loop on|off\r\n");
            return 1;
        }
        uint32_t ctrl;
        if (!read_ctrl(&ctrl)) {
            tdsh_printf("lora: cannot read the control register\r\n");
            return 1;
        }
        const uint32_t next = strcmp(argv[2], "on") == 0
                            ? (ctrl | CTRL_LOOPBACK)
                            : (ctrl & ~CTRL_LOOPBACK);
        if (!write_reg(REG_CTRL, next)) {
            tdsh_printf("lora: cannot write the control register\r\n");
            return 1;
        }
        tdsh_printf("lora: loopback %s\r\n", strcmp(argv[2], "on") == 0 ? "on" : "off");
        return 0;
    }

    if (strcmp(argv[1], "read") == 0) {
        uint32_t window = 500;
        if (argc >= 3) {
            const int ms = atoi(argv[2]);
            if (ms > 0) window = (uint32_t)ms;
        }
        const int got = drain(window);
        if (got < 0) {
            tdsh_printf("\r\nlora: the core did not answer during the read\r\n");
            return 1;
        }
        tdsh_printf("\r\nlora: %d byte(s) in %lu ms\r\n", got, (unsigned long)window);
        return 0;
    }

    if (strcmp(argv[1], "flush") == 0) {
        uint32_t ctrl;
        if (!read_ctrl(&ctrl) || !write_reg(REG_CTRL, ctrl | CTRL_FLUSH)) {
            tdsh_printf("lora: cannot flush the FIFOs\r\n");
            return 1;
        }
        tdsh_printf("lora: FIFOs flushed\r\n");
        return 0;
    }

    if (strcmp(argv[1], "reset") == 0) {
        uint32_t ctrl;
        if (!read_ctrl(&ctrl)) {
            tdsh_printf("lora: cannot reach the UART's control register\r\n");
            return 1;
        }
        if (!write_reg(REG_CTRL, ctrl | CTRL_NRST_LOW)) return 1;
        vTaskDelay(pdMS_TO_TICKS(120));
        if (!write_reg(REG_CTRL, ctrl & ~CTRL_NRST_LOW)) return 1;
        vTaskDelay(pdMS_TO_TICKS(400));
        tdsh_printf("lora: NRST pulsed\r\n");
        (void)drain(300);
        return 0;
    }

    /* `lora at ...` and `lora ver` both end up sending one line. */
    char line[96];
    size_t used = 0;
    if (strcmp(argv[1], "ver") == 0) {
        used = (size_t)snprintf(line, sizeof(line), "AT+VER?");
    } else if (strcmp(argv[1], "at") == 0) {
        for (int i = 2; i < argc; i++) {
            const size_t n = strlen(argv[i]);
            if (used + n + 2 >= sizeof(line)) {
                tdsh_printf("lora: the command is longer than %u bytes\r\n",
                            (unsigned)(sizeof(line) - 2));
                return 1;
            }
            if (used != 0) line[used++] = ' ';
            memcpy(line + used, argv[i], n);
            used += n;
        }
        line[used] = '\0';
    } else {
        tdsh_printf("lora: unknown argument %s\r\n", argv[1]);
        return 1;
    }

    uint32_t ctrl;
    if (!read_ctrl(&ctrl) || !write_reg(REG_CTRL, ctrl | CTRL_FLUSH | CTRL_HOLD)) {
        tdsh_printf("lora: cannot hold the transmitter\r\n");
        return 1;
    }
    for (size_t i = 0; i < used; i++) {
        if (!send_byte((uint8_t)line[i])) {
            tdsh_printf("lora: the transmit FIFO did not drain\r\n");
            return 1;
        }
    }
    if (!send_byte('\r') || !send_byte('\n')) {
        tdsh_printf("lora: the transmit FIFO did not take the line\r\n");
        return 1;
    }
    if (!write_reg(REG_CTRL, ctrl & ~CTRL_HOLD)) {
        tdsh_printf("lora: cannot release the transmitter\r\n");
        return 1;
    }

    const int got = drain(800);
    if (got < 0) {
        tdsh_printf("\r\nlora: the core did not answer during the read\r\n");
        return 1;
    }
    if (got == 0) {
        tdsh_printf("\r\nlora: no reply in 800 ms\r\n");
        return 1;
    }
    tdsh_printf("\r\n");
    return 0;
}

/* ----------------------------------------------------------- registration */

static const tdsh_command_t s_lora_commands[] = {
    { "lora", "lora at|ctrl|loop|read|flush|status|reset|ver",
      "Drive the RYLR998 LoRa module on the PMOD UART", cmd_lora, 0 },
};

int tang_lora_register(void)
{
    return tdsh_register_commands(s_lora_commands,
                                  sizeof(s_lora_commands) / sizeof(s_lora_commands[0]));
}
