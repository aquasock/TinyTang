// TinyTang — glue for the vendored Gowin JTAG programmer.
//
// ports/bl616/tang_jtag_programmer.c is nand2mario's Apache-2.0 bit-banged
// GPIO JTAG programmer for the Gowin GW5A/GW2A, taken from Tang-Control's
// fpga/programmer.cpp and used here unmodified apart from the include list and
// one added comment saying where its licence text lives in this tree.
// This header supplies everything it expected from its own tree: the console
// it reports through, the card file it reads, the GPIO device handle, and
// stubs for the FPGA UART bookkeeping, which TinyTang does not have yet.
//
// The board-specific facts it relies on are the Tang Console's JTAG pins:
// TMS GPIO0, TCK GPIO1, TDO GPIO2, TDI GPIO3 (the register addresses used for
// the fast bit-bang path are declared in the programmer itself).

#ifndef TANG_JTAG_GLUE_H
#define TANG_JTAG_GLUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "bflb_gpio.h"
#include "bflb_mtimer.h"
#include "ff.h"

#include "tdsh.h"
#include "tdsh_bl616.h"

int tdsh_printf(const char *fmt, ...);

/* The programmer uses this handle as if it were a variable; resolving it per
 * use keeps the vendored code untouched. */
#define gpio_dev bflb_device_get_by_name("gpio")

/* Progress goes to the TinyDesk console; the programmer's two reporting calls
 * both become plain console output. */
#define overlay_printf(...) tdsh_printf(__VA_ARGS__)
#define overlay_status(...) tdsh_printf(__VA_ARGS__)
#define overlay_cursor(...) ((void)0)   /* places the OSD cursor; no OSD here */

/* No FPGA UART transport exists in TinyTang, so the programmer's baud
 * bookkeeping is a constant at the legacy-safe rate it expects to find. */
static inline uint32_t fpga_uart_get_baud(void) { return 2000000u; }
static inline uint32_t fpga_uart_set_baud(uint32_t b) { (void)b; return 2000000u; }
static inline uint32_t fpga_debug_set_baud(uint32_t b) { (void)b; return 2000000u; }

/* The Tang Console's JTAG pins. */
#define GPIO_PIN_JTAG_TMS 0
#define GPIO_PIN_JTAG_TCK 1
#define GPIO_PIN_JTAG_TDO 2
#define GPIO_PIN_JTAG_TDI 3

/* Pin helpers the programmer calls.  These were supplied to it from a header
 * outside its tree; they are reproduced here from the working build's
 * preprocessed source, so the pin numbers and directions are the board's own. */
static inline void GPIO_PIN_JTAG_TMS_H(void) { bflb_gpio_set(gpio_dev, GPIO_PIN_JTAG_TMS); }
static inline void GPIO_PIN_JTAG_TMS_L(void) { bflb_gpio_reset(gpio_dev, GPIO_PIN_JTAG_TMS); }
static inline void GPIO_PIN_JTAG_TCK_H(void) { bflb_gpio_set(gpio_dev, GPIO_PIN_JTAG_TCK); }
static inline void GPIO_PIN_JTAG_TCK_L(void) { bflb_gpio_reset(gpio_dev, GPIO_PIN_JTAG_TCK); }
static inline void GPIO_PIN_JTAG_TDI_H(void) { bflb_gpio_set(gpio_dev, GPIO_PIN_JTAG_TDI); }
static inline void GPIO_PIN_JTAG_TDI_L(void) { bflb_gpio_reset(gpio_dev, GPIO_PIN_JTAG_TDI); }
static inline bool GPIO_PIN_JTAG_TDO_V(void) { return bflb_gpio_read(gpio_dev, GPIO_PIN_JTAG_TDO); }

/* The programmer's own debug tracing; off here. */
#define DEBUG(...) ((void)0)

#define BLOCK_SIZE 4096

static uint8_t fbuf[BLOCK_SIZE];
static FIL     fcore;

static int get_file_size(const char *fname)
{
    FIL f;
    if (f_open(&f, fname, FA_READ) != FR_OK) {
        return 0;
    }
    const FSIZE_t size = f_size(&f);
    f_close(&f);
    return (int)size;
}

bool fpga_program(const char *fname);

#endif /* TANG_JTAG_GLUE_H */
