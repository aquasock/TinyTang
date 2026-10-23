/* SPDX-License-Identifier: MIT
 * `oled print`'s layout (ports/bl616/oled_text.c) against oled_text.h: each
 * argument a line, wrapping at the font's width, the stride of 24, text cut
 * at the last cell, and every other cell the core's dark reset cell.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "oled_text.h"

static uint16_t cells[OLED_CELLS];

static bool lay(int cols, int rows, int n, const char *const *lines)
{
    /* Start from junk: the layout must blank every cell itself. */
    memset(cells, 0x5a, sizeof(cells));
    return oled_text_layout(lines, n, cols, rows, cells);
}

static uint16_t ch(char c) { return (uint16_t)(OLED_TEXT | (unsigned char)c); }

/* Every cell outside [from, to) is blank. */
static void blank_except(int from, int to)
{
    for (int i = 0; i < OLED_CELLS; i++) {
        if (i < from || i >= to) {
            assert(cells[i] == OLED_BLANK);
        }
    }
}

int main(void)
{
    /* Nothing: a clear, every cell the reset cell. */
    assert(lay(12, 8, 0, NULL));
    blank_except(0, 0);

    /* One line at the top-left. */
    const char *hi[] = {"Hi"};
    assert(lay(12, 8, 1, hi));
    assert(cells[0] == ch('H') && cells[1] == ch('i'));
    blank_except(0, 2);

    /* Each argument starts a row; a row is 24 cells apart at every size. */
    const char *two[] = {"ab", "cd"};
    assert(lay(12, 8, 2, two));
    assert(cells[0] == ch('a') && cells[24] == ch('c') && cells[25] == ch('d'));
    assert(cells[2] == OLED_BLANK && cells[26] == OLED_BLANK);

    /* Wrapping at the width: the 13th character opens row 1, and the cells
     * past the grid's width on row 0 stay blank. */
    const char *long_line[] = {"abcdefghijklm"};
    assert(lay(12, 8, 1, long_line));
    assert(cells[11] == ch('l') && cells[12] == OLED_BLANK && cells[24] == ch('m'));

    /* A line that exactly fills its row adds no blank row before the next. */
    const char *full[] = {"abcdefghijkl", "x"};
    assert(lay(12, 8, 2, full));
    assert(cells[11] == ch('l') && cells[24] == ch('x') && cells[48] == OLED_BLANK);

    /* An empty argument is a blank row. */
    const char *gap[] = {"a", "", "b"};
    assert(lay(12, 8, 3, gap));
    assert(cells[0] == ch('a') && cells[24] == OLED_BLANK && cells[48] == ch('b'));

    /* Standard's 12x8 holds 96 characters; the 97th is cut, and nothing is
     * written below the grid. */
    char text[400];
    memset(text, 'z', 96);
    text[96] = '\0';
    const char *fits[] = {text};
    assert(lay(12, 8, 1, fits));
    assert(cells[7 * 24 + 11] == ch('z'));
    text[96] = 'y';
    text[97] = '\0';
    assert(!lay(12, 8, 1, fits));
    assert(cells[7 * 24 + 11] == ch('z'));
    for (int i = 8 * 24; i < OLED_CELLS; i++) {
        assert(cells[i] == OLED_BLANK);
    }
    /* A line past the last row is cut too. */
    const char *rows9[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
    assert(lay(12, 8, 8, rows9));
    assert(!lay(12, 8, 9, rows9));
    assert(cells[7 * 24] == ch('8') && cells[8 * 24] == OLED_BLANK);

    /* Graphics' 24x16 uses every cell: 384 fit, 385 do not. */
    memset(text, 'g', 384);
    text[384] = '\0';
    assert(lay(24, 16, 1, fits));
    for (int i = 0; i < OLED_CELLS; i++) {
        assert(cells[i] == ch('g'));
    }
    text[384] = 'g';
    text[385] = '\0';
    assert(!lay(24, 16, 1, fits));

    /* Compact's 16x8: the 17th character opens row 1. */
    const char *c17[] = {"0123456789abcdefg"};
    assert(lay(16, 8, 1, c17));
    assert(cells[15] == ch('f') && cells[16] == OLED_BLANK && cells[24] == ch('g'));

    /* A tab is a space; a control character, DEL and a UTF-8 byte are the
     * core's boxed fallback. */
    const char *odd[] = {"a\tb\r\x7f\xc3\xa9"};
    assert(lay(12, 8, 1, odd));
    assert(cells[0] == ch('a') && cells[1] == ch(' ') && cells[2] == ch('b'));
    assert(cells[3] == (OLED_TEXT | OLED_UNKNOWN) && cells[4] == (OLED_TEXT | OLED_UNKNOWN));
    assert(cells[5] == (OLED_TEXT | OLED_UNKNOWN) && cells[6] == (OLED_TEXT | OLED_UNKNOWN));

    printf("tb_oled_text: PASS\n");
    return 0;
}
