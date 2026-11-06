// SPDX-License-Identifier: MIT
//
// The real ports/bl616/phosphor/pmod_sockets.cpp on the host: what a core load
// does to the belief that the loaded core has an I2C bus on the USB-A port.
//
// That belief is one bool, and three things read it: the Wii pad
// (tang_wiipad_hw_ready), the clock, and `tangi2c`.  A load clears it before
// programming because the next core may not have the bus at all; a load that
// programs nothing must put it back, or the pad and the clock stay dead over a
// core that never moved.  That is the regression this pins.
//
// The FPGA is modelled at the two seams the module uses -- the link frames for
// the core-ID probe and fpga_debug_request for the register reads -- so the
// module's own retry loops and its socket-word path run for real.

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* The module includes all of these inside one extern "C" block, so the test
 * has to see the same linkage or every definition below is a different
 * function from the one it declares.  fpga_debug.h is the exception: the
 * module includes it outside the block. */
extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "tang_fpga_link.h"
#include "tang_ini.h"
#include "tang_clock.h"
#include "tdsh.h"
}

#include "fpga_debug.h"

/* ------------------------------------------------------ the module's API */

extern "C" {
void tang_ini_core_loaded(void);
void tang_ini_core_replacing(void);
void tang_ini_core_load_failed(void);
bool tang_core_i2c_bus(void);
}

/* ------------------------------------------------------------ the seams */

static unsigned g_checks, g_failed;
static void check(bool ok, const char *what)
{
    g_checks++;
    if (!ok) {
        g_failed++;
        printf("  FAIL %s\n", what);
    } else {
        printf("  ok   %s\n", what);
    }
}

/* The core answering on the UART, and its register ABI.  A core that is not
 * in the module's table (TinyNES, id 1) answers but has no sockets. */
static uint8_t g_core = 0x54;
static uint32_t g_abi = 0x00010006u;
static bool g_link_up = true;
static unsigned g_clock_changed;
static unsigned g_words_written;

void tang_clock_core_changed(void) { g_clock_changed++; }

int tang_fpga_link_open(void) { return g_link_up ? 0 : -1; }
void tang_fpga_lock(void) {}
void tang_fpga_unlock(void) {}
void tang_fpga_drain(void) {}
int tang_fpga_frame(uint8_t type, const uint8_t *, size_t)
{
    return type == FPGA_CMD_CORE_ID ? 0 : -1;
}
int tang_fpga_wait(uint8_t want, uint8_t *out, size_t cap, uint32_t)
{
    if (want != FPGA_RESP_CORE_ID || cap < 1) {
        return -1;
    }
    *out = g_core;
    return 1;
}

bool fpga_debug_request(uint8_t op, uint32_t address, uint32_t value,
                        fpga_debug_result *out, uint32_t timeout_ms)
{
    (void)timeout_ms;
    memset(out, 0, sizeof *out);
    if (op == FPGA_EXT_READ32) {
        out->status = 0;
        out->data = address == 0x04u ? g_abi : 0u;
        return true;
    }
    if (op == FPGA_EXT_WRITE32) {
        out->status = 0;
        out->data = value;
        g_words_written++;
        return true;
    }
    return false;
}

/* FatFS: no /tinytang.ini in this test, which is one of the module's real
 * paths -- print_declaration() says so and sends nothing. */
FRESULT f_open(FIL *, const TCHAR *, BYTE) { return -1;   /* anything but FR_OK: no /tinytang.ini */ }
FRESULT f_read(FIL *, void *, UINT, UINT *br) { *br = 0; return FR_OK; }
FRESULT f_close(FIL *) { return FR_OK; }

/* tdsh_printf is how the module speaks; the test counts the one line that
 * means the declaration was refused, so silence is distinguishable from a
 * message nobody read. */
static char g_out[4096];
static size_t g_out_len;
extern "C" int tdsh_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(g_out + g_out_len, sizeof g_out - g_out_len, fmt, ap);
    va_end(ap);
    if (n > 0) {
        g_out_len += (size_t)n < sizeof g_out - g_out_len ? (size_t)n
                                                          : sizeof g_out - g_out_len - 1;
    }
    return n;
}

extern "C" int tdsh_register_commands(const tdsh_command_t *, size_t)
{
    return 0;   /* registering a command is not what this test is about */
}

void vTaskDelay(TickType_t) {}
TickType_t xTaskGetTickCount(void) { return 0; }
BaseType_t xTaskCreate(void (*)(void *), const char *, uint32_t, void *, unsigned, void *)
{
    return pdPASS;
}

/* ------------------------------------------------------------- the cases */

static void reset(uint8_t core, uint32_t abi)
{
    g_core = core;
    g_abi = abi;
    g_link_up = true;
    g_clock_changed = 0;
    g_words_written = 0;
    g_out[0] = '\0';
    g_out_len = 0;
}

int main(void)
{
    printf("pmod sockets: the I2C bus belief across a core load\n");

    /* A desktop core at ABI 1.6 has the bus; that is the state a failed load
     * must return to. */
    reset(0x54, 0x00010006u);
    tang_ini_core_loaded();
    check(tang_core_i2c_bus(), "a desktop core at ABI 1.6 sets the bus");

    /* The regression: the load never programmed anything. */
    reset(0x54, 0x00010006u);
    tang_ini_core_loaded();
    const unsigned before = g_clock_changed;
    tang_ini_core_replacing();
    check(!tang_core_i2c_bus(), "replacing a core clears the bus");
    tang_ini_core_load_failed();
    check(tang_core_i2c_bus(),
          "a load that programmed nothing puts the bus back (the pad stays alive)");
    check(g_clock_changed > before + 1,
          "both the clear and the restore ask the clock to resync");

    /* And back the other way: a card with no bus keeps none.  The module's
     * belief is a static, so each case establishes its precondition rather
     * than inheriting it from the case above -- a load of a socketless core
     * is the only way to put it back to "no bus". */
    reset(0x01, 0x00000000u);
    tang_ini_core_replacing();
    tang_ini_core_loaded();
    check(!tang_core_i2c_bus(), "a socketless core leaves the bus off");
    tang_ini_core_replacing();
    tang_ini_core_load_failed();
    check(!tang_core_i2c_bus(),
          "a load that failed over a core with no bus leaves it off");

    /* A successful load of a core without the bus clears it. */
    reset(0x01, 0x00000000u);
    tang_ini_core_replacing();
    tang_ini_core_loaded();
    check(!tang_core_i2c_bus(),
          "a successful load of a socketless core (TinyNES, id 1) clears the bus");

    /* A desktop core at an ABI older than 1.5 has no bus on USB-A. */
    reset(0x54, 0x00010004u);
    tang_ini_core_replacing();
    tang_ini_core_loaded();
    check(!tang_core_i2c_bus(),
          "a desktop core below ABI 1.5 does not claim the bus");

    /* The socket word still goes out, so the fix did not disturb that path:
     * both sockets released from a card with no /tinytang.ini. */
    reset(0x54, 0x00010006u);
    tang_ini_core_loaded();
    check(g_words_written >= 1, "a loaded desktop core is still sent its socket word");
    check(strstr(g_out, "no /tinytang.ini") != NULL,
          "a missing /tinytang.ini is reported, not inferred from silence");

    /* The link being down is not a crash: the load succeeded but nothing
     * answered, so there is no bus. */
    reset(0x54, 0x00010006u);
    g_link_up = false;
    tang_ini_core_replacing();
    tang_ini_core_loaded();
    check(!tang_core_i2c_bus(), "a core that does not answer leaves the bus off");

    printf("pmod sockets: %u checks, %u failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
