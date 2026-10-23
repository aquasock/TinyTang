// TinyTang — laying printed text out in the desktop core's OLED cells.
//
// The core keeps 384 cells with a stride of 24 whatever font set is showing
// (fpga/desktop/README.md), so a cell's index is row * 24 + column and only
// the font's cols x rows corner is drawn.  Each cell is character 7:0,
// foreground 11:8 and background 15:12.  This file is only the layout: lines
// in, cells out, with nothing from the core in it, so it is checked on the
// host (tools/tests/test_oled_text.sh).
//
// SPDX-License-Identifier: MIT

#ifndef OLED_TEXT_H
#define OLED_TEXT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_CELLS  384
#define OLED_STRIDE 24
/* The core's reset cell, a space in grey on black: the panel is dark. */
#define OLED_BLANK  0x0720
/* Printed text: grey (7) on black (0), as the reset cell. */
#define OLED_TEXT   0x0700
/* The boxed fallback the core draws for a character it has no glyph for. */
#define OLED_UNKNOWN 0x7f

/* Blank every cell, then lay `lines` out from the top-left of a cols x rows
 * grid: each line starts a new row and wraps at cols.  Printable ASCII is
 * kept, a tab is a space, and anything else becomes OLED_UNKNOWN.  Returns
 * false when the text did not fit, in which case what fitted is kept and the
 * rest is cut. */
bool oled_text_layout(const char *const *lines, int nlines, int cols, int rows,
                      uint16_t cells[OLED_CELLS]);

#ifdef __cplusplus
}
#endif

#endif /* OLED_TEXT_H */
