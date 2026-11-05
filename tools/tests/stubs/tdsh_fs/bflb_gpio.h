/* Host stub: records the SD power gate. SPDX-License-Identifier: MIT */
#ifndef STUB_FS_BFLB_GPIO_H
#define STUB_FS_BFLB_GPIO_H
#include <stdint.h>
#define GPIO_PIN_16 16u
#define GPIO_OUTPUT 1
#define GPIO_FLOAT 0
#define GPIO_SMT_EN 0
#define GPIO_DRV_3 0
struct bflb_device_s;
void bflb_gpio_init(void *dev, uint32_t pin, int flags);
void bflb_gpio_set(void *dev, uint32_t pin);
uint32_t host_gpio_high_mask(void);
#endif
