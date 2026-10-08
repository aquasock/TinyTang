// SPDX-License-Identifier: MIT
#include "oled_vterm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static oled_vterm_t vt;
static void feed(const char *s) {oled_vterm_write(&vt,(const uint8_t*)s,(int)strlen(s));}
int main(void)
{
    oled_vterm_init(&vt,24,16);
    assert(sizeof(vt)<4096 && vt.cols==24 && vt.rows==16);
    feed("123456789012345678901234X");
    assert(vt.cells[23].ch=='4' && vt.cells[24].ch=='X' && vt.cx==1 && vt.cy==1);
    feed("\033[2J\033[H\033[31;44mA\033[0mB");
    assert(vt.cells[0].ch=='A' && vt.cells[0].fg==1 && vt.cells[0].bg==4);
    assert(vt.cells[1].ch=='B' && vt.cells[1].fg==7 && vt.cells[1].bg==0);
    // A complete unsupported UTF-8 character consumes exactly one display cell.
    feed("\xc3\xa9\xf0\x9f\x98\x80" "C");
    assert(vt.cells[2].ch==127 && vt.cells[3].ch==127 && vt.cells[4].ch=='C');
    feed("\033[?25l");assert(!vt.cursor_visible);
    feed("\033[?25h\033[16;24HZQ");
    assert(vt.cells[14*24+23].ch=='Z' && vt.cells[15*24].ch=='Q');
    assert(vt.sb_count==1 && vt.cols==24 && vt.rows==16);
    oled_vterm_init(&vt,24,16);
    for(int i=0;i<40;i++) feed("line\r\n");
    assert(vt.cy==15 && vt.sb_count==16);
    puts("OLED terminal: fixed geometry, wrap, scroll, ANSI colours, cursor and Unicode fallback PASS");
    return 0;
}
