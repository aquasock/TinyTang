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
#include "tang_crash.h"
}

// 16 KB of stack for the shell task: the line editor, the parser and uScript
// want more than an idle task, and this is the task the user interacts with.
#define SHELL_TASK_STACK_WORDS 4096
#define SHELL_TASK_PRIORITY    5

static void shell_task(void *arg)
{
    (void)arg;

    /* The card is mounted here, in a task, and not in main(): f_mount() takes
     * the FatFS reentrancy mutex with a timeout (TOOL-015), and a blocking
     * call made before vTaskStartScheduler() has no task to block, so it
     * dereferences a NULL pxCurrentTCB and takes a load access fault. */
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

    // First: take the previous run's crash or hang record aside before
    // anything can overwrite it (ports/bl616/tang_crash.h).
    tang_crash_init();

    // USB comes up first, before any storage work: the stack registers its
    // endpoints and its event handler here, and enumeration then proceeds
    // independently of anything that follows.  Card probing is deliberately
    // after it -- mounting before the device was up is what the last failing
    // build did, and the working firmware on this board also leaves the mount
    // until later rather than doing it at boot.
    tdsh_bl616_console_init();

    // A missing card is not fatal: the shell still comes up and reports it.
    // The mount itself happens in shell_task, after the scheduler (see there).

#ifdef TINYTANG_USB_ONLY
    // Diagnostic build: no scheduler, no shell.
    while (1) {
        bflb_mtimer_delay_ms(1000);
    }
#else
    xTaskCreate(shell_task, "shell", SHELL_TASK_STACK_WORDS, NULL,
                SHELL_TASK_PRIORITY, NULL);
    // The watchdog and its heartbeat; the watchdog arms when its task runs.
    tang_crash_start();

    vTaskStartScheduler();
#endif

    while (1) {
        bflb_mtimer_delay_ms(1000);
    }
}
