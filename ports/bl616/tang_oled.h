// SPDX-License-Identifier: MIT
#ifndef TANG_OLED_H
#define TANG_OLED_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int tang_oled_start(void);
bool tang_oled_running(void);
bool tang_oled_snapshot(uint16_t cells[384], uint32_t *cursor);
void tang_oled_poll(void);
void tang_oled_core_replacing(void);
void tang_oled_core_loaded(void);
void td_oled_terminal_register(void);
/* Font sets, numbered as the desktop core's register 0x118 numbers them. */
enum { TANG_OLED_GRAPHICS = 0, TANG_OLED_STANDARD = 1, TANG_OLED_COMPACT = 2 };
/* The set the user asked for; Standard until `oledterm font` changes it. */
int tang_oled_font_requested(void);
/* Size the terminal for the set the loaded core can show, which is Graphics
 * on a core before desktop ABI 1.2; returns true when the grid changed. */
bool tang_oled_font_effective(int font);
int tang_oled_register(void);
#ifdef __cplusplus
}
#endif
#endif
