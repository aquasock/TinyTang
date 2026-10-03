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

    // A missing SD card is not fatal: the shell still comes up, and says so
    // when the filesystem is asked for.
    (void)tdsh_bl616_fs_mount();

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

    // A boot banner on the SDK console (UART0).  Visible only if the board
    // wires the BL616's UART to the debug port, but it costs nothing and it
    // is the one signal available when the USB device is not enumerating.
    printf("\r\nTinyTang: boot\r\n");

    // USB comes up before the scheduler, the order the SDK's own device
    // examples use: the stack registers its endpoints and its event handler
    // here, and enumeration then proceeds independently of any task.
    tdsh_bl616_console_init();

    printf("TinyTang: usb stack initialised\r\n");

#ifdef TINYTANG_USB_ONLY
    // Diagnostic build: no scheduler, no shell.  If the device enumerates in
    // this configuration but not with the shell, the fault is above the USB
    // stack; if it still does not, the fault is in the USB bring-up itself.
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
