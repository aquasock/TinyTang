// TinyTang — the console transport: a USB CDC-ACM device.
//
// This is the terminal.  The shell renders ANSI into it and the computer on
// the other end of the cable merely displays it, which is TinyDesk's model:
// the board does the work, the PC is a terminal.
//
// Modelled on the SDK's own usbd_cdc_acm example, not on any previous
// firmware.  Received bytes land in a ring buffer from the endpoint callback
// and are drained by the shell; transmitted bytes are handed to the USB stack
// in max-packet-size chunks.

#include "tdsh_bl616.h"
#include "tang_osd_desk.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "bflb_mtimer.h"

#define CDC_IN_EP  0x81
#define CDC_OUT_EP 0x02
#define CDC_INT_EP 0x83

#define USBD_VID       0xFFFF
#define USBD_PID       0x5454   /* "TT" — distinct from the firmware we replace */
#define USBD_MAX_POWER 100
#define USBD_LANGID_STRING 1033

#ifdef CONFIG_USB_HS
#define CDC_MAX_MPS 512
#else
#define CDC_MAX_MPS 64
#endif

#define USB_CONFIG_SIZE (9 + CDC_ACM_DESCRIPTOR_LEN)

static const uint8_t cdc_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0100, 0x01),
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, CDC_MAX_MPS, 0x02),
    USB_LANGID_INIT(USBD_LANGID_STRING),
    0x12, USB_DESCRIPTOR_TYPE_STRING,  /* "TinyTang" */
    'T', 0x00, 'i', 0x00, 'n', 0x00, 'y', 0x00,
    'T', 0x00, 'a', 0x00, 'n', 0x00, 'g', 0x00,
    0x1a, USB_DESCRIPTOR_TYPE_STRING,  /* "TinyTang CDC" */
    'T', 0x00, 'i', 0x00, 'n', 0x00, 'y', 0x00, 'T', 0x00, 'a', 0x00, 'n', 0x00, 'g', 0x00,
    ' ', 0x00, 'C', 0x00, 'D', 0x00, 'C', 0x00,
    0x0c, USB_DESCRIPTOR_TYPE_STRING,  /* "0.1.3" */
    '0', 0x00, '.', 0x00, '1', 0x00, '.', 0x00, '3', 0x00,
    0x00
};

/* The endpoint callbacks run in interrupt context, so the buffers live in
 * non-cached RAM and the indices are guarded by a critical section. */
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t s_out_buf[CDC_MAX_MPS];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t s_in_buf[CDC_MAX_MPS];

#define RX_RING_SIZE 4096
#define RX_RING_MASK (RX_RING_SIZE - 1)
static volatile uint8_t  s_rx_ring[RX_RING_SIZE];
static volatile uint16_t s_rx_head = 0;   /* written by the USB callback */
static volatile uint16_t s_rx_tail = 0;   /* read by the shell */
/* Flow control.  The OUT endpoint is only re-armed while the ring has room for
 * a full packet, so a host that sends faster than the shell consumes is held
 * off by USB itself instead of having its bytes dropped.  Without this a bulk
 * upload (a firmware image, say) loses most of its data silently. */
static volatile bool     s_rx_rearm_pending = false;
static volatile bool     s_configured = false;
static volatile bool     s_tx_busy = false;
static volatile bool     s_dtr = false;

/* Last byte handed to the host, so a bare '\n' can be turned into "\r\n".
 *
 * A USB CDC carries bytes verbatim, and the terminal on the other end is in
 * raw mode (screen, picocom, minicom all are), so it does NOT translate LF to
 * CRLF the way a cooked tty does.  The shell emits bare '\n' in places, which
 * on a raw terminal moves down without returning the carriage -- the output
 * walks off to the right.  Normalising here fixes every consumer at once and
 * is harmless for a cooked one. */
static uint8_t s_last_tx = 0;

/* How long one packet may wait for the host to take it. */
#define TX_CHUNK_TIMEOUT_MS 50u

/* Send to the USB host, never holding the caller up for long.
 *
 * A packet only leaves when the host asks for it, and a host asks only while a
 * program has the port open and is reading.  The old loop waited a million
 * yields per packet whenever the device was enumerated but nobody was reading,
 * and every caller waited with it -- including the desktop, which draws through
 * this console, so with the cable attached TinyDesk on HDMI went laggy, and a
 * large burst (opening the Terminal window) looked like a freeze.  It also
 * cleared the busy flag on its own and started the next packet over one the
 * hardware still held.
 *
 * Now nothing is sent unless a program has the port open (DTR), a packet waits
 * at most TX_CHUNK_TIMEOUT_MS, and a packet the host has not taken is left
 * pending: further output is dropped until the host reads it, and its
 * completion (usbd_cdc_acm_bulk_in) clears the way by itself. */
static void console_flush(const uint8_t *buf, size_t len)
{
    size_t done = 0;
    while (done < len) {
        if (!s_configured || !s_dtr) return;   /* nobody to send to */
        if (s_tx_busy) return;                  /* the host has not read the last one */
        size_t chunk = len - done;
        if (chunk > CDC_MAX_MPS) chunk = CDC_MAX_MPS;
        memcpy(s_in_buf, buf + done, chunk);
        s_tx_busy = true;
        usbd_ep_start_write(CDC_IN_EP, s_in_buf, chunk);
        const uint64_t started = bflb_mtimer_get_time_ms();
        while (s_tx_busy) {
            if (bflb_mtimer_get_time_ms() - started >= TX_CHUNK_TIMEOUT_MS) {
                return;                         /* left pending; see above */
            }
            taskYIELD();
        }
        done += chunk;
    }
}

void usbd_event_handler(uint8_t event)
{
    switch (event) {
    case USBD_EVENT_CONFIGURED:
        s_configured = true;
        s_tx_busy = false;
        usbd_ep_start_read(CDC_OUT_EP, s_out_buf, CDC_MAX_MPS);
        break;
    case USBD_EVENT_DISCONNECTED:
        s_configured = false;
        s_tx_busy = false;
        break;
    default:
        break;
    }
}

/* The host tools' status probe (tdsh_bl616.h).  It is recognised here, in
 * the interrupt that fills the input ring, so neither the shell nor the
 * desktop ever sees it -- which is what lets a tool ask what is on the console
 * without typing into whatever is there.  It is matched within one USB packet,
 * which is how a tool's single write arrives; a partial match is handed on
 * unchanged at the end of the packet, so a lone Esc from a terminal is not
 * held back waiting for bytes that will not come.  While a command reads raw
 * data (tangput), bytes pass untouched: a file may hold the sequence. */
static const uint8_t k_probe[] = { 0x1Bu, '[', '?', '7', '7', 'n' };
static volatile bool s_probe_pending;
static volatile bool s_raw;

void tdsh_bl616_console_set_raw(bool raw)
{
    s_raw = raw;
}

static void ring_put(uint8_t b)
{
    uint16_t next = (uint16_t)((s_rx_head + 1u) & RX_RING_MASK);
    if (next != s_rx_tail) {
        s_rx_ring[s_rx_head] = b;
        s_rx_head = next;
    }
}

void usbd_cdc_acm_bulk_out(uint8_t ep, uint32_t nbytes)
{
    (void)ep;
    /* Interrupt context.  This is the sole producer of the ring, and the shell
     * task is the sole consumer, so the head/tail pair needs no lock -- and a
     * task-context critical section here would be wrong: taskENTER_CRITICAL()
     * from an ISR can assert on interrupt priority and hang the handler, which
     * stalls enumeration.  Only our own index is written here. */
    size_t held = 0;
    for (uint32_t i = 0; i < nbytes; i++) {
        const uint8_t b = s_out_buf[i];
        if (s_raw) {
            ring_put(b);
            continue;
        }
        if (b == k_probe[held]) {
            if (++held == sizeof(k_probe)) {
                s_probe_pending = true;
                held = 0;
            }
            continue;
        }
        for (size_t k = 0; k < held; k++) {
            ring_put(k_probe[k]);
        }
        held = 0;
        if (b == k_probe[0]) {
            held = 1;
            continue;
        }
        ring_put(b);
    }
    for (size_t k = 0; k < held; k++) {
        ring_put(k_probe[k]);
    }
    /* Re-arm only with room for a whole packet; otherwise the read is armed
     * again from read_byte() once the consumer has made room. */
    uint16_t used = (uint16_t)((s_rx_head - s_rx_tail) & RX_RING_MASK);
    if ((uint16_t)(RX_RING_SIZE - 1u - used) >= CDC_MAX_MPS) {
        s_rx_rearm_pending = false;
        usbd_ep_start_read(CDC_OUT_EP, s_out_buf, CDC_MAX_MPS);
    } else {
        s_rx_rearm_pending = true;
    }
}

void usbd_cdc_acm_bulk_in(uint8_t ep, uint32_t nbytes)
{
    (void)ep;
    if ((nbytes % CDC_MAX_MPS) == 0 && nbytes) {
        usbd_ep_start_write(CDC_IN_EP, NULL, 0);   /* zlp */
    } else {
        s_tx_busy = false;
    }
}

static struct usbd_endpoint cdc_out_ep = { .ep_addr = CDC_OUT_EP, .ep_cb = usbd_cdc_acm_bulk_out };
static struct usbd_endpoint cdc_in_ep  = { .ep_addr = CDC_IN_EP,  .ep_cb = usbd_cdc_acm_bulk_in };
static struct usbd_interface intf0;
static struct usbd_interface intf1;

void usbd_cdc_acm_set_dtr(uint8_t intf, bool dtr)
{
    (void)intf;
    s_dtr = dtr;
}

void tdsh_bl616_console_init(void)
{
    usbd_desc_register(cdc_descriptor);
    usbd_add_interface(usbd_cdc_acm_init_intf(&intf0));
    usbd_add_interface(usbd_cdc_acm_init_intf(&intf1));
    usbd_add_endpoint(&cdc_out_ep);
    usbd_add_endpoint(&cdc_in_ep);
    usbd_initialize();
}

bool tdsh_bl616_console_connected(void)
{
    return s_configured && s_dtr;
}

int tdsh_bl616_console_read_byte(void)
{
    /* Answer a status probe from whichever task reads the console next: the
     * shell at its prompt, or the desktop, which polls it continuously.  A
     * command that is running reads nothing, so it answers only when it is
     * done -- too late for a tool waiting to send, which is the point. */
    if (s_probe_pending) {
        s_probe_pending = false;
        char reply[16];
        const int n = snprintf(reply, sizeof(reply), "\x1b[?77;%dn",
                               tdsh_bl616_console_state());
        if (n > 0) {
            (void)tdsh_bl616_console_write_usb(reply, (size_t)n);
        }
    }
    /* Sole consumer of the ring; the ISR owns the head.  No lock needed. */
    int out = -1;
    if (s_rx_tail != s_rx_head) {
        out = (int)s_rx_ring[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1u) & RX_RING_MASK);
    }
    if (out >= 0 && s_rx_rearm_pending) {
        uint16_t used = (uint16_t)((s_rx_head - s_rx_tail) & RX_RING_MASK);
        if ((uint16_t)(RX_RING_SIZE - 1u - used) >= CDC_MAX_MPS) {
            s_rx_rearm_pending = false;
            usbd_ep_start_read(CDC_OUT_EP, s_out_buf, CDC_MAX_MPS);
        }
    }
    return out;
}

int tdsh_bl616_console_write(const void *data, size_t length)
{
    /* Everything the console shows passes through here, which makes this the
     * one place the desktop layer has to watch.  The feed sees the shell's own
     * bytes -- a newline, not the carriage-return-newline inserted below for
     * the USB host.  It is a no-op while the layer is off.  The console is one
     * stream and the layer is a view of it, so it does not own the tap. */
    tang_osd_desk_feed(data, length);
    return tdsh_bl616_console_write_usb(data, length);
}

int tdsh_bl616_console_write_usb(const void *data, size_t length)
{
    const uint8_t *p = (const uint8_t *)data;
    uint8_t out[CDC_MAX_MPS];
    size_t  out_len = 0;
    size_t  count = 0;
    const bool skip_query = tang_osd_desk_enabled();

    for (size_t i = 0; i < length; i++) {
        /* While the layer owns the display, the layer answers the desktop's
         * size query -- it saw the bytes through the tap above.  A terminal on
         * the console would answer too, and the desktop takes *any* cursor
         * position report as a resize, so with both answering it would flip
         * between 80x45 and the terminal's size once a second, a full repaint
         * each time.  Keep the query off the wire.  The 0x1B guard keeps this
         * from costing a comparison on every ordinary byte. */
        if (skip_query && p[i] == 0x1Bu &&
            length - i >= (size_t)TANG_DESK_SIZE_QUERY_LEN &&
            memcmp(p + i, TANG_DESK_SIZE_QUERY, TANG_DESK_SIZE_QUERY_LEN) == 0) {
            i += (size_t)TANG_DESK_SIZE_QUERY_LEN - 1;
            continue;
        }

        uint8_t c = p[i];

        if (c == '\n' && s_last_tx != '\r') {
            if (out_len == sizeof(out)) { console_flush(out, out_len); out_len = 0; }
            out[out_len++] = '\r';
        }
        if (out_len == sizeof(out)) { console_flush(out, out_len); out_len = 0; }
        out[out_len++] = c;
        s_last_tx = c;
        count++;
    }

    if (out_len) console_flush(out, out_len);
    return (int)count;
}
