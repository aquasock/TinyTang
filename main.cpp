// TinyTang — a TangCore drop-in replacement powered by TinyDesk Shell.
//
// The BL616 boots this firmware, brings up the USB CDC console, mounts the SD
// card, and runs TinyDesk Shell in a task.  The shell is the interface: the
// user's terminal is just a display, and the shell is where a Tang core will
// be loaded from (the FPGA loader lands here next).

#include <stdio.h>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "bflb_mtimer.h"

#include "tdsh_bl616.h"
}

// 16 KB of stack for the shell task: the line editor, the parser and uScript
// want more than an idle task, and this is the task the user interacts with.
#define SHELL_TASK_STACK_WORDS 4096
#define SHELL_TASK_PRIORITY    5

static void shell_task(void *arg)
{
    (void)arg;

    if (tdsh_bl616_init("tinytang") != 0) {
        static const char msg[] = "tinytang: shell init failed\r\n";
        tdsh_bl616_console_write(msg, sizeof(msg) - 1);
    } else {
        tdsh_bl616_run();
    }

    vTaskDelete(NULL);
}

int main(void)
{
    board_init();

    // Storage is brought up here, in main, before the USB device is
    // initialised -- the order the working firmware on this board uses.  Doing
    // it from the shell task instead put the card's GPIO and clock setup in
    // flight while the host was still enumerating the device, which is the
    // kind of interference that stops enumeration altogether.  A missing card
    // is not fatal: the shell still comes up and reports it.
    (void)tdsh_bl616_fs_mount();

    // USB comes up before the scheduler, the order the SDK's own device
    // examples use: the stack registers its endpoints and its event handler
    // here, and enumeration then proceeds independently of any task.
    tdsh_bl616_console_init();

#ifdef TINYTANG_USB_ONLY
    // Diagnostic build: no scheduler, no shell.
    while (1) {
        bflb_mtimer_delay_ms(1000);
    }
#else
    xTaskCreate(shell_task, "shell", SHELL_TASK_STACK_WORDS, NULL,
                SHELL_TASK_PRIORITY, NULL);

    vTaskStartScheduler();
#endif

    while (1) {
        bflb_mtimer_delay_ms(1000);
    }
}
