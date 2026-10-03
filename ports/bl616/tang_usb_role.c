// TinyTang — which side of the OTG port are we?
//
// The OTG connector is one port with one role at a time.  Tethered to a
// computer it should be a CDC device, which is the console the user types
// into; with a keyboard in it, it should be a host, and the board stands
// alone.  This file is the switching between those two.
//
// It is built on CherryUSB's own lifecycle rather than on the ID pin.  Fact 11
// records why: the OTG block reports no role signal that moves when the cable
// is changed, so there is nothing to key off there.  What we do have is the
// device stack's own view -- it enumerates when a host is on the other end and
// fires USBD_EVENT_DISCONNECTED when that host goes away -- and the host
// stack, which finds a keyboard if one is there.
//
// Three details make the round trip safe, all checked in the SDK rather than
// assumed:
//
//   * usbd_add_endpoint() assigns by endpoint index rather than appending, so
//     registering the console's endpoints twice is idempotent.
//   * usbd_deinitialize() resets the interface offset and deinits the
//     controller, so re-registering the two CDC interfaces rewrites indices 0
//     and 1 instead of duplicating them.
//   * usbd_initialize() is just usb_dc_init(), so bring-up is repeatable.
//
// Which means tdsh_bl616_console_init() is safe to call again, and the way
// back from host mode is a plain re-init.
//
// HID devices are discovered by name: the host HID class registers itself as
// /dev/input<minor> (DEV_FORMAT in class/hid/usbh_hid.c), so a non-NULL
// usbh_find_class_instance("/dev/input0") means a HID device enumerated.
//
// This step switches roles and reports what it found.  It does not yet read
// key reports -- that is the next piece, and it is separate because a host
// that cannot enumerate at all would make it wasted work.

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "tdsh.h"
#include "tdsh_bl616.h"

#include "usbd_core.h"
#include "usbh_core.h"
#include "usbh_hid.h"
#include "ff.h"

int tdsh_printf(const char *fmt, ...);

#define ROLE_PROBE_STEP_MS 200
#define ROLE_PROBE_DEFAULT_S 15

/* The OTG block, as the SDK's host bring-up touches it (mirrors the local
 * defines in drivers/lhal/src/bflb_usb_v2.c). */
#define BFLB_USB_BASE 0x20072000u
#define BFLB_PDS_BASE 0x2000e000u

#define USB_OTG_CSR_OFFSET     0x80u
#define PDS_USB_CTL_OFFSET     0x500u

#define USB_A_BUS_REQ_HOV      (1u << 4)
#define USB_A_BUS_DROP_HOV     (1u << 5)
#define PDS_REG_USB_DRVBUS_POL (1u << 4)
#define PDS_REG_USB_IDDIG      (1u << 5)

static uint32_t reg_read(uint32_t address)
{
    return *(volatile uint32_t *)address;
}

static void reg_or(uint32_t address, uint32_t set)
{
    volatile uint32_t *const p = (volatile uint32_t *)address;
    *p |= set;
}

static void reg_and_clear(uint32_t address, uint32_t clear)
{
    volatile uint32_t *const p = (volatile uint32_t *)address;
    *p &= ~clear;
}

/* Put the connector back the way a peripheral needs it.
 *
 * This is the step that is easy to miss, and it cost a session: the SDK has a
 * host bring-up (usb_hc_low_level_init) and NO host deinit, and
 * usbh_deinitialize() only tears down the software tree.  So the OTG block is
 * left forced to A-device with VBUS being driven, and nothing undoes it --
 * which looks from outside exactly like a dead port, because bringing the
 * device stack up while the port still insists it is a host means no host ever
 * enumerates us.
 *
 * usb_hc_low_level_init drops then requests VBUS; the reverse is to drop it
 * and stop requesting, and to put the ID-input bit back the way the device
 * path found it. */
static void port_back_to_device(void)
{
    reg_or(BFLB_USB_BASE + USB_OTG_CSR_OFFSET, USB_A_BUS_DROP_HOV);
    reg_and_clear(BFLB_USB_BASE + USB_OTG_CSR_OFFSET, USB_A_BUS_REQ_HOV);
    reg_or(BFLB_PDS_BASE + PDS_USB_CTL_OFFSET, PDS_REG_USB_IDDIG);
    /* Leave the polarity bit as we found it, since the probe may have flipped
     * it and nothing else in the SDK ever writes it. */
    reg_and_clear(BFLB_PDS_BASE + PDS_USB_CTL_OFFSET, PDS_REG_USB_DRVBUS_POL);
}

/* 0 = device (the console), 1 = host. */
static volatile int  s_role;
static volatile bool s_busy;
static char          s_note[64] = "no probe run yet";

/* ---------------------------------------------------------------- logging */

// Role changes are logged to the card, because the interesting ones happen
// while the console does not exist: switching to host takes the console away,
// and a revert that fails leaves no way to ask what happened.  A line here
// survives both, and `cat /usbrole.log` reads it back after a power cycle.
//
// Appending from this task is safe in practice: the shell task is blocked
// waiting for console input at this point and is not touching FatFS.
static void role_log(const char *fmt, ...)
{
    if (!tdsh_bl616_fs_ready()) {
        return;
    }

    FIL file;
    if (f_open(&file, "/sd/usbrole.log", FA_OPEN_APPEND | FA_WRITE) != FR_OK) {
        return;
    }

    char line[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    UINT wrote = 0;
    (void)f_write(&file, line, (UINT)strlen(line), &wrote);
    (void)f_write(&file, "\r\n", 2, &wrote);
    f_close(&file);
}

/* ---------------------------------------------------------------- switching */

static void go_host(void)
{
    /* The console goes first: its endpoints belong to the other stack. */
    usbd_deinitialize();
    (void)usbh_initialize();
    s_role = 1;
    role_log("go_host");
}

static void go_device(void)
{
    (void)usbh_deinitialize();
    port_back_to_device();
    /* Idempotent, per the note at the top of the file. */
    tdsh_bl616_console_init();
    s_role = 0;
    role_log("go_device");
}

/* ------------------------------------------------------------------- probe */

/* Switch to host, look for a keyboard for `seconds`, then come back.  Running
 * the switch and the revert in one step is deliberate: while the port is a
 * host there is no console to print to, so the only way the finding reaches
 * the user is to store it and print it after the console returns. */
static void probe_task(void *arg)
{
    const uint32_t seconds = (uint32_t)(uintptr_t)arg;
    const uint32_t budget_ms = (seconds ? seconds : ROLE_PROBE_DEFAULT_S) * 1000u;

    s_note[0] = '\0';
    go_host();

    /* Two halves on purpose.  The SDK requests VBUS but never writes
     * PDS_REG_USB_DRVBUS_POL -- the polarity of the DRVVBUS output -- so if
     * this board's switch is wired for the opposite sense, VBUS simply never
     * appears and no amount of host bring-up changes that.  The first half
     * leaves the bit at its reset value; the second flips it.  If the keyboard
     * lights up partway through the window, that is the answer.
     *
     * The port's own view is logged throughout, because the host stack's
     * silence does not distinguish "nothing attached" from "attached and not
     * enumerated" -- and that is the difference between a wiring problem and a
     * software one. */
    const uint32_t half_ms = budget_ms / 2u;
    role_log("probe start, budget %us", (unsigned)(budget_ms / 1000u));
    role_log("phase 1: DRVBUS_POL=0, csr=%08x pds=%08x",
             (unsigned)reg_read(BFLB_USB_BASE + USB_OTG_CSR_OFFSET),
             (unsigned)reg_read(BFLB_PDS_BASE + PDS_USB_CTL_OFFSET));

    bool phase2 = false;
    uint32_t last_csr = 0xffffffffu;
    struct usbh_hid *hid = NULL;
    for (uint32_t waited = 0; waited < budget_ms && hid == NULL;
         waited += ROLE_PROBE_STEP_MS) {
        if (!phase2 && waited >= half_ms) {
            phase2 = true;
            reg_or(BFLB_PDS_BASE + PDS_USB_CTL_OFFSET, PDS_REG_USB_DRVBUS_POL);
            role_log("phase 2: DRVBUS_POL=1, pds=%08x",
                     (unsigned)reg_read(BFLB_PDS_BASE + PDS_USB_CTL_OFFSET));
        }

        vTaskDelay(pdMS_TO_TICKS(ROLE_PROBE_STEP_MS));

        const uint32_t csr = reg_read(BFLB_USB_BASE + USB_OTG_CSR_OFFSET);
        if (csr != last_csr) {
            role_log("  t=%us csr=%08x pds=%08x", (unsigned)(waited / 1000u),
                     (unsigned)csr,
                     (unsigned)reg_read(BFLB_PDS_BASE + PDS_USB_CTL_OFFSET));
            last_csr = csr;
        }

        hid = (struct usbh_hid *)usbh_find_class_instance("/dev/input0");
    }

    if (hid != NULL) {
        snprintf(s_note, sizeof(s_note), "host: HID %04x:%04x enumerated",
                 (unsigned)hid->hport->device_desc.idVendor,
                 (unsigned)hid->hport->device_desc.idProduct);
    } else {
        snprintf(s_note, sizeof(s_note), "host: nothing enumerated in %us",
                 (unsigned)(budget_ms / 1000u));
    }

    role_log("probe result: %s", s_note);
    go_device();
    s_busy = false;
    vTaskDelete(NULL);
}

/* ----------------------------------------------------------------- command */

static int cmd_usbrole(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;

    if (argc < 2) {
        tdsh_printf("usb: role is %s%s\r\n",
                    s_role ? "host" : "device",
                    s_busy ? " (a probe is running)" : "");
        tdsh_printf("     last probe: %s\r\n", s_note);
        tdsh_printf("usage: usbrole probe [seconds]      switch to host, look, come back\r\n");
        tdsh_printf("       usbrole host | usbrole device  switch and stay (no console in host)\r\n");
        return 1;
    }

    if (strcmp(argv[1], "probe") == 0) {
        if (s_busy) {
            tdsh_printf("usbrole: a probe is already running\r\n");
            return 1;
        }
        const uint32_t seconds = (argc >= 3) ? (uint32_t)strtoul(argv[2], NULL, 10) : 0;
        s_busy = true;

        /* The console drops the moment we switch, so say this first. */
        tdsh_printf("usbrole: switching to host for %us; the console will vanish and\r\n",
                    (unsigned)((seconds ? seconds : ROLE_PROBE_DEFAULT_S)));
        tdsh_printf("come back when the probe ends. Then run `usbrole` again.\r\n");
        vTaskDelay(pdMS_TO_TICKS(150));     /* let that reach the host */

        if (xTaskCreate(probe_task, "usbrole", 1024, (void *)(uintptr_t)seconds,
                        3, NULL) != pdPASS) {
            s_busy = false;
            tdsh_printf("usbrole: cannot start the probe task\r\n");
            return 1;
        }
        return 0;
    }

    if (strcmp(argv[1], "host") == 0) {
        tdsh_printf("usbrole: to host; there is no console from here on\r\n");
        vTaskDelay(pdMS_TO_TICKS(150));
        go_host();
        return 0;
    }

    if (strcmp(argv[1], "device") == 0) {
        go_device();
        snprintf(s_note, sizeof(s_note), "back to device");
        tdsh_printf("usbrole: to device\r\n");
        return 0;
    }

    tdsh_printf("usbrole: unknown argument %s\r\n", argv[1]);
    return 1;
}

static const tdsh_command_t s_role_commands[] = {
    { "usbrole", "usbrole [probe|host|device]",
      "Switch the OTG port between the CDC console and being a USB host",
      cmd_usbrole, 0 },
};

int tang_usb_role_register(void)
{
    return tdsh_register_commands(s_role_commands,
                                  sizeof(s_role_commands) / sizeof(s_role_commands[0]));
}
