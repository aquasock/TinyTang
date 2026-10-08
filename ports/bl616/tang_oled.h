// SPDX-License-Identifier: MIT
#ifndef TANG_OLED_H
#define TANG_OLED_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int tang_oled_start(void);
bool tang_oled_snapshot(uint16_t cells[384], uint32_t *cursor);
void tang_oled_poll(void);
void tang_oled_core_replacing(void);
void tang_oled_core_loaded(void);
void td_oled_terminal_register(void);
int tang_oled_register(void);
#ifdef __cplusplus
}
#endif
#endif
