// SPDX-License-Identifier: MIT
// Compile the existing terminal emulator with a distinct type and symbol set.
#include "oled_vterm.h"
static int display_utf8(td_utf8_decoder_t *decoder, uint8_t byte, uint32_t out[2])
{
    int n = td_utf8_feed(decoder, byte, out);
    for (int i = 0; i < n; i++)
        if (out[i] < 32 || out[i] > 126) out[i] = 127;
    return n;
}
#define td_utf8_feed display_utf8
#define td_vterm_t oled_vterm_t
#define td_vterm_init oled_vterm_init
#define td_vterm_resize oled_vterm_resize
#define td_vterm_write oled_vterm_write
#define td_vterm_draw oled_vterm_draw
#define td_vterm_scrollback_lines oled_vterm_scrollback_lines
#include "../../third_party/tinydesk/src/vterm.c"
