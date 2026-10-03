// TinyTang — a TangCore drop-in replacement powered by TinyDesk Shell.
//
// Milestone 0: prove the build harness and the board bring-up.  This file
// grows into the shell's host once ports/bl616 exists; for now it does the
// least that can be shown to run on the board.

#include <stdio.h>

extern "C" {
#include "bflb_mtimer.h"
#include "board.h"
#include "bflb_gpio.h"
}

int main(void)
{
    board_init();

    printf("\r\nTinyTang: board bring-up alive\r\n");

    // Blink nothing and print nothing else over the SDK console: the shell
    // will own the console, so this stands in until it does.
    while (1) {
        bflb_mtimer_delay_ms(1000);
    }
}
