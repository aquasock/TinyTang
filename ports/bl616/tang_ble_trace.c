// TinyTang — an HCI trace of the Bluetooth stack, for comparing with btmon.
//
// SPDX-License-Identifier: MIT
//
// Diagnostic.  With CONFIG_BT_DEBUG_MONITOR set, the SDK's host passes every
// HCI packet -- each command and ACL packet it sends to the controller in
// bt_send(), each event and ACL packet it receives in bt_recv() and
// bt_recv_prio() -- to bt_monitor_send(), tagged with the Linux monitor
// protocol's packet types (host/monitor.h).  The SDK's own bt_monitor_send()
// prints hex to a debug UART this board does not bring out; the link renames
// it (-Wl,--wrap=bt_monitor_send in CMakeLists.txt) to the one here, which
// records the packets in RAM instead.
//
// The capture is linear, not a ring: `bletrace clear` before a test, and it
// keeps everything from then on until it is full, so a session is recorded
// from its first packet.  LE advertising reports are counted, not stored,
// because a scan would fill it in seconds.  `bletrace dump` prints the packets
// as text, and tools/ble_trace_to_btsnoop.py turns that into a btsnoop file
// that `btmon -r` decodes like a capture taken on a PC.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bflb_irq.h"
#include "bflb_mtimer.h"

#include "tdsh.h"

int tdsh_printf(const char *fmt, ...);

#define TRACE_BYTES    (16 * 1024)
#define TRACE_CAP_MAX  96      /* bytes kept per packet; the length is kept whole */
#define TRACE_HDR      8       /* u32 time in us, u8 type, u8 kept, u16 length */

/* host/monitor.h's packet types, which are also btsnoop's monitor opcodes. */
#define MON_COMMAND_PKT 2
#define MON_EVENT_PKT   3

static uint8_t s_trace[TRACE_BYTES];
static size_t s_used;
static uint32_t s_records;
static uint32_t s_dropped;
static uint32_t s_adv_skipped;

static bool is_adv_report(uint16_t opcode, const uint8_t *p, size_t len)
{
    /* LE Meta (0x3e) with subevent 0x02 advertising, 0x0b direct or 0x0d
     * extended advertising report. */
    return opcode == MON_EVENT_PKT && len >= 3 && p[0] == 0x3e &&
           (p[2] == 0x02 || p[2] == 0x0b || p[2] == 0x0d);
}

void __wrap_bt_monitor_send(uint16_t opcode, const void *data, size_t len)
{
    const uint8_t *p = data;
    const uint32_t now = (uint32_t)bflb_mtimer_get_time_us();
    const size_t keep = len < TRACE_CAP_MAX ? len : TRACE_CAP_MAX;

    uintptr_t flags = bflb_irq_save();
    if (is_adv_report(opcode, p, len)) {
        s_adv_skipped++;
    } else if (s_used + TRACE_HDR + keep > TRACE_BYTES) {
        s_dropped++;
    } else {
        uint8_t *r = &s_trace[s_used];
        r[0] = (uint8_t)now;
        r[1] = (uint8_t)(now >> 8);
        r[2] = (uint8_t)(now >> 16);
        r[3] = (uint8_t)(now >> 24);
        r[4] = (uint8_t)opcode;
        r[5] = (uint8_t)keep;
        r[6] = (uint8_t)len;
        r[7] = (uint8_t)(len >> 8);
        memcpy(r + TRACE_HDR, p, keep);
        s_used += TRACE_HDR + keep;
        s_records++;
    }
    bflb_irq_restore(flags);
}

static void trace_status(void)
{
    tdsh_printf("bletrace: %lu packet(s), %lu of %u bytes, %lu advertising report(s) "
                "skipped, %lu packet(s) dropped when full\r\n",
                (unsigned long)s_records, (unsigned long)s_used, (unsigned)TRACE_BYTES,
                (unsigned long)s_adv_skipped, (unsigned long)s_dropped);
}

/* One line per packet: "BT <us> <type> <length> <hex of the kept bytes>",
 * packets [from, from + count).  The console drops output when the PC has not
 * taken a packet within 50 ms (usb_cdc_bl616.c console_flush), and a dump
 * printed without pause lost everything after its first ~4.7 KB, so each line
 * waits a tick. */
static void trace_dump(uint32_t from, uint32_t count)
{
    static const char hex[] = "0123456789abcdef";
    char line[32 + 2 * TRACE_CAP_MAX];
    uint32_t n = 0;

    trace_status();
    for (size_t at = 0; at < s_used; n++) {
        const uint8_t *r = &s_trace[at];
        const uint32_t t = (uint32_t)r[0] | (uint32_t)r[1] << 8 |
                           (uint32_t)r[2] << 16 | (uint32_t)r[3] << 24;
        const unsigned keep = r[5];
        const unsigned len = (unsigned)r[6] | (unsigned)r[7] << 8;
        at += TRACE_HDR + keep;
        if (n < from || n - from >= count) {
            continue;
        }
        char *o = line;
        for (unsigned i = 0; i < keep; i++) {
            *o++ = hex[r[TRACE_HDR + i] >> 4];
            *o++ = hex[r[TRACE_HDR + i] & 0xf];
        }
        *o = '\0';
        tdsh_printf("BT %lu %u %u %s\r\n", (unsigned long)t, (unsigned)r[4], len, line);
        vTaskDelay(1);
    }
    tdsh_printf("bletrace: end\r\n");
}

int tang_ble_trace_cmd(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    if (argc >= 2 && strcmp(argv[1], "clear") == 0) {
        uintptr_t flags = bflb_irq_save();
        s_used = 0;
        s_records = 0;
        s_dropped = 0;
        s_adv_skipped = 0;
        bflb_irq_restore(flags);
        tdsh_printf("bletrace: cleared\r\n");
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "dump") == 0) {
        uint32_t from = argc >= 3 ? (uint32_t)strtoul(argv[2], NULL, 10) : 0;
        uint32_t count = argc >= 4 ? (uint32_t)strtoul(argv[3], NULL, 10) : UINT32_MAX;
        trace_dump(from, count);
        return 0;
    }
    if (argc >= 2) {
        tdsh_printf("usage: bletrace [clear | dump [from [count]]]\r\n");
        return 1;
    }
    trace_status();
    return 0;
}
