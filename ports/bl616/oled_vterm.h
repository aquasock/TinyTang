// SPDX-License-Identifier: MIT
#ifndef TANG_OLED_VTERM_H
#define TANG_OLED_VTERM_H
// A separately compiled instance of TinyDesk's MIT terminal emulator. Keep
// its private geometry out of translation units that use the desktop vterm.
#define TD_VT_MAX_COLS 24
#define TD_VT_MAX_ROWS 16
#undef TD_VT_SCROLLBACK
#define TD_VT_SCROLLBACK 16
#define td_vterm_t oled_vterm_t
#define td_vterm_init oled_vterm_init
#define td_vterm_resize oled_vterm_resize
#define td_vterm_write oled_vterm_write
#define td_vterm_draw oled_vterm_draw
#define td_vterm_scrollback_lines oled_vterm_scrollback_lines
#include "tinydesk/td_vterm.h"
#undef td_vterm_t
#undef td_vterm_init
#undef td_vterm_resize
#undef td_vterm_write
#undef td_vterm_draw
#undef td_vterm_scrollback_lines
#endif
