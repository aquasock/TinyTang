// TinyTang — the BL616 <-> FPGA UART link, and `nesload`.
//
// A Tang core that takes its cartridge over a wire gets it here.  The board
// wires the BL616's UART1 to the core's UART pins (TX GPIO 28, RX GPIO 27 --
// the same pair the reference firmware on this board uses), and the core's
// iosys module parses frames out of that stream.  Nothing in this file is
// inherited from a previous firmware's design: the protocol is read off the
// core's own source (src/iosys/iosys_bl616.v in the nestang tree) and the pins
// and rate are the board's.
//
// The frame, in both directions:
//
//     0xAA len_hi len_lo type payload[len-1]
//
// len counts the type byte, so a ROM frame carrying n bytes of data has
// len = n + 1.  The length is big-endian -- the core reads the high byte
// first (RECV_LEN1) and the reference firmware's fpga_tx_header writes
// len >> 8 first -- and it must stay below 2048, because a length high byte
// >= 8 drops the core back to hunting for the next magic.
//
// Commands this file sends (BL616 -> core):
//   0x01              ask for the core ID.  The reply is the link's liveness
//                     proof: it tells "the core is up and reading us" from
//                     "the stream went nowhere" without watching the HDMI
//                     output, which is the only other evidence available.
//   0x06 state        set the loading state.  Non-zero holds the core in
//                     reset and opens the ROM window; 0 releases it, and that
//                     edge is what starts the game -- Tang-Control's
//                     set_loading_state carries the same note from the other
//                     side, "turn off game loading, this starts the core".
//   0x07 data...      ROM bytes, straight to the core's loader.  The loader
//                     parses the iNES header itself, so the file goes in
//                     verbatim and nothing here interprets it.
//   0x08 on           show/hide the OSD layer.  This one is not cosmetic.
//                     `overlay` is not a per-pixel mask -- it selects the
//                     entire picture at the HDMI mixer (nes2hdmi.sv:210,
//                     `if (overlay) rgb <= overlay_color`), and the core
//                     comes out of reset with the OSD on.  A cartridge loaded
//                     without turning it off plays its music behind a black
//                     screen carrying nothing but the core's logo, which
//                     looks exactly like a video fault and is not one.
//   0x10 <request>    the core's CRC-16-protected register endpoint (opcode 1
//                     reads a 32-bit register).  The clock monitor's cycle
//                     counters are read this way -- `fmon` below, and see
//                     fpga/desktop/clock_monitor.sv for what they are.
//
// The sequence for a cartridge is three steps and is the reference
// firmware's, unchanged:
//
//     loading = 1;  stream the file in 1024-byte frames;  loading = 0
//
// `ae350run` loads a program into a TinyNES core's AE350 from the card itself
// (ae350_load.c has the protocol), sends it a file when it asks, and follows
// its output until it exits.  A program that prints "TinyNES LINK" has turned
// the core's UART to TinyTang's frames: the link is handed back to the desk
// layer, which can then draw TinyDesk over the game, and the program's text is
// followed in FPGA_RESP_AE350_TEXT frames instead.
//
// `fpgapipe` hands the link to the host instead: the console's bytes go to
// the core's UART and the core's bytes come back, untouched, until the host
// closes the port.  It is how a core that speaks its own protocol on these
// pins (TinyNES's AE350 boot ROM and its program loader) is driven from the
// host in two-wire mode.
//
// Two things are done deliberately.  The frame is written with the SDK's
// polling putchar and its return value is checked: putchar gives up after
// 100 ms with -ETIMEDOUT, and a dropped byte in the middle of a ROM would
// otherwise corrupt it silently, which is exactly the kind of failure that
// looks like a bad dump later.  The RX side runs from an interrupt into a
// ring, because the BL616's 32-byte RX FIFO cannot hold a burst at 2 Mbaud
// between polls; without draining it in the ISR the core's replies are simply
// not seen.

#include "tdsh_bl616.h"
#include "fpga_frames.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tang_gbc_ini.h"
#include "tang_nes_ini.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "bflb_gpio.h"
#include "bflb_irq.h"
#include "hardware/uart_reg.h"
#include "bflb_mtimer.h"
#include "bflb_uart.h"
#include "ff.h"

#include "tdsh.h"
#include "ae350_load.h"
#include "tang_ae350_run.h"
#include "tang_fpga_link.h"
#include "tang_osd.h"
#include "tang_osd_desk.h"

int tdsh_printf(const char *fmt, ...);

/* ------------------------------------------------------------- board facts */

#define FPGA_UART_BAUD     2000000u
#define FPGA_UART_TX_PIN   GPIO_PIN_28
#define FPGA_UART_RX_PIN   GPIO_PIN_27

#define FPGA_ROM_CHUNK     1024u

/* Single-producer (ISR) / single-consumer (shell task) ring.  head is written
 * only by the ISR and tail only by the reader, so no lock is needed.  It is
 * sized for a burst: the AE350's save comes back as a run of 65-byte text
 * frames and the decoder drains the ring a frame at a time. */
#define FPGA_RX_RING_SIZE  4096u

static struct bflb_device_s *s_uart;
static bool                 s_uart_ready;
static uint32_t             s_baud = FPGA_UART_BAUD;
static SemaphoreHandle_t    s_tx_lock;

static uint8_t           s_rx_ring[FPGA_RX_RING_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;
/* Bytes the ring had no room for.  Kept because a save that comes back short
 * has to say which buffer lost them: this one, or the frame queue. */
static volatile uint32_t s_rx_drops;

static uint8_t s_rom_chunk[FPGA_ROM_CHUNK];

/* Set while fpgapipe owns the RX ring, so the desk layer's joypad and keyboard
 * polls, which decode without the lock, leave the core's bytes to it. */
static volatile bool s_pipe;

/* ------------------------------------------------------------- RX from core */

static void fpga_uart_rx_isr(int irq, void *arg)
{
    (void)irq;
    (void)arg;

    const uint32_t status = bflb_uart_get_intstatus(s_uart);
    if (status & (UART_INTSTS_RX_FIFO | UART_INTSTS_RTO)) {
        while (bflb_uart_rxavailable(s_uart)) {
            const uint8_t ch = (uint8_t)bflb_uart_getchar(s_uart);
            const uint32_t next = (s_rx_head + 1) & (FPGA_RX_RING_SIZE - 1);
            if (next != s_rx_tail) {          /* drop only if the ring is full */
                s_rx_ring[s_rx_head] = ch;
                s_rx_head = next;
            } else {
                s_rx_drops++;
            }
        }
        if (status & UART_INTSTS_RTO) {
            bflb_uart_int_clear(s_uart, UART_INTCLR_RTO);
        }
    }
}

static bool rx_pop(uint8_t *out)
{
    if (s_rx_head == s_rx_tail) {
        return false;
    }
    *out = s_rx_ring[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1) & (FPGA_RX_RING_SIZE - 1);
    return true;
}

void tang_fpga_drain(void)
{
    s_rx_tail = s_rx_head;
    fpga_frames_reset();     /* the bytes went away; so did the half-seen frame */
}

/* Move everything waiting in the byte ring into the frame queue.
 *
 * The decoding itself lives in fpga_frames.c: it is the trickiest logic in the
 * link and the link is the hardest file here to test, because it needs a UART,
 * an interrupt and a card.  Split out, it runs on a host. */
static void rx_decode(void)
{
    taskENTER_CRITICAL();
    uint8_t byte;
    /* Stop at a full queue instead of overfilling it: the bytes stay in the
     * ring until a waiter takes a frame, so a burst delays frames rather than
     * losing the oldest of them.  A frame left half-decoded across the stop is
     * resumed exactly where it was, because that state lives in fpga_frames. */
    while (!s_pipe && fpga_frames_space() && rx_pop(&byte)) {
        fpga_frames_push(byte);
    }
    taskEXIT_CRITICAL();
}

void tang_fpga_joypad(uint16_t *joy1, uint16_t *joy2)
{
    rx_decode();
    taskENTER_CRITICAL();
    fpga_frames_joypad(joy1, joy2);
    taskEXIT_CRITICAL();
}

/* The transmit lock.  A FreeRTOS mutex rather than a critical section:
 * interrupts stay enabled, because the RX interrupt is what keeps the core's
 * replies from overflowing the 32-byte FIFO. */
void tang_fpga_lock(void)
{
    if (s_tx_lock) {
        xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    }
}

void tang_fpga_unlock(void)
{
    if (s_tx_lock) {
        xSemaphoreGive(s_tx_lock);
    }
}

/* The lock with a bound, for a caller that must not be held by another's long
 * run; the header carries the rule and the check that goes beside it.  A
 * frame's own transmit is a fraction of a tick at 2 Mbaud, while a host's pipe
 * and a load hold this for seconds -- which is the stall this refuses.  It
 * judges nothing itself: the pipe is asked about by the caller, so that the
 * caller's own code is what a host test can plant a fault in. */
bool tang_fpga_lock_within(uint32_t ms)
{
    if (!s_tx_lock) {
        return false;
    }
    return xSemaphoreTake(s_tx_lock, pdMS_TO_TICKS(ms)) == pdTRUE;
}

/* Set by cmd_fpgapipe and by ae350run's load, and read by the desk layer so it
 * sends nothing while a host owns the link.  A plain read of the flag the
 * writers set inside a critical section: a stale answer costs one skipped poll,
 * which is why the lock it guards is also taken with a bound. */
bool tang_fpga_pipe_open(void)
{
    return s_pipe;
}

/* ------------------------------------------------------------------ startup */

/* Program UART1 at a rate and (re)attach the RX interrupt.  Shared by the
 * first open and by a baud switch, so the two cannot configure it differently. */
static void uart_configure(uint32_t baud)
{
    const struct bflb_uart_config_s cfg = {
        .baudrate = baud,
        .direction = UART_DIRECTION_TXRX,
        .data_bits = UART_DATA_BITS_8,
        .stop_bits = UART_STOP_BITS_1,
        .parity = UART_PARITY_NONE,
        .bit_order = UART_LSB_FIRST,
        .flow_ctrl = 0,
        .tx_fifo_threshold = 7,
        .rx_fifo_threshold = 7,
    };
    bflb_uart_init(s_uart, &cfg);

    /* The core is not the console: the SDK's console stays on uart0 where
     * board_init put it, and the shell's output stays on USB CDC. */
    s_rx_head = s_rx_tail = 0;
    bflb_uart_rxint_mask(s_uart, false);
    bflb_irq_attach(s_uart->irq_num, fpga_uart_rx_isr, NULL);
    bflb_irq_enable(s_uart->irq_num);
    s_baud = baud;
}

int tang_fpga_link_open(void)
{
    if (s_uart_ready) {
        return 0;
    }
    if (!s_tx_lock) {
        s_tx_lock = xSemaphoreCreateMutex();
        if (!s_tx_lock) {
            return -1;
        }
    }

    struct bflb_device_s *gpio = bflb_device_get_by_name("gpio");
    bflb_gpio_uart_init(gpio, FPGA_UART_RX_PIN, GPIO_UART_FUNC_UART1_RX);
    bflb_gpio_uart_init(gpio, FPGA_UART_TX_PIN, GPIO_UART_FUNC_UART1_TX);

    s_uart = bflb_device_get_by_name("uart1");
    uart_configure(FPGA_UART_BAUD);

    s_uart_ready = true;
    return 0;
}

int tang_fpga_set_baud(uint32_t baud)
{
    if (!s_uart_ready) {
        return -1;
    }
    /* Let the last request leave the TX FIFO at the old rate before the
     * divider changes under it. */
    while (bflb_uart_txempty(s_uart) == 0) {
    }
    taskENTER_CRITICAL();
    bflb_uart_deinit(s_uart);
    uart_configure(baud);
    taskEXIT_CRITICAL();
    return 0;
}

uint32_t tang_fpga_baud(void)
{
    return s_baud;
}

/* ------------------------------------------------------------- TX to core */

/* Fill the TX FIFO as fast as it drains.
 *
 * bflb_uart_putchar() reads the millisecond timer before every byte, for its
 * 100 ms timeout.  That made a byte cost about 7.5 us of CPU, which capped the
 * link near 133 KB/s whatever the baud -- below even 2 Mbaud's line rate, and
 * well short of the 176 KB/s a CD-quality stream needs at 5 Mbaud.  Here the
 * FIFO's free count is read once and that many bytes are written, and the
 * clock is consulted only while the FIFO is full, with the same timeout. */
static int uart_write(const uint8_t *data, size_t length)
{
    const uint32_t base = s_uart->reg_base;
    size_t i = 0;
    while (i < length) {
        uint32_t room = (getreg32(base + UART_FIFO_CONFIG_1_OFFSET) &
                         UART_TX_FIFO_CNT_MASK) >> UART_TX_FIFO_CNT_SHIFT;
        if (room == 0) {
            const uint64_t start = bflb_mtimer_get_time_ms();
            for (;;) {
                /* Read the clock before the FIFO: a higher-priority task can
                 * pre-empt this loop for longer than the limit, and the FIFO
                 * has usually drained by the time it gets the CPU back. */
                const uint64_t now = bflb_mtimer_get_time_ms();
                room = (getreg32(base + UART_FIFO_CONFIG_1_OFFSET) &
                        UART_TX_FIFO_CNT_MASK) >> UART_TX_FIFO_CNT_SHIFT;
                if (room != 0) {
                    break;
                }
                if (now - start > 100) {
                    return -1;  /* the FIFO stayed full for 100 ms: byte dropped */
                }
            }
        }
        while (room != 0 && i < length) {
            putreg8(data[i++], base + UART_FIFO_WDATA_OFFSET);
            room--;
        }
    }
    return 0;
}

/* One frame.  Caller holds the lock. */
int tang_fpga_frame(uint8_t type, const uint8_t *payload, size_t length)
{
    const size_t len = length + 1;      /* the type byte is counted */
    if (len > FPGA_FRAME_MAX) {
        return -1;
    }
    const uint8_t header[4] = {
        FPGA_FRAME_MAGIC, (uint8_t)(len >> 8), (uint8_t)len, type
    };
    if (uart_write(header, sizeof(header)) != 0) {
        return -1;
    }
    if (length != 0 && uart_write(payload, length) != 0) {
        return -1;
    }
    return 0;
}

/* Assemble response frames out of the ring until `want_type` arrives or the
 * deadline passes.  Everything else (the core's periodic joypad frames, say)
 * is parsed and discarded so it cannot desynchronise the reader.  Returns the
 * payload length, or -1 on timeout or on a short-copy that would exceed cap.
 */
int tang_fpga_wait(uint8_t want_type, uint8_t *out, size_t cap,
                   uint32_t timeout_ms)
{
    const uint32_t deadline = bflb_mtimer_get_time_ms() + timeout_ms;

    for (;;) {
        rx_decode();

        int rc;
        taskENTER_CRITICAL();
        rc = fpga_frames_take(want_type, out, cap);
        taskEXIT_CRITICAL();

        if (rc >= 0) {
            return rc;
        }
        /* A refusal is final: the frame was dropped, so waiting out the
         * timeout for it would only delay the answer. */
        if (rc == FPGA_FRAMES_REFUSED) {
            return -1;
        }
        if ((int32_t)(bflb_mtimer_get_time_ms() - deadline) >= 0) {
            return -1;
        }
        vTaskDelay(1);
    }
}

bool tang_fpga_core_id(uint8_t *id, uint32_t timeout_ms)
{
    if (tang_fpga_link_open() != 0) {
        return false;
    }
    tang_fpga_lock();
    tang_fpga_drain();
    const int sent = tang_fpga_frame(FPGA_CMD_CORE_ID, NULL, 0);
    tang_fpga_unlock();
    return sent == 0 && tang_fpga_wait(FPGA_RESP_CORE_ID, id, 1, timeout_ms) == 1;
}

/* ------------------------------------------------------------------ commands */

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xffff), the core's crc_byte in
 * desktop_regs.sv, byte for byte.  The extended request is protected with it,
 * so this must match or the core answers status 3 (bad CRC) and nothing else. */
static uint16_t fpga_crc16(uint16_t crc, uint8_t data)
{
    crc ^= (uint16_t)data << 8;
    for (int i = 0; i < 8; i++)
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                              : (uint16_t)(crc << 1);
    return crc;
}

/* Read one 32-bit register from the core's desktop register endpoint.
 *
 * The request is 14 bytes of payload under frame type 0x10:
 *
 *   version opcode seq_hi seq_lo addr[3..0] value[3..0] crc_hi crc_lo
 *
 * opcode 1 is read32 and the value field is ignored (zero here); the CRC
 * covers the type byte and the first twelve payload bytes.  The reply is 15
 * bytes and carries the register in bytes 9..12, big-endian, with the core's
 * status in byte 2 (0 = ok).  Caller need not hold the lock. */
static bool fpga_ext_read32(uint32_t address, uint32_t *value, uint8_t *status)
{
    uint8_t req[14];
    memset(req, 0, sizeof(req));
    req[0] = 1;                                   /* extended-request version */
    req[1] = 1;                                   /* opcode: read32           */
    req[4] = (uint8_t)(address >> 24);
    req[5] = (uint8_t)(address >> 16);
    req[6] = (uint8_t)(address >> 8);
    req[7] = (uint8_t)address;
    uint16_t crc = fpga_crc16(0xffffu, 0x10u);
    for (int i = 0; i < 12; i++) crc = fpga_crc16(crc, req[i]);
    req[12] = (uint8_t)(crc >> 8);
    req[13] = (uint8_t)crc;

    tang_fpga_lock();
    const int sent = tang_fpga_frame(0x10, req, sizeof(req));
    tang_fpga_unlock();
    if (sent != 0) return false;

    uint8_t rep[16];
    if (tang_fpga_wait(0x10, rep, sizeof(rep), 500) != 15) return false;
    if (status) *status = rep[2];
    if (value)
        *value = ((uint32_t)rep[9] << 24) | ((uint32_t)rep[10] << 16) |
                 ((uint32_t)rep[11] << 8) | (uint32_t)rep[12];
    return true;
}

/* The clock monitor's registers, contiguous from 0x120: `clk` and `pix` each
 * have two probes (A and B) so a host can see whether both regions the placer
 * chose are running, and `ref` is the 50 MHz crystal every rate is measured
 * against.  See fpga/desktop/clock_monitor.sv. */
#define MON_BASE 0x120u
#define MON_REGS 5u
static const char *const mon_names[MON_REGS] = { "clk", "clk_b", "pix", "pix_b", "ref" };

/* Read all five counters twice, holding for `hold_ms` between, and report the
 * deltas as rates against the nominal 50 MHz reference.  A counter that stands
 * still (a clock that did not reach it) shows as a zero delta. */
static int cmd_fmon(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;

    uint32_t hold_ms = 5000;
    if (argc >= 2) {
        char *end = NULL;
        const unsigned long v = strtoul(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || v == 0 || v > 60000) {
            tdsh_printf("usage: fmon [hold-ms]   0 < ms <= 60000\r\n");
            return 1;
        }
        hold_ms = (uint32_t)v;
    }

    if (tang_fpga_link_open() != 0) {
        tdsh_printf("fmon: cannot bring up UART1\r\n");
        return 1;
    }

    uint32_t stamp = 0;
    uint8_t st = 0;
    if (!fpga_ext_read32(0x134u, &stamp, &st)) {
        tdsh_printf("fmon: no reply -- is the desktop core (with the monitor) loaded?\r\n");
        return 1;
    }
    tdsh_printf("fmon: stamp %02x%02x%02x%02x status %u\r\n",
                (unsigned)(stamp >> 24) & 0xff, (unsigned)(stamp >> 16) & 0xff,
                (unsigned)(stamp >> 8) & 0xff, (unsigned)stamp & 0xff, (unsigned)st);
    if (stamp != 0x4d4f4e31u) {
        tdsh_printf("fmon: this core has no clock monitor (want stamp 4d4f4e31)\r\n");
        return 1;
    }

    uint32_t a[MON_REGS], b[MON_REGS];

    tang_fpga_lock(); tang_fpga_drain(); tang_fpga_unlock();
    const uint32_t t0 = bflb_mtimer_get_time_ms();
    for (uint32_t i = 0; i < MON_REGS; i++)
        if (!fpga_ext_read32(MON_BASE + 4u * i, &a[i], NULL)) {
            tdsh_printf("fmon: read A of %s failed\r\n", mon_names[i]);
            return 1;
        }

    vTaskDelay(pdMS_TO_TICKS(hold_ms));

    const uint32_t t1 = bflb_mtimer_get_time_ms();
    for (uint32_t i = 0; i < MON_REGS; i++)
        if (!fpga_ext_read32(MON_BASE + 4u * i, &b[i], NULL)) {
            tdsh_printf("fmon: read B of %s failed\r\n", mon_names[i]);
            return 1;
        }

    tdsh_printf("fmon: hold %u ms\r\n", (unsigned)(t1 - t0));
    for (uint32_t i = 0; i < MON_REGS; i++)
        tdsh_printf("fmon: %-6s %10u %10u  d %u\r\n",
                    mon_names[i], (unsigned)a[i], (unsigned)b[i],
                    (unsigned)(b[i] - a[i]));

    /* Rate in Hz, as 50 MHz * dtarget / dref, in 64-bit arithmetic because
     * the product overflows 32 bits.  No float: tdsh_printf has no %f. */
    const uint32_t dref = b[MON_REGS - 1] - a[MON_REGS - 1];
    if (dref == 0) {
        tdsh_printf("fmon: the 50 MHz reference did not tick; no rate\r\n");
        return 1;
    }
    for (uint32_t i = 0; i + 1 < MON_REGS; i++) {
        const uint32_t d = b[i] - a[i];
        const uint32_t hz = (uint32_t)(((uint64_t)50000000u * d) / dref);
        tdsh_printf("fmon: %-6s %u Hz\r\n", mon_names[i], (unsigned)hz);
    }
    return 0;
}

/* Read one register of the core's desktop endpoint by address, for checking
 * the link and the register map against a core whose values are known.  The
 * desktop's magic (`fpgar 0`) is 0x00544453 and its ABI word (`fpgar 4`) is
 * 0x00010001, both of which the core answered before the clock monitor
 * existed, so a wrong answer here is the frame, not the monitor. */
static int cmd_fpgar(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    if (argc < 2) {
        tdsh_printf("usage: fpgar <hex-address>\r\n");
        return 1;
    }
    char *end = NULL;
    const unsigned long addr = strtoul(argv[1], &end, 16);
    if (end == argv[1] || *end != '\0' || addr > 0xfffffffful) {
        tdsh_printf("fpgar: not a hex address: %s\r\n", argv[1]);
        return 1;
    }

    if (tang_fpga_link_open() != 0) {
        tdsh_printf("fpgar: cannot bring up UART1\r\n");
        return 1;
    }

    uint32_t value = 0;
    uint8_t status = 0;
    if (!fpga_ext_read32((uint32_t)addr, &value, &status)) {
        tdsh_printf("fpgar: no reply on UART1 (is a core loaded?)\r\n");
        return 1;
    }
    tdsh_printf("fpgar 0x%08lx -> 0x%08lx status %u\r\n",
                addr, (unsigned long)value, (unsigned)status);
    return status == 0 ? 0 : 1;
}

static bool resolve_path(tdsh_session_t *session, const char *in,
                         char *out, size_t out_size)
{
    char logical[TDSH_MAX_PATH];
    return tdsh_path_to_real(session, in, out, out_size,
                             logical, sizeof(logical)) == 0;
}

static int cmd_fpga(tdsh_session_t *session, int argc, char **argv)
{
    (void)session; (void)argc; (void)argv;

    if (tang_fpga_link_open() != 0) {
        tdsh_printf("fpga: cannot bring up UART1\r\n");
        return 1;
    }

    tang_fpga_lock();
    tang_fpga_drain();
    const int sent = tang_fpga_frame(FPGA_CMD_CORE_ID, NULL, 0);
    tang_fpga_unlock();

    if (sent != 0) {
        tdsh_printf("fpga: link up, but the frame could not be sent\r\n");
        return 1;
    }

    uint8_t id = 0;
    const int got = tang_fpga_wait(FPGA_RESP_CORE_ID, &id, sizeof(id), 500);
    if (got == 1) {
        tdsh_printf("fpga: core %u answering on UART1 at %u baud\r\n",
                    (unsigned)id, (unsigned)s_baud);
        return 0;
    }

    tdsh_printf("fpga: no answer on UART1 at %u baud (is a core loaded?)\r\n",
                (unsigned)s_baud);
    return 1;
}

static int cmd_nesload(tdsh_session_t *session, int argc, char **argv)
{
    if (argc < 2) {
        tdsh_printf("usage: nesload <path>\r\n");
        return 1;
    }

    char real[TDSH_MAX_REAL_PATH];
    if (!resolve_path(session, argv[1], real, sizeof(real))) {
        tdsh_printf("nesload: bad path %s\r\n", argv[1]);
        return 1;
    }

    FIL file;
    if (f_open(&file, real, FA_READ) != FR_OK) {
        tdsh_printf("nesload: cannot open %s\r\n", argv[1]);
        return 1;
    }

    const uint32_t size = (uint32_t)f_size(&file);

    /* The core's loader reads an iNES header to size the ROM; a file without
     * one cannot be loaded, so say so here rather than streaming 100 KB into
     * a core that is waiting for a header it will never get. */
    uint8_t magic[16];
    UINT got = 0;
    if (size < sizeof(magic) ||
        f_read(&file, magic, sizeof(magic), &got) != FR_OK ||
        got != sizeof(magic) ||
        magic[0] != 'N' || magic[1] != 'E' || magic[2] != 'S' || magic[3] != 0x1A) {
        f_close(&file);
        tdsh_printf("nesload: %s is not an iNES image (no NES\\x1a header)\r\n", argv[1]);
        return 1;
    }
    if (f_lseek(&file, 0) != FR_OK) {
        f_close(&file);
        tdsh_printf("nesload: cannot rewind\r\n");
        return 1;
    }

    const unsigned mapper = (unsigned)((magic[7] & 0xF0) | (magic[6] >> 4));
    if (tang_fpga_link_open() != 0) {
        f_close(&file);
        tdsh_printf("nesload: cannot bring up UART1\r\n");
        return 1;
    }

    tdsh_printf("nesload: %u bytes, mapper %u -> the core at %u baud\r\n",
                (unsigned)size, mapper, (unsigned)s_baud);

    tang_fpga_lock();
    tang_fpga_drain();

    /* Non-zero holds the core in reset while the cartridge goes in. */
    if (tang_fpga_frame(FPGA_CMD_SET_LOAD, (const uint8_t[]){ 1 }, 1) != 0) {
        tang_fpga_unlock();
        f_close(&file);
        tdsh_printf("nesload: cannot start the load\r\n");
        return 1;
    }

    uint32_t sent_total = 0;
    for (;;) {
        UINT br = 0;
        if (f_read(&file, s_rom_chunk, FPGA_ROM_CHUNK, &br) != FR_OK) {
            tang_fpga_unlock();
            f_close(&file);
            tdsh_printf("nesload: SD read failed at %u\r\n", (unsigned)sent_total);
            return 1;
        }
        if (br == 0) {
            break;
        }
        if (tang_fpga_frame(FPGA_CMD_ROM_DATA, s_rom_chunk, br) != 0) {
            tang_fpga_unlock();
            f_close(&file);
            tdsh_printf("nesload: the core stopped accepting data at %u\r\n",
                        (unsigned)sent_total);
            return 1;
        }
        sent_total += br;
        if (br < FPGA_ROM_CHUNK) {
            break;
        }
        tang_fpga_unlock();
        taskYIELD();                    /* keep the console and USB alive */
        tang_fpga_lock();
    }

    tang_fpga_unlock();

    /* Hide the OSD before the core starts: leaving it on shows the text layer
     * over a game that is otherwise running perfectly.  Through the OSD
     * module, so that its idea of the overlay's state stays true -- which
     * means dropping the lock first, since the OSD takes it itself. */
    if (tang_osd_set(false) != 0) {
        f_close(&file);
        tdsh_printf("nesload: streamed %u bytes but could not clear the OSD\r\n",
                    (unsigned)sent_total);
        return 1;
    }

    /* Releasing the load starts the game. */
    tang_fpga_lock();
    const int released = tang_fpga_frame(FPGA_CMD_SET_LOAD, (const uint8_t[]){ 0 }, 1);
    tang_fpga_unlock();
    f_close(&file);

    if (released != 0) {
        tdsh_printf("nesload: streamed %u bytes but could not start the core\r\n",
                    (unsigned)sent_total);
        return 1;
    }

    tdsh_printf("nesload: %u bytes in; the core is running\r\n", (unsigned)sent_total);
    return 0;
}

/* ------------------------------------------------------------- fpgapipe */

/* Pass the console straight through to UART1 and back until the host closes
 * the port (DTR drops).  Nothing is framed, so a core with its own protocol
 * on the link -- TinyNES's AE350 loader -- is driven by the host's own tool.
 * The TX lock is held throughout, so the desk layer and the other commands
 * wait rather than interleave their frames with the host's stream, and the
 * probe is passed through as data because the stream is binary. */
static int cmd_fpgapipe(tdsh_session_t *session, int argc, char **argv)
{
    (void)session; (void)argc; (void)argv;

    if (tang_fpga_link_open() != 0) {
        tdsh_printf("fpgapipe: cannot bring up UART1\r\n");
        return 1;
    }

    tang_fpga_lock();
    taskENTER_CRITICAL();
    s_pipe = true;
    taskEXIT_CRITICAL();
    tang_fpga_drain();           /* whatever the core said before the host asked */
    tdsh_printf("fpgapipe: open at %u baud; close the port to end\r\n",
                (unsigned)s_baud);
    tdsh_bl616_console_set_raw(true);

    static uint8_t up[512];
    static uint8_t down[512];
    int rc = 0;
    while (tdsh_bl616_console_connected()) {
        size_t n = 0;
        int c;
        while (n < sizeof(up) && (c = tdsh_bl616_console_read_byte()) >= 0) {
            up[n++] = (uint8_t)c;
        }
        const bool sent = n != 0;
        if (sent && uart_write(up, n) != 0) {
            rc = 1;              /* UART1's FIFO stayed full for 100 ms */
            break;
        }
        n = 0;
        while (n < sizeof(down) && rx_pop(&down[n])) {
            n++;
        }
        if (n != 0 && !tdsh_bl616_console_write_raw(down, n)) {
            break;               /* the host closed the port */
        }
        if (!sent && n == 0) {
            vTaskDelay(1);       /* idle: let lower-priority tasks run */
        }
    }

    tdsh_bl616_console_set_raw(false);
    tang_fpga_drain();
    taskENTER_CRITICAL();
    s_pipe = false;
    taskEXIT_CRITICAL();
    tang_fpga_unlock();
    if (rc != 0) {
        tdsh_printf("fpgapipe: UART1 stopped taking bytes\r\n");
    }
    return rc;
}

/* ------------------------------------------------------------- ae350run */

/* The link while ae350run holds it: bytes from the ring, sent in pieces
 * with a yield between, so USB and the console keep running through an
 * 8 KiB block. */
#define AE350_LINE_MAX 160
#define AE350_SILENT_S 300
/* A battery save rides the link as hex text: in link mode the FPGA frames the
 * AE350's bytes as TinyTang's text frames, and that receiver drops CR and ends
 * a line on LF, so raw binary would not survive.  The bound is the largest
 * thing a core may hand back, and what a core hands back is a memory image
 * rather than only a NES battery now: 8 KiB was a Nintendo cartridge's SRAM,
 * and Tiny89's calculator hands back its 256 KiB of RAM, with its 4 MiB flash
 * the next one in line.  A save over the bound is still refused rather than
 * truncated -- the length is judged before a byte is written. */
#define AE350_SAVE_MAX (8u * 1024u * 1024u)
#define AE350_SAVE_DIR "/sd/root/saves/nes"
/* TinyNES's one save state per cartridge (TinyNES core-log entry 49). */
#define AE350_STATE_DIR "/sd/root/states/nes"

typedef struct {
    FIL *elf_file;
    const ae350_elf_t *elf;
    FIL *data_file;
    uint32_t data_length;
    FIL *save_file;                     /* the cartridge's .sav, while sent */
    uint32_t save_length;
    const uint8_t *blob;                /* memory a request is answered from */
    uint32_t blob_length;
    char line[AE350_LINE_MAX];
    size_t line_len;
    bool line_ready;
} ae350_run_t;

static int ae350_write(void *ctx, const uint8_t *data, size_t length)
{
    (void)ctx;
    while (length != 0) {
        const size_t n = length > 512 ? 512 : length;
        if (uart_write(data, n) != 0) {
            return -1;
        }
        data += n;
        length -= n;
        taskYIELD();
    }
    return 0;
}

static int ae350_read_byte(void *ctx, uint32_t timeout_ms)
{
    (void)ctx;
    const uint64_t start = bflb_mtimer_get_time_ms();
    for (;;) {
        uint8_t byte;
        if (rx_pop(&byte)) {
            return byte;
        }
        if (bflb_mtimer_get_time_ms() - start >= timeout_ms) {
            return -1;
        }
        vTaskDelay(1);
    }
}

/* The board's text, a line at a time; a finished line is printed and kept
 * for the caller to look at. */
static void ae350_text(void *ctx, uint8_t byte)
{
    ae350_run_t *run = ctx;
    if (byte == '\r') {
        return;
    }
    if (byte == '\n') {
        run->line[run->line_len] = 0;
        tdsh_printf("%s\r\n", run->line);
        run->line_ready = true;
        run->line_len = 0;
        return;
    }
    if (run->line_len < AE350_LINE_MAX - 1) {
        run->line[run->line_len++] = (char)byte;
    }
}

static int ae350_file_read(void *ctx, uint32_t offset, uint8_t *dst, uint32_t length)
{
    FIL *file = ctx;
    UINT got = 0;
    if (f_lseek(file, offset) != FR_OK || f_read(file, dst, length, &got) != FR_OK ||
        got != length) {
        return -1;
    }
    return 0;
}

static int ae350_program_bytes(void *ctx, uint32_t offset, uint8_t *dst, uint32_t length)
{
    ae350_run_t *run = ctx;
    return ae350_elf_image(run->elf, ae350_file_read, run->elf_file, offset, dst, length);
}

/* "TNR1", the length, then the file's bytes from offset. */
static int ae350_tnr1_bytes(FIL *file, uint32_t file_length, uint32_t offset,
                            uint8_t *dst, uint32_t length)
{
    uint8_t header[AE350_FILE_HEADER] = {
        'T', 'N', 'R', '1',
        (uint8_t)file_length, (uint8_t)(file_length >> 8),
        (uint8_t)(file_length >> 16), (uint8_t)(file_length >> 24),
    };
    while (length != 0 && offset < AE350_FILE_HEADER) {
        *dst++ = header[offset++];
        length--;
    }
    if (length == 0) {
        return 0;
    }
    return ae350_file_read(file, offset - AE350_FILE_HEADER, dst, length);
}

/* The file the program asked for, "TNR1" and its length first. */
static int ae350_payload_bytes(void *ctx, uint32_t offset, uint8_t *dst, uint32_t length)
{
    ae350_run_t *run = ctx;
    return ae350_tnr1_bytes(run->data_file, run->data_length, offset, dst, length);
}

/* The cartridge's battery save, sent as the file itself and not with the
 * "TNR1" header the cartridge payload carries: the AE350 receives this one
 * straight into the core's save buffer, so a header would land inside the save
 * and shift every byte by eight.  That is what a save arrived as on
 * 2026-10-14 -- the first eight bytes of the buffer read "TNR1" and 8192. */
static int ae350_save_bytes(void *ctx, uint32_t offset, uint8_t *dst, uint32_t length)
{
    ae350_run_t *run = ctx;
    return ae350_file_read(run->save_file, offset, dst, length);
}

/* The settings blob, which is memory rather than a file: the AE350's button
 * mapping, received headerless into its own array the way a save is. */
static int ae350_blob_bytes(void *ctx, uint32_t offset, uint8_t *dst, uint32_t length)
{
    ae350_run_t *run = ctx;
    if (run->blob == NULL || offset + length > run->blob_length) {
        return -1;
    }
    memcpy(dst, run->blob + offset, length);
    return 0;
}

/* The next whole line from the board, or false after timeout_ms. */
static bool ae350_next_line(ae350_run_t *run, uint32_t timeout_ms)
{
    const uint64_t start = bflb_mtimer_get_time_ms();
    run->line_ready = false;
    while (!run->line_ready) {
        const uint64_t spent = bflb_mtimer_get_time_ms() - start;
        if (spent >= timeout_ms) {
            return false;
        }
        const int byte = ae350_read_byte(run, (uint32_t)(timeout_ms - spent));
        if (byte >= 0) {
            ae350_text(run, (uint8_t)byte);
        }
    }
    return true;
}

static bool starts_with(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* The program this deck runs names itself in what it prints: TinyNES's player
 * says "TinyNES WANT ...", TinyGBC's "TinyGBC WANT ...".  One set of parsers
 * serves both, so a line is matched against the name it carries and then past
 * it, and the caller reads the same fields either way.  The NVRAM and settings
 * requests below are TinyNES's alone until the TinyGBC player has them. */
static const char *ae350_past_name(const char *line)
{
    static const char *const names[] = { "TinyNES ", "TinyGBC " };

    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (starts_with(line, names[i])) {
            return line + strlen(names[i]);
        }
    }
    return NULL;
}

/* True when the line is "<name><what>"; *rest, when given, is what follows. */
static bool ae350_says(const char *line, const char *what, const char **rest)
{
    const char *tail = ae350_past_name(line);
    const size_t n = strlen(what);

    if (tail == NULL || strncmp(tail, what, n) != 0) {
        return false;
    }
    if (rest != NULL) {
        *rest = tail + n;
    }
    return true;
}

static const char *ae350_error(int rc)
{
    switch (rc) {
    case AE350_ERR_TIMEOUT: return "no reply";
    case AE350_ERR_NAK:     return "refused";
    case AE350_ERR_WRITE:   return "UART1 stopped taking bytes";
    case AE350_ERR_IMAGE:   return "cannot read the file";
    case AE350_ERR_RANGE:   return "not inside the AE350's SDRAM";
    default:                return "failed";
    }
}

/* What the program's last line means for ae350run: keep following it, or
 * stop with the code in *code. */
static bool ae350_line_ends(const ae350_run_t *run, int *code)
{
    const char *rest = NULL;

    if (ae350_says(run->line, "EXIT ", &rest)) {
        *code = atoi(rest);
        return true;
    }
    if (starts_with(run->line, "BAD ")) {
        *code = 1;
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ the save --
 *
 * Battery saves are named after the cartridge: its basename under
 * AE350_SAVE_DIR, with the extension replaced by ".sav".  Save states are
 * named the same way under AE350_STATE_DIR, with ".state".  Both come back
 * through the one receiver below, which writes whichever s_ae350_dest names. */
static char s_ae350_save_path[TDSH_MAX_REAL_PATH];
static char s_ae350_state_path[TDSH_MAX_REAL_PATH];
static char s_ae350_dest[TDSH_MAX_REAL_PATH];
static const char *s_ae350_dest_kind = "save";

static bool ae350_named_path(const char *cart_real, const char *dir,
                             const char *ext, char *out, size_t size)
{
    const char *base = strrchr(cart_real, '/');
    const char *dot;
    size_t stem;
    int n;

    out[0] = '\0';                      /* a caller that finds "" has no save */
    base = base ? base + 1 : cart_real;
    dot = strrchr(base, '.');
    stem = dot && dot != base ? (size_t)(dot - base) : strlen(base);
    if (stem == 0) {
        return false;
    }
    n = snprintf(out, size, "%s/%.*s%s", dir, (int)stem, base, ext);
    return n > 0 && (size_t)n < size;
}

/* The save coming back, in either of two ways.  TinyNES's is hex lines
 * between "TinyNES NVRAM SAVE", which carries the length and CRC-32, and
 * "TinyNES NVRAM DONE", 32 bytes a line, because a line is what the text
 * path frames and it cannot carry arbitrary bytes.  Tiny89's is the bytes
 * themselves between "TinyNES NVRAM DATA" and the length it gave: no byte is
 * special there, so none is escaped and none is expanded, which is what makes
 * a 4 MiB memory image minutes rather than the hours hex would take.  Kept in
 * statics: 8 KiB does not belong on a shell task's stack. */
#define AE350_SAVE_CHUNK 512u
static char s_ae350_save_line[AE350_LINE_MAX];
static size_t s_ae350_save_line_len;
static uint32_t s_ae350_save_expect, s_ae350_save_crc, s_ae350_save_got;
static uint32_t s_ae350_save_running;    /* CRC-32 of what has been written */
static bool s_ae350_save_active, s_ae350_save_failed, s_ae350_save_open;
static FIL s_ae350_save_file;
static bool s_ae350_save_bin;            /* the payload is bytes, not hex */
static uint32_t s_ae350_save_left;       /* payload bytes still due */
static unsigned s_ae350_save_chunk_len;
static uint8_t s_ae350_save_chunk[AE350_SAVE_CHUNK];

static int ae350_hex_digit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

/* Append to the save file, counting and checksumming as it goes.  The CRC is
 * the incremental use of ae350_crc32: feeding it its own result composes, so
 * a save can be checked without ever holding all of it. */
static int ae350_save_write(const uint8_t *bytes, unsigned n)
{
    UINT wrote = 0;

    if (f_write(&s_ae350_save_file, bytes, n, &wrote) != FR_OK || wrote != n) {
        s_ae350_save_failed = true;
        return -1;
    }
    s_ae350_save_running = ae350_crc32(s_ae350_save_running, bytes, n);
    s_ae350_save_got += n;
    return 0;
}

/* One line of hex, written out as it arrives.  A save is 8 KiB and that much
 * contiguous heap is not there while the desktop is resident -- malloc asked
 * for it and was refused on 2026-10-14 -- so nothing is buffered but the line. */
static void ae350_save_hex(const char *line)
{
    uint8_t bytes[64];
    unsigned n = 0;

    for (; *line != '\0'; line += 2) {
        const int high = ae350_hex_digit(line[0]);
        const int low = line[1] != '\0' ? ae350_hex_digit(line[1]) : -1;
        if (high < 0 || low < 0 || !s_ae350_save_open ||
            s_ae350_save_got + n >= s_ae350_save_expect) {
            s_ae350_save_failed = true;
            return;
        }
        bytes[n++] = (uint8_t)(high << 4 | low);
        if (n == sizeof bytes) {
            if (ae350_save_write(bytes, n) != 0) {
                return;
            }
            n = 0;
        }
    }
    if (n != 0) {
        (void)ae350_save_write(bytes, n);
    }
}

/* A binary save's buffer, written out when it fills and when its payload
 * ends: a file call a byte would be 4 MiB of file calls. */
static void ae350_save_flush(void)
{
    if (s_ae350_save_chunk_len == 0) {
        return;
    }
    if (ae350_save_write(s_ae350_save_chunk, s_ae350_save_chunk_len) != 0) {
        s_ae350_save_failed = true;
    }
    s_ae350_save_chunk_len = 0;
}

/* One payload byte of a binary save.  A frame carries up to 128 of them, so
 * this fills a chunk every few frames, and the length the header gave is what
 * ends the payload -- always, failure included.  A save that cannot be written
 * still counts its bytes down rather than stopping at the first bad one: the
 * length is the only thing that says where the payload ends, and a reader that
 * lost count would swallow every byte after it, take the console with it and
 * never return ae350run.  Latched here, the failure ends the payload at the
 * right place, and ae350_save_finish prints it and removes the file. */
static void ae350_save_raw(uint8_t byte)
{
    if (s_ae350_save_left == 0) {
        return;
    }
    s_ae350_save_left--;
    if (!s_ae350_save_open || s_ae350_save_failed) {
        s_ae350_save_failed = true;
        return;
    }
    s_ae350_save_chunk[s_ae350_save_chunk_len++] = byte;
    if (s_ae350_save_chunk_len == sizeof s_ae350_save_chunk) {
        ae350_save_flush();
    }
}

/* Where a save is built: the name a load will find, with ".part" on the end.
 * Nothing loads it, so a save that does not verify is a file nobody sees, and
 * the previous good save is still sitting under the real name. */
static bool ae350_save_temp_path(char *out, size_t size)
{
    int n;

    out[0] = '\0';
    if (s_ae350_dest[0] == '\0') {
        return false;
    }
    n = snprintf(out, size, "%s.part", s_ae350_dest);
    return n > 0 && (size_t)n < size;
}

/* Read a save back and hash it: the length the header gave and the CRC-32 it
 * gave, against what the card actually holds.  From the writer's side a card
 * that did not take the bytes, a file that closed short and a card that is
 * full all look the same, so without this "saved" is a promise rather than a
 * fact.  The buffer is the save's own chunk buffer, empty by the time this
 * runs, so no stack is spent on a payload that may be megabytes. */
static bool ae350_save_verify(const char *path, uint32_t expect, uint32_t crc)
{
    FIL file;
    uint32_t running = 0, total = 0;
    bool ok = false;

    if (f_open(&file, path, FA_READ) != FR_OK) {
        return false;
    }
    for (;;) {
        UINT got = 0;

        if (f_read(&file, s_ae350_save_chunk, sizeof s_ae350_save_chunk,
                   &got) != FR_OK) {
            break;
        }
        if (got == 0) {
            ok = total == expect && running == crc;
            break;
        }
        running = ae350_crc32(running, s_ae350_save_chunk, got);
        total += got;
        if (total > expect) {
            break;
        }
    }
    (void)f_close(&file);
    return ok;
}

/* Open the save file, making the folders a card that has never saved lacks.
 * Both kinds write straight into it as the payload arrives, and what it opens
 * is the temporary rather than the name a load finds: see ae350_save_finish(). */
static bool ae350_save_open_file(void)
{
    char temp[TDSH_MAX_REAL_PATH];

    /* Every folder above the file, from the card's root down: a card that has
     * never saved, or never saved a state, lacks them. */
    char folder[TDSH_MAX_REAL_PATH];
    snprintf(folder, sizeof folder, "%s", s_ae350_dest);
    for (char *slash = strchr(folder + 4, '/'); slash; slash = strchr(slash + 1, '/')) {
        *slash = '\0';
        (void)f_mkdir(folder);
        *slash = '/';
    }
    if (!ae350_save_temp_path(temp, sizeof temp)) {
        return false;
    }
    if (f_open(&s_ae350_save_file, temp, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        tdsh_printf("ae350run: cannot write %s\r\n", temp);
        return false;
    }
    s_ae350_save_open = true;
    return true;
}

/* The header both kinds share: a length and a CRC-32 after a fixed-width
 * prefix, which is the same width whether the payload is hex or bytes.  Every
 * save starts from nothing, so one that follows a failed one inherits none of
 * its state. */
static void ae350_save_arm(const char *counts, bool binary, const char *dest,
                           const char *kind)
{
    unsigned long expect = 0, crc = 0;

    snprintf(s_ae350_dest, sizeof s_ae350_dest, "%s", dest);
    s_ae350_dest_kind = kind;

    s_ae350_save_got = 0;
    s_ae350_save_running = 0;
    s_ae350_save_line_len = 0;
    s_ae350_save_chunk_len = 0;
    s_ae350_save_left = 0;
    s_ae350_save_failed = false;
    s_ae350_save_open = false;
    s_ae350_save_bin = binary;
    s_ae350_save_active = true;
    if (sscanf(counts, "%lu %lx", &expect, &crc) != 2 || expect == 0 ||
        expect > AE350_SAVE_MAX) {
        s_ae350_save_expect = 0;
        s_ae350_save_crc = 0;
        s_ae350_save_failed = true;
        return;
    }
    s_ae350_save_expect = (uint32_t)expect;
    s_ae350_save_crc = (uint32_t)crc;
    s_ae350_save_left = (uint32_t)expect;
    if (s_ae350_dest[0] == '\0') {
        s_ae350_save_failed = true;
        return;
    }
    if (!ae350_save_open_file()) {
        s_ae350_save_failed = true;
    }
}

/* The save arrived: check it twice over, read it back, and only then let it be
 * the save -- or say why not.  What the card holds matters more than what was
 * sent, and the order here is what makes trying again safe: the previous good
 * save is untouched until a new one has been read back and hashed, because
 * this link loses a frame now and then -- about one run in six, measured on
 * 2026-10-16 -- and the answer to that is to press the key again. */
static void ae350_save_finish(void)
{
    const bool whole = !s_ae350_save_failed && s_ae350_save_open &&
                       s_ae350_save_got == s_ae350_save_expect &&
                       s_ae350_save_running == s_ae350_save_crc;
    FRESULT closed = FR_OK;
    char temp[TDSH_MAX_REAL_PATH];
    const bool named = ae350_save_temp_path(temp, sizeof temp);

    if (s_ae350_save_open) {
        /* Sync, then close, and believe the answer: a close that fails leaves
         * the directory entry uncommitted, which is a file of the right size
         * that cannot be opened again -- the state this was in on 2026-10-14. */
        if (whole) {
            (void)f_sync(&s_ae350_save_file);
        }
        closed = f_close(&s_ae350_save_file);
        s_ae350_save_open = false;
    }
    if (!whole || closed != FR_OK) {
        tdsh_printf("ae350run: the %s did not arrive whole: %lu of %lu bytes, "
                    "%lu dropped in the link ring, close %d\r\n",
                    s_ae350_dest_kind, (unsigned long)s_ae350_save_got,
                    (unsigned long)s_ae350_save_expect,
                    (unsigned long)s_rx_drops, (int)closed);
        /* The half-save is removed and the save already on the card is left
         * alone: what must never happen is a partial file under the real name,
         * and what must not happen is losing the last good one to a bad try. */
        if (named) {
            (void)f_unlink(temp);
        }
        return;
    }

    /* Whole as it arrived, which is not the same as whole on the card. */
    if (!named ||
        !ae350_save_verify(temp, s_ae350_save_expect, s_ae350_save_crc)) {
        tdsh_printf("ae350run: the card does not hold the %s that was sent: "
                    "%lu bytes to %s, and %s\r\n",
                    s_ae350_dest_kind, (unsigned long)s_ae350_save_got,
                    named ? temp : s_ae350_dest,
                    s_ae350_dest[0] != '\0' ? "the one already there is "
                    "left alone" : "there was none there to leave");
        if (named) {
            (void)f_unlink(temp);
        }
        return;
    }

    if (named) {
        /* Replace the old save with the verified one.  FatFS will not rename
         * onto a name that exists, so the old one goes first -- after the new
         * one is known good, never before. */
        (void)f_unlink(s_ae350_dest);
        if (f_rename(temp, s_ae350_dest) != FR_OK) {
            tdsh_printf("ae350run: the card will not put %s in place over %s\r\n",
                        temp, s_ae350_dest);
            (void)f_unlink(temp);
            return;
        }
    }
    tdsh_printf("ae350run: saved the %s, %lu bytes to %s, read back and verified %08lx\r\n",
                s_ae350_dest_kind, (unsigned long)s_ae350_save_got, s_ae350_dest,
                (unsigned long)s_ae350_save_running);
}

/* The line that opens a hex save; its hex follows.  Everything from here to
 * the DONE line is consumed in this file so hex never reaches the console. */
static void ae350_save_begin(const char *line)
{
    ae350_save_arm(line + 19, false, s_ae350_save_path, "save");
}

/* The same thing with the bytes themselves as the payload: TinyNES's framing
 * with the data put in whole.  The length the header carries says where the
 * payload ends, so nothing in it needs escaping and nothing is special -- CR,
 * LF and 0xAA are bytes here, and the text reader that would have mangled
 * them is not the one reading this. */
static void ae350_save_data_begin(const char *line)
{
    ae350_save_arm(line + 19, true, s_ae350_save_path, "save");
}

/* A save state, TinyNES's one per cartridge, as bytes: "TinyNES STATE DATA"
 * and the same length and CRC-32, written to the cartridge's .state through
 * the same verified replace as a battery save. */
static void ae350_state_data_begin(const char *line)
{
    ae350_save_arm(line + 19, true, s_ae350_state_path, "state");
}

/* One byte of a save's hex, or of its DONE line. */
static void ae350_save_byte(uint8_t byte)
{
    if (byte == '\r') {
        return;
    }
    if (byte != '\n') {
        if (s_ae350_save_line_len < sizeof(s_ae350_save_line) - 1) {
            s_ae350_save_line[s_ae350_save_line_len++] = (char)byte;
        } else {
            s_ae350_save_failed = true;
        }
        return;
    }
    s_ae350_save_line[s_ae350_save_line_len] = '\0';
    if (starts_with(s_ae350_save_line, "TinyNES NVRAM DONE")) {
        ae350_save_finish();
        s_ae350_save_active = false;
    } else {
        ae350_save_hex(s_ae350_save_line);
    }
    s_ae350_save_line_len = 0;
}

/* After "TinyNES LINK": the core answers in frames now, so the link goes back
 * to everyone (the desk layer draws, its task reads the joypad report) and the
 * program's text is read out of FPGA_RESP_AE350_TEXT frames.  The caller holds
 * the lock and the pipe flag on entry and gets them back on return.  The game
 * starts on screen; the overlay is put back as it was afterwards, so the
 * desktop core comes back the way it was left. */
static int ae350_follow_link(ae350_run_t *run)
{
    taskENTER_CRITICAL();
    s_pipe = false;
    taskEXIT_CRITICAL();
    tang_fpga_unlock();

    const bool was_shown = tang_osd_shown();
    (void)tang_osd_set(false);
    tang_osd_desk_core_reloaded();     /* the layer's enable, and a repaint */

    static uint8_t text[FPGA_FRAMES_PAYLOAD_MAX];
    int code = 1;
    for (unsigned silent = 0;;) {
        const int n = tang_fpga_wait(FPGA_RESP_AE350_TEXT, text, sizeof(text), 1000);
        if (n < 0) {
            if (++silent >= AE350_SILENT_S) {
                tdsh_printf("ae350run: nothing from the program for %u s; leaving it\r\n",
                            (unsigned)AE350_SILENT_S);
                break;
            }
            continue;
        }
        silent = 0;
        bool done = false;
        for (int i = 0; i < n && !done; i++) {
            if (s_ae350_save_active) {
                if (s_ae350_save_bin) {
                    ae350_save_raw(text[i]);
                    if (s_ae350_save_left == 0) {
                        ae350_save_flush();
                        ae350_save_finish();
                        s_ae350_save_active = false;
                        s_ae350_save_bin = false;
                    }
                } else {
                    ae350_save_byte(text[i]);
                }
                continue;
            }
            run->line_ready = false;
            ae350_text(run, text[i]);
            if (!run->line_ready) {
                continue;
            }
            if (starts_with(run->line, "TinyNES NVRAM DATA ")) {
                ae350_save_data_begin(run->line);
                continue;
            }
            if (starts_with(run->line, "TinyNES STATE DATA ")) {
                ae350_state_data_begin(run->line);
                continue;
            }
            if (starts_with(run->line, "TinyNES NVRAM SAVE ")) {
                ae350_save_begin(run->line);
                continue;
            }
            done = ae350_line_ends(run, &code);
        }
        if (done) {
            break;
        }
    }

    (void)tang_osd_set(was_shown);
    tang_fpga_lock();
    taskENTER_CRITICAL();
    s_pipe = true;
    taskEXIT_CRITICAL();
    return code;
}

static uint8_t s_ae350_block[AE350_LOAD_BLOCK];
static FIL s_ae350_elf_file;
static FIL s_ae350_data_file;

/* Answer "TinyNES NVRAM WANT" or "TinyNES STATE WANT" <address> <room> with
 * the cartridge's .sav or .state, the file itself with no header, in the load
 * protocol.  Nothing on the card is the usual case and not an error: the
 * program waits about a second and starts without it. */
static void ae350_send_named(ae350_run_t *run, const ae350_link_t *link,
                             const char *args, const char *path, const char *kind)
{
    unsigned long address = 0, room = 0;
    char reason = 0;

    if (path[0] == '\0' || sscanf(args, "%lx %lx", &address, &room) != 2 ||
        f_open(&s_ae350_save_file, path, FA_READ) != FR_OK) {
        tdsh_printf("ae350run: no %s to send yet\r\n", kind);
        return;
    }
    run->save_file = &s_ae350_save_file;
    run->save_length = (uint32_t)f_size(&s_ae350_save_file);
    if (run->save_length == 0 || run->save_length > (uint32_t)room) {
        tdsh_printf("ae350run: %s is empty or larger than the program's room for it\r\n",
                    path);
    } else {
        ae350_link_t save_link = *link;
        save_link.image = ae350_save_bytes;
        const int rc = ae350_send_image(&save_link, (uint32_t)address,
                                        run->save_length, (uint32_t)address, 5,
                                        s_ae350_block, &reason);
        if (rc != AE350_OK) {
            tdsh_printf("ae350run: sending %s: %s\r\n", path, ae350_error(rc));
        } else {
            tdsh_printf("ae350run: sent %s, %lu bytes\r\n", path,
                        (unsigned long)run->save_length);
        }
    }
    f_close(&s_ae350_save_file);
    run->save_file = NULL;
}

/* Send a settings blob to the address the program asked for, in the boot
 * ROM's load protocol, as a cartridge is sent. */
static int ae350_send_blob(ae350_run_t *run, const ae350_link_t *link,
                           unsigned long address, const uint8_t *blob,
                           size_t length, char *reason)
{
    run->blob = blob;
    run->blob_length = length;
    ae350_link_t set_link = *link;
    set_link.image = ae350_blob_bytes;
    return ae350_send_image(&set_link, (uint32_t)address, length,
                            (uint32_t)address, 5, s_ae350_block, reason);
}

/* Load <player> into a TinyNES core's AE350 and run it, sending <data> (a
 * cartridge, or NULL) when it asks; follow its output until it exits and
 * return its code.  Both paths are FatFS paths the port mounts, already
 * resolved, so this reads no shell session and maps no /root onto /sd:
 * `cmd_ae350run` below resolves the shell's names and calls it, and so does a
 * launch from the desktop's Library page (tang_ae350_run.h). */
int tang_ae350_run(const char *player, const char *data)
{
    if (player == NULL || player[0] == '\0') {
        tdsh_printf("usage: ae350run <program.elf> [<file>]\r\n");
        return 1;
    }
    if (f_open(&s_ae350_elf_file, player, FA_READ) != FR_OK) {
        tdsh_printf("ae350run: cannot open %s\r\n", player);
        return 1;
    }
    ae350_run_t run = { .elf_file = &s_ae350_elf_file };
    /* The settings this launch will use, read once.  /nes.ini and the AE350's
     * mapping are the same choice seen from two sides. */
    static tang_nes_settings_t nes;
    (void)tang_nes_load(&nes, TANG_NES_INI_REAL);
    /* No cartridge last time does not mean one now: the path must not
     * survive from a previous run. */
    s_ae350_save_path[0] = '\0';
    s_ae350_state_path[0] = '\0';
    if (data != NULL) {
        if (f_open(&s_ae350_data_file, data, FA_READ) != FR_OK) {
            f_close(&s_ae350_elf_file);
            tdsh_printf("ae350run: cannot open %s\r\n", data);
            return 1;
        }
        run.data_file = &s_ae350_data_file;
        run.data_length = (uint32_t)f_size(&s_ae350_data_file);
        (void)ae350_named_path(data, AE350_SAVE_DIR, ".sav", s_ae350_save_path,
                               sizeof(s_ae350_save_path));
        (void)ae350_named_path(data, AE350_STATE_DIR, ".state", s_ae350_state_path,
                               sizeof(s_ae350_state_path));
    }
    static ae350_elf_t elf;
    int rc = ae350_elf_parse(&elf, ae350_file_read, &s_ae350_elf_file);
    if (rc != AE350_OK) {
        tdsh_printf("ae350run: %s: %s\r\n", player, ae350_error(rc));
        goto close_files;
    }
    run.elf = &elf;
    if (tang_fpga_link_open() != 0) {
        tdsh_printf("ae350run: cannot bring up UART1\r\n");
        rc = -1;
        goto close_files;
    }

    tang_fpga_lock();
    taskENTER_CRITICAL();
    s_pipe = true;
    taskEXIT_CRITICAL();
    tang_fpga_drain();

    /* The boot ROM says it is waiting about once a second. */
    bool waiting = false;
    for (int i = 0; i < 5 && !waiting; i++) {
        waiting = ae350_next_line(&run, 1500) && ae350_says(run.line, "LOAD", NULL);
    }
    int code = 1;
    const ae350_link_t link = {
        .ctx = &run, .write = ae350_write, .read_byte = ae350_read_byte,
        .text = ae350_text, .image = ae350_program_bytes,
    };
    char reason = 0;
    if (!waiting) {
        tdsh_printf("ae350run: no boot ROM is waiting on UART1 (is the core loaded?)\r\n");
        goto release;
    }
    tdsh_printf("ae350run: %s, %lu bytes at 0x%08lx\r\n", player,
                (unsigned long)(elf.end - elf.base), (unsigned long)elf.base);
    rc = ae350_send_image(&link, elf.base, elf.end - elf.base, elf.entry, 5,
                          s_ae350_block, &reason);
    if (rc != AE350_OK) {
        tdsh_printf("ae350run: loading %s: %s%s%c\r\n", player, ae350_error(rc),
                    reason ? ", reason " : "", reason ? reason : ' ');
        goto release;
    }

    /* Follow the program: send the file when asked, stop at its exit, or
     * give up on one silent for AE350_SILENT_S, which has hung. */
    for (unsigned silent = 0;;) {
        const char *rest = NULL;
        if (!ae350_next_line(&run, 1000)) {
            if (++silent >= AE350_SILENT_S) {
                tdsh_printf("ae350run: nothing from the program for %u s; leaving it\r\n",
                            (unsigned)AE350_SILENT_S);
                break;
            }
            continue;
        }
        silent = 0;
        if (ae350_says(run.line, "WANT ", &rest)) {
            unsigned long address = 0, room = 0;
            if (run.data_file == NULL ||
                sscanf(rest, "%lx %lx", &address, &room) != 2 ||
                run.data_length + AE350_FILE_HEADER > room) {
                tdsh_printf("ae350run: the program wants a file it cannot have\r\n");
                continue;
            }
            ae350_link_t file_link = link;
            file_link.image = ae350_payload_bytes;
            rc = ae350_send_image(&file_link, (uint32_t)address,
                                  run.data_length + AE350_FILE_HEADER,
                                  (uint32_t)address, 5, s_ae350_block, &reason);
            if (rc != AE350_OK) {
                tdsh_printf("ae350run: sending %s: %s\r\n", data, ae350_error(rc));
            }
            continue;
        }
        if (starts_with(run.line, "TinyNES NVRAM WANT ")) {
            ae350_send_named(&run, &link, run.line + 19, s_ae350_save_path, "save");
            continue;
        }
        if (starts_with(run.line, "TinyNES STATE WANT ")) {
            ae350_send_named(&run, &link, run.line + 19, s_ae350_state_path, "state");
            continue;
        }
        if (starts_with(run.line, "TinyNES SETTINGS WANT ")) {
            /* The blob's bytes are the ABI with the core (TinyNES
             * firmware/apps/nes_bench/main.cpp): player 1's mapping, player
             * 2's, the picture's pixel aspect, its palette, then the master
             * volume and the mute.  The core prints the length it wants, and
             * an older core wants only the first sixteen to eighteen of these,
             * so nothing beyond what was asked for is ever sent. */
            static uint8_t blob[2 * TANG_NES_BUTTONS + 4];
            unsigned long address = 0, room = 0;
            if (sscanf(run.line + 22, "%lx %lx", &address, &room) != 2 ||
                room < 2 * TANG_NES_BUTTONS) {
                tdsh_printf("ae350run: the settings request is malformed\r\n");
                continue;
            }
            memcpy(blob, nes.p1, TANG_NES_BUTTONS);
            memcpy(blob + TANG_NES_BUTTONS, nes.p2, TANG_NES_BUTTONS);
            blob[2 * TANG_NES_BUTTONS] = nes.aspect;
            blob[2 * TANG_NES_BUTTONS + 1] = nes.palette;
            blob[2 * TANG_NES_BUTTONS + 2] = nes.audio.volume;
            blob[2 * TANG_NES_BUTTONS + 3] = nes.audio.mute;
            const size_t sending = room < sizeof blob ? (size_t)room : sizeof blob;
            rc = ae350_send_blob(&run, &link, address, blob, sending, &reason);
            tdsh_printf("ae350run: settings %s, p1:",
                        nes.found ? TANG_NES_INI_SHOWN : "(defaults)");
            for (int i = 0; i < TANG_NES_BUTTONS; i++) {
                tdsh_printf(" %s=%s", tang_nes_button_name(i),
                            tang_nes_pad_name(nes.p1[i]));
            }
            tdsh_printf(" aspect=%s palette=%s volume=%u%s%s\r\n",
                        tang_nes_aspect_name(nes.aspect),
                        tang_nes_palette_name(nes.palette),
                        (unsigned)nes.audio.volume, nes.audio.mute ? " muted" : "",
                        sending < sizeof blob ? " (not all asked for)" : "");
            if (rc != AE350_OK) {
                tdsh_printf("ae350run: sending the settings: %s\r\n", ae350_error(rc));
            }
            continue;
        }
        if (starts_with(run.line, "TinyGBC SETTINGS WANT ")) {
            /* TinyGBC's blob (TinyGBC firmware/apps/gb_bench/main.cpp): the
             * master volume and the mute, from /gbc.ini. */
            static tang_gbc_settings_t gbc;
            static uint8_t blob[2];
            unsigned long address = 0, room = 0;
            if (sscanf(run.line + 22, "%lx %lx", &address, &room) != 2 || room < 1) {
                tdsh_printf("ae350run: the settings request is malformed\r\n");
                continue;
            }
            (void)tang_gbc_load(&gbc, TANG_GBC_INI_REAL);
            blob[0] = gbc.audio.volume;
            blob[1] = gbc.audio.mute;
            const size_t sending = room < sizeof blob ? (size_t)room : sizeof blob;
            rc = ae350_send_blob(&run, &link, address, blob, sending, &reason);
            tdsh_printf("ae350run: settings %s, volume=%u%s\r\n",
                        gbc.found ? TANG_GBC_INI_SHOWN : "(defaults)",
                        (unsigned)gbc.audio.volume, gbc.audio.mute ? " muted" : "");
            if (rc != AE350_OK) {
                tdsh_printf("ae350run: sending the settings: %s\r\n", ae350_error(rc));
            }
            continue;
        }
        if (ae350_says(run.line, "LINK", NULL)) {
            code = ae350_follow_link(&run);
            break;
        }
        if (ae350_says(run.line, "EXIT ", &rest)) {
            code = atoi(rest);
            break;
        }
        if (ae350_says(run.line, "LOAD", NULL)) {
            tdsh_printf("ae350run: the program stopped without exiting\r\n");
            break;
        }
    }

release:
    tang_fpga_drain();
    taskENTER_CRITICAL();
    s_pipe = false;
    taskEXIT_CRITICAL();
    tang_fpga_unlock();
    f_close(&s_ae350_elf_file);
    if (run.data_file) {
        f_close(&s_ae350_data_file);
    }
    return code;

close_files:
    f_close(&s_ae350_elf_file);
    if (run.data_file) {
        f_close(&s_ae350_data_file);
    }
    return 1;
}

/* The shell's half: turn the names the shell takes into real paths and hand
 * them to tang_ae350_run(), which is the whole of the work. */
static int cmd_ae350run(tdsh_session_t *session, int argc, char **argv)
{
    if (argc < 2) {
        tdsh_printf("usage: ae350run <program.elf> [<file>]\r\n");
        return 1;
    }
    char player[TDSH_MAX_REAL_PATH];
    if (!resolve_path(session, argv[1], player, sizeof(player))) {
        tdsh_printf("ae350run: cannot open %s\r\n", argv[1]);
        return 1;
    }
    char data[TDSH_MAX_REAL_PATH];
    if (argc > 2) {
        if (!resolve_path(session, argv[2], data, sizeof(data))) {
            tdsh_printf("ae350run: cannot open %s\r\n", argv[2]);
            return 1;
        }
        return tang_ae350_run(player, data);
    }
    return tang_ae350_run(player, NULL);
}

/* ------------------------------------------------------------ registration */

static const tdsh_command_t s_fpga_commands[] = {
    { "nesload", "nesload <path>", "Stream an iNES ROM into the running core and start it",
      cmd_nesload, 0 },
    { "fpga", "fpga", "Ask the loaded core for its ID over UART1",
      cmd_fpga, 0 },
    { "fmon", "fmon [hold-ms]", "Read the core's clock-monitor counters over UART1",
      cmd_fmon, 0 },
    { "fpgar", "fpgar <hex-addr>", "Read one desktop register over UART1",
      cmd_fpgar, 0 },
    { "fpgapipe", "fpgapipe", "Pass console bytes to and from UART1 until the port closes",
      cmd_fpgapipe, 0 },
    { "ae350run", "ae350run <program.elf> [<file>]",
      "Load a program into a TinyNES core's AE350 and serve its files",
      cmd_ae350run, 0 },
};

int tdsh_bl616_fpga_register(void)
{
    return tdsh_register_commands(s_fpga_commands,
                                  sizeof(s_fpga_commands) / sizeof(s_fpga_commands[0]));
}
