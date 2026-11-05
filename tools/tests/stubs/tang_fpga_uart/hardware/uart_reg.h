/* Host stub: the UART register offsets, and a register file the model owns.
 * SPDX-License-Identifier: MIT
 *
 * tang_fpga_uart.c's uart_write() reads the TX FIFO's free count from
 * UART_FIFO_CONFIG_1 and pushes bytes to UART_FIFO_WDATA, so these two are
 * the whole transmit seam: the FIFO model here reports room and captures
 * every byte written, which is exactly what the core would see on the wire. */
#ifndef STUB_FU_UART_REG_H
#define STUB_FU_UART_REG_H
#include <stdint.h>

#define UART_FIFO_CONFIG_1_OFFSET 0x00u
#define UART_FIFO_WDATA_OFFSET    0x04u
#define UART_TX_FIFO_CNT_MASK     0x03ff0000u
#define UART_TX_FIFO_CNT_SHIFT    16u

#define UART_INTSTS_RX_FIFO (1u << 0)
#define UART_INTSTS_RTO     (1u << 1)
#define UART_INTCLR_RTO     (1u << 1)

/* The register file behind the bus. */
uint32_t host_reg_read(uint32_t address);
void host_reg_write(uint32_t address, uint8_t value);
/* Bits a test wants the status register to report. */
void host_set_intstatus(uint32_t bits);

static inline uint32_t getreg32(uint32_t address)
{
    return host_reg_read(address);
}
static inline void putreg8(uint8_t value, uint32_t address)
{
    host_reg_write(address, value);
}
#endif
