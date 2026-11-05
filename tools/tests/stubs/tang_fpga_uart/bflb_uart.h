/* Host stub: the UART driver calls tang_fpga_uart.c makes. SPDX-License-Identifier: MIT */
#ifndef STUB_FU_BFLB_UART_H
#define STUB_FU_BFLB_UART_H
#include <stdint.h>
#include <stdbool.h>

struct bflb_device_s {
    int irq_num;
    uint32_t reg_base;
};

struct bflb_uart_config_s {
    uint32_t baudrate;
    int direction, data_bits, stop_bits, parity, bit_order, flow_ctrl;
    int tx_fifo_threshold, rx_fifo_threshold;
};

#define UART_DIRECTION_TXRX 3
#define UART_DATA_BITS_8    3
#define UART_STOP_BITS_1    0
#define UART_PARITY_NONE    0
#define UART_LSB_FIRST      0

struct bflb_device_s *bflb_device_get_by_name(const char *name);
int bflb_uart_init(struct bflb_device_s *dev, const struct bflb_uart_config_s *cfg);
int bflb_uart_deinit(struct bflb_device_s *dev);
int bflb_uart_get_intstatus(struct bflb_device_s *dev);
bool bflb_uart_rxavailable(struct bflb_device_s *dev);
int bflb_uart_getchar(struct bflb_device_s *dev);
void bflb_uart_int_clear(struct bflb_device_s *dev, uint32_t mask);
void bflb_uart_rxint_mask(struct bflb_device_s *dev, bool mask);
int bflb_uart_txempty(struct bflb_device_s *dev);
int bflb_uart_putchar(struct bflb_device_s *dev, int ch);
#endif
