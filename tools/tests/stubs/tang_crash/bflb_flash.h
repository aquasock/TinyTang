/* Host stub: SPDX-License-Identifier: MIT */
#ifndef STUB_CR_FLASH_H
#define STUB_CR_FLASH_H
#include <stdint.h>
int bflb_flash_read(uint32_t addr, void *buf, uint32_t len);
int bflb_flash_erase(uint32_t addr, uint32_t len);
int bflb_flash_write(uint32_t addr, const void *buf, uint32_t len);
#endif
