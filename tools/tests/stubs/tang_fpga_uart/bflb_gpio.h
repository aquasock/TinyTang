/* Host stub: the two pin-mux calls, recorded. SPDX-License-Identifier: MIT */
#ifndef STUB_FU_BFLB_GPIO_H
#define STUB_FU_BFLB_GPIO_H
#include <stdint.h>
#define GPIO_PIN_27 27u
#define GPIO_PIN_28 28u
#define GPIO_UART_FUNC_UART1_RX 5
#define GPIO_UART_FUNC_UART1_TX 6
void bflb_gpio_uart_init(void *dev, uint32_t pin, int func);
#endif
