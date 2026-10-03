// TinyTang — probe the BL616's USB OTG role signals.
//
// One hardware question decides the whole "tether it to a PC, or plug a
// keyboard into it" design: does this board give the controller any way to
// tell what is on the other end of the connector?
//
// USB-C carries no ID pin -- only CC1/CC2 -- so one of three things is true,
// and the answer lives in the schematic we do not have:
//
//   * the board derives an ID-like signal from CC and feeds it to the BL616,
//   * the board wires CC to GPIOs, leaving a Type-C state machine to firmware,
//   * or the port is strapped to a single role.
//
// So read the registers instead of guessing.  Attach the cable to a PC, run
// this, then attach a keyboard and run it again.  What matters is not the
// absolute value but whether the ID input MOVES between the two attachments:
// if it never changes, the connector's role is not reaching the controller
// and automatic role selection is not available to us.
//
// OTG_CSR is a supported thing to read -- the SDK's own usbh_get_port_speed()
// reads the same register for the speed field, and the bit names come from
// drivers/lhal/include/hardware/usb_v2_reg.h.
//
// Deliberately NOT read: OTG_ISR.  Its bits are pulse-or-value latched flags
// that the running USB stack may depend on, and clearing someone else's
// interrupt to satisfy curiosity would be a poor trade.
//
// This is a probe, not a feature.  It reads and prints; it configures nothing,
// and it does not touch the running device stack.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "bflb_mtimer.h"

#include "tdsh.h"
#include "tdsh_bl616.h"
#include "tang_osd.h"

int tdsh_printf(const char *fmt, ...);

/* Mirrors the SDK's own local defines in drivers/lhal/src/bflb_usb_v2.c, which
 * is also where the host bring-up shows the ID pin being forced aside. */
#define BFLB_USB_BASE   0x20072000u
#define BFLB_PDS_BASE   0x2000e000u

#define PDS_USB_CTL_OFFSET     0x500u
#define PDS_REG_USB_DRVBUS_POL (1u << 4)    /* VBUS drive polarity */
#define PDS_REG_USB_IDDIG      (1u << 5)    /* ID input: cleared to force host */

#define USB_OTG_CSR_OFFSET     0x80u

/* OTG_CSR fields, from hardware/usb_v2_reg.h. */
#define CSR_A_BUS_REQ  4u
#define CSR_A_BUS_DROP 5u
#define CSR_B_SESS_VLD 17u
#define CSR_A_SESS_VLD 18u
#define CSR_VBUS_VLD   19u
#define CSR_CROLE      20u
#define CSR_ID         21u
#define CSR_SPD_SHIFT  22u

static uint32_t csr_read(uint32_t address)
{
    return *(volatile uint32_t *)address;
}

static unsigned bit(uint32_t value, unsigned index)
{
    return (value >> index) & 1u;
}

static const char *speed_name(unsigned speed)
{
    switch (speed) {
    case 0:  return "full (12 Mbps)";
    case 1:  return "low (1.5 Mbps)";
    case 2:  return "high (480 Mbps)";
    default: return "not detected";
    }
}

static int cmd_usbstat(tdsh_session_t *session, int argc, char **argv)
{
    (void)session; (void)argc; (void)argv;

    const uint32_t csr = csr_read(BFLB_USB_BASE + USB_OTG_CSR_OFFSET);
    const uint32_t pds = csr_read(BFLB_PDS_BASE + PDS_USB_CTL_OFFSET);

    tdsh_printf("usb: OTG_CSR  @%08x = %08x\r\n",
                (unsigned)(BFLB_USB_BASE + USB_OTG_CSR_OFFSET), (unsigned)csr);
    tdsh_printf("     id        %u   0: this port should be the host (A-device)\r\n",
                bit(csr, CSR_ID));
    tdsh_printf("     crole     %u   role the controller believes it has\r\n",
                bit(csr, CSR_CROLE));
    tdsh_printf("     speed     %u   %s\r\n",
                (unsigned)((csr >> CSR_SPD_SHIFT) & 3u),
                speed_name((csr >> CSR_SPD_SHIFT) & 3u));
    tdsh_printf("     vbus_vld  %u   VBUS present on the connector\r\n",
                bit(csr, CSR_VBUS_VLD));
    tdsh_printf("     a_sess    %u   A-session valid\r\n",
                bit(csr, CSR_A_SESS_VLD));
    tdsh_printf("     b_sess    %u   B-session valid\r\n",
                bit(csr, CSR_B_SESS_VLD));
    tdsh_printf("     bus_req   %u   asking to drive VBUS\r\n",
                bit(csr, CSR_A_BUS_REQ));
    tdsh_printf("     bus_drop  %u   VBUS drive dropped\r\n",
                bit(csr, CSR_A_BUS_DROP));
    tdsh_printf("usb: usb_ctl  @%08x = %08x  iddig=%u drvbus_pol=%u\r\n",
                (unsigned)(BFLB_PDS_BASE + PDS_USB_CTL_OFFSET), (unsigned)pds,
                (pds & PDS_REG_USB_IDDIG) ? 1u : 0u,
                (pds & PDS_REG_USB_DRVBUS_POL) ? 1u : 0u);

    return 0;
}

/* ---------------------------------------------------------------- watching */

// The measurement that matters cannot be read from the console, because the
// console is the port being measured: with a keyboard attached there is no
// computer to print to.  So a watcher samples the register and puts the state
// somewhere that survives the swap -- on the OSD page, in row 27, which the
// OSD terminal leaves free, and on the console when one is attached.
//
// Run `osd term on` first, so the page is up and the reading stays visible on
// the board's own video output after the cable is moved.

#define WATCH_PERIOD_MS 250
#define WATCH_ROW       27      /* below the terminal's 25 rows */

static volatile bool s_watching;
static bool          s_task_started;
static uint32_t      s_last_csr;
static uint32_t      s_last_pds;

/* A small ring of the distinct states seen.  It exists because the reading
 * that matters is taken while the cable is in a keyboard, when there is no
 * computer to print to: the ring holds it until a console is attached again
 * and the replay walks it out.  Only changes are stored. */
#define WATCH_HISTORY 8
static char     s_history[WATCH_HISTORY][TANG_OSD_COLS + 1];
static unsigned s_seq;
static unsigned s_dumped;

/* One line that fits the page width.  Read it as: ID is the connector's role
 * input, G is the PDS iddig bit that the SDK clears to force host mode, ROLE
 * is what the controller decided, SPD the speed it detected, then VBUS and the
 * two session-valid states, and D the VBUS-drive-drop bit. */
static void watch_line(char *out, size_t size)
{
    const uint32_t csr = csr_read(BFLB_USB_BASE + USB_OTG_CSR_OFFSET);
    const uint32_t pds = csr_read(BFLB_PDS_BASE + PDS_USB_CTL_OFFSET);
    const unsigned spd = (csr >> CSR_SPD_SHIFT) & 3u;
    static const char *const names[4] = { "FL", "LO", "HI", "--" };

    snprintf(out, size, "ID%u G%u ROLE%u SPD%u%s VB%u A%u B%u D%u",
             bit(csr, CSR_ID),
             (pds & PDS_REG_USB_IDDIG) ? 1u : 0u,
             bit(csr, CSR_CROLE),
             spd, names[spd < 4 ? spd : 3],
             bit(csr, CSR_VBUS_VLD),
             bit(csr, CSR_A_SESS_VLD),
             bit(csr, CSR_B_SESS_VLD),
             bit(csr, CSR_A_BUS_DROP));
}

static void watch_task(void *arg)
{
    (void)arg;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(WATCH_PERIOD_MS));
        if (!s_watching) {
            continue;
        }

        const uint32_t csr = csr_read(BFLB_USB_BASE + USB_OTG_CSR_OFFSET);
        const uint32_t pds = csr_read(BFLB_PDS_BASE + PDS_USB_CTL_OFFSET);

        if (csr != s_last_csr || pds != s_last_pds) {
            s_last_csr = csr;
            s_last_pds = pds;

            char *slot = s_history[s_seq % WATCH_HISTORY];
            watch_line(slot, TANG_OSD_COLS + 1);
            s_seq++;

            /* The board's own screen: available with no computer attached,
             * which is the whole point.  Quiet when no core is loaded. */
            (void)tang_osd_put(0, WATCH_ROW, slot, strlen(slot));
        }

        /* Replay to the console whenever one is attached.  This is the path
         * that carries a reading taken with a keyboard attached back to the
         * computer, so the swap test does not depend on watching the TV.  One
         * line per tick, so a burst cannot swamp the console. */
        if (s_dumped < s_seq && tdsh_bl616_console_connected()) {
            const unsigned oldest = (s_seq > WATCH_HISTORY) ? s_seq - WATCH_HISTORY : 0u;
            if (s_dumped < oldest) {
                s_dumped = oldest;      /* those fell out of the ring */
            }
            tdsh_printf("usbwatch %u: %s\r\n", s_dumped,
                        s_history[s_dumped % WATCH_HISTORY]);
            s_dumped++;
        }
    }
}

static int cmd_usbwatch(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;

    if (argc < 2) {
        tdsh_printf("usage: usbwatch on | usbwatch off\r\n");
        tdsh_printf("Reports each USB OTG role change to the OSD (row %d) and the\r\n",
                    WATCH_ROW);
        tdsh_printf("console. For the cable-swap test, run `osd term on` first so the\r\n");
        tdsh_printf("reading stays visible without a computer.\r\n");
        tdsh_printf("now: %s\r\n", s_watching ? "watching" : "idle");
        return 1;
    }

    if (strcmp(argv[1], "on") == 0) {
        s_last_csr = csr_read(BFLB_USB_BASE + USB_OTG_CSR_OFFSET);
        s_last_pds = csr_read(BFLB_PDS_BASE + PDS_USB_CTL_OFFSET);

        /* Record where we start, so the replay tells the whole story rather
         * than only the states reached after the command. */
        s_seq = 0;
        s_dumped = 0;
        watch_line(s_history[0], sizeof(s_history[0]));
        s_seq = 1;
        s_watching = true;

        if (!s_task_started &&
            xTaskCreate(watch_task, "usbwatch", 512, NULL, 2, NULL) == pdPASS) {
            s_task_started = true;
        }
        tdsh_printf("usbwatch: on; move the cable and watch row %d of the page,\r\n",
                    WATCH_ROW);
        tdsh_printf("or read the replay here once the console is back\r\n");
        return 0;
    }
    if (strcmp(argv[1], "off") == 0) {
        s_watching = false;
        tdsh_printf("usbwatch: off\r\n");
        return 0;
    }

    tdsh_printf("usbwatch: unknown argument %s\r\n", argv[1]);
    return 1;
}

/* ------------------------------------------------------------ registration */

static const tdsh_command_t s_usb_commands[] = {
    { "usbstat", "usbstat",
      "Read the USB OTG block: role, ID input, VBUS and detected speed",
      cmd_usbstat, 0 },
    { "usbwatch", "usbwatch on|off",
      "Report USB OTG role changes to the OSD, for the cable-swap test",
      cmd_usbwatch, 0 },
};

int tang_usbstat_register(void)
{
    return tdsh_register_commands(s_usb_commands,
                                  sizeof(s_usb_commands) / sizeof(s_usb_commands[0]));
}
