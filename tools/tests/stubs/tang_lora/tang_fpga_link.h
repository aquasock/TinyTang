/* Host stub: the two register helpers tang_lora.c calls. SPDX-License-Identifier: MIT */
#ifndef STUB_TANG_FPGA_LINK_H
#define STUB_TANG_FPGA_LINK_H
#include <stdint.h>
#include <stdbool.h>
bool tang_fpga_read32(uint32_t address, uint32_t *value);
bool tang_fpga_write32(uint32_t address, uint32_t value);
#endif
