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
#include "bflb_uart.h"
#include "bflb_gpio.h"

#include "tdsh_bl616.h"
#include "tang_crash.h"
}

#ifdef TINYTANG_UART_TRACE
/* Diagnostic: report boot progress on the BL616's UART1 (GPIO27/28), the link
 * to the FPGA, which the SIPEED FT2232 also taps as /dev/ttyUSB1 at 2 Mbaud.
 * That channel is observable when the USB console is not, so it says how far a
 * build that never enumerates actually got.  Remove with the guard. */
static struct bflb_device_s *s_trace_uart;
static volatile uint32_t s_milestone;

static void trace_init(void)
{
    struct bflb_device_s *gpio = bflb_device_get_by_name("gpio");
    bflb_gpio_uart_init(gpio, 27, GPIO_UART_FUNC_UART1_RX);
    bflb_gpio_uart_init(gpio, 28, GPIO_UART_FUNC_UART1_TX);
    s_trace_uart = bflb_device_get_by_name("uart1");
    const struct bflb_uart_config_s cfg = {
        .baudrate = 2000000, .direction = UART_DIRECTION_TX,
        .data_bits = UART_DATA_BITS_8, .stop_bits = UART_STOP_BITS_1,
        .parity = UART_PARITY_NONE, .bit_order = UART_LSB_FIRST, .flow_ctrl = 0,
    };
    bflb_uart_init(s_trace_uart, &cfg);
}

static void trace(const char *s)
{
    while (*s) {
        bflb_uart_putchar(s_trace_uart, (uint8_t)*s++);
    }
}

static void trace_ms(uint32_t n)
{
    switch (n) {
    case 1:  trace("TT:M1\r\n"); break;   /* board_init returned */
    case 2:  trace("TT:M2\r\n"); break;   /* crash recorder armed */
    case 3:  trace("TT:M3\r\n"); break;   /* USB console init returned */
    default: trace("TT:M9\r\n"); break;   /* spin: nothing was reached */
    }
    s_milestone = n;
}
#endif

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
#ifndef TINYTANG_NO_FS
    (void)tdsh_bl616_fs_mount();
#endif

#ifdef TINYTANG_NO_SHELL
    // Diagnostic: the scheduler and the USB console are live, but the shell is
    // never initialised.  Separates a fault in the shell from one in the
    // console or the scheduler.
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
#else
    if (tdsh_bl616_init("tinytang") != 0) {
        static const char msg[] = "tinytang: shell init failed\r\n";
        tdsh_bl616_console_write(msg, sizeof(msg) - 1);
    } else {
        tdsh_bl616_run();
    }

    vTaskDelete(NULL);
#endif
}

int main(void)
{
    board_init();
#ifdef TINYTANG_UART_TRACE
    trace_init();
    trace_ms(1);   /* board_init returned */
#endif

    // First: take the previous run's crash or hang record aside before
    // anything can overwrite it (ports/bl616/tang_crash.h).
#ifndef TINYTANG_NO_CRASH
    tang_crash_init();
#endif
#ifdef TINYTANG_UART_TRACE
    trace_ms(2);   /* crash recorder armed */
#endif

    // USB comes up first, before any storage work: the stack registers its
    // endpoints and its event handler here, and enumeration then proceeds
    // independently of anything that follows.  Card probing is deliberately
    // after it -- mounting before the device was up is what the last failing
    // build did, and the working firmware on this board also leaves the mount
    // until later rather than doing it at boot.
    tdsh_bl616_console_init();
#ifdef TINYTANG_UART_TRACE
    trace_ms(3);   /* USB console init returned */
#endif

    // A missing card is not fatal: the shell still comes up and reports it.
    // The mount itself happens in shell_task, after the scheduler (see there).

#ifdef TINYTANG_USB_ONLY
    // Diagnostic build: no scheduler, no shell.
    while (1) {
#ifdef TINYTANG_UART_TRACE
        /* Repeat the milestone so a reader that attaches after boot still
         * learns how far the app got. */
        trace_ms(s_milestone ? s_milestone : 4);
#endif
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
