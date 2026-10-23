// TinyTang — laying printed text out in the desktop core's OLED cells
// (oled_text.h).
//
// SPDX-License-Identifier: MIT

#include "oled_text.h"

static uint16_t cell_for(unsigned char c)
{
    if (c == '\t') {
        c = ' ';
    }
    if (c < 32 || c > 126) {
        c = OLED_UNKNOWN;
    }
    return (uint16_t)(OLED_TEXT | c);
}

bool oled_text_layout(const char *const *lines, int nlines, int cols, int rows,
                      uint16_t cells[OLED_CELLS])
{
    for (int i = 0; i < OLED_CELLS; i++) {
        cells[i] = OLED_BLANK;
    }
    if (cols > OLED_STRIDE) {
        cols = OLED_STRIDE;
    }
    if (rows * OLED_STRIDE > OLED_CELLS) {
        rows = OLED_CELLS / OLED_STRIDE;
    }
    int row = 0;
    for (int n = 0; n < nlines; n++) {
        const unsigned char *p = (const unsigned char *)lines[n];
        int col = 0;
        for (; *p != '\0'; p++) {
            if (col == cols) {
                row++;
                col = 0;
            }
            if (row >= rows) {
                return false;
            }
            cells[row * OLED_STRIDE + col] = cell_for(*p);
            col++;
        }
        row++;
    }
    return true;
}
