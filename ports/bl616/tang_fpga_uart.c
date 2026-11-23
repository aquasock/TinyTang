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
#include "tang_gba_load.h"
#include "tang_gba_ini.h"

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
#include <stdarg.h>
#include "tang_osd.h"
#include "tang_osd_desk.h"
#include "tang_pipe_sniff.h"
#include "tang_snes_load.h"
#include "tang_clock.h"
#include "tang_plane.h"

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
static TaskHandle_t         s_lock_owner;   /* who holds it, for re-entry */
static uint32_t             s_lock_depth;

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

bool tang_fpga_wii_report(uint8_t r[6], uint32_t *seq)
{
    rx_decode();
    taskENTER_CRITICAL();
    const bool ok = fpga_frames_wii(r, seq);
    taskEXIT_CRITICAL();
    return ok;
}

/* The transmit lock.  A FreeRTOS mutex rather than a critical section:
 * interrupts stay enabled, because the RX interrupt is what keeps the core's
 * replies from overflowing the 32-byte FIFO.
 *
 * It is re-entrant by owner.  A caller that holds the link across a whole
 * request and its reply -- which EXTCTL-005 requires, or the reply can be
 * overtaken -- calls the register helpers inside that hold, and those helpers
 * take it again; a plain FreeRTOS mutex would deadlock the task against
 * itself.  The depth is counted per owner instead, so one task nests freely
 * while another still waits. */
void tang_fpga_lock(void)
{
    if (!s_tx_lock) {
        return;
    }
    const TaskHandle_t me = xTaskGetCurrentTaskHandle();
    if (s_lock_owner == me) {
        s_lock_depth++;
        return;
    }
    xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    s_lock_owner = me;
    s_lock_depth = 1;
}

void tang_fpga_unlock(void)
{
    if (!s_tx_lock) {
        return;
    }
    if (s_lock_owner != xTaskGetCurrentTaskHandle()) {
        return;                     /* not ours to give */
    }
    if (s_lock_depth > 0) {
        s_lock_depth--;
    }
    if (s_lock_depth == 0) {
        s_lock_owner = NULL;
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
    const TaskHandle_t me = xTaskGetCurrentTaskHandle();
    if (s_lock_owner == me) {
        s_lock_depth++;
        return true;
    }
    if (xSemaphoreTake(s_tx_lock, pdMS_TO_TICKS(ms)) != pdTRUE) {
        return false;
    }
    s_lock_owner = me;
    s_lock_depth = 1;
    return true;
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
/* One register transaction's budget.  The acquisition is bounded so a caller
 * that never gives the link back -- a host pipe, a load, a stuck task -- fails
 * this read instead of blocking the caller for ever, and the hold covers the
 * reply as well as the frame, because releasing it after the transmit lets
 * another caller's frame overtake the answer (EXTCTL-005). */
#define FPGA_REG_LOCK_MS   100u
#define FPGA_REG_REPLY_MS  500u

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

    if (!tang_fpga_lock_within(FPGA_REG_LOCK_MS)) return false;
    const int sent = tang_fpga_frame(0x10, req, sizeof(req));
    uint8_t rep[16];
    const int got = sent == 0 ? tang_fpga_wait(0x10, rep, sizeof(rep), FPGA_REG_REPLY_MS) : 0;
    tang_fpga_unlock();
    if (sent != 0) return false;
    if (got != 15) return false;
    if (status) *status = rep[2];
    if (value)
        *value = ((uint32_t)rep[9] << 24) | ((uint32_t)rep[10] << 16) |
                 ((uint32_t)rep[11] << 8) | (uint32_t)rep[12];
    return true;
}

/* The desktop core's ABI word, for a caller that has just loaded a core and
 * wants to know what its desk layer's font can draw.  Register 0 must hold the
 * desktop's magic, so a core that answers the extended frame but is not the
 * desktop is not mistaken for one; a core that does not answer at all costs
 * one reply timeout per try.  A freshly configured core is given three tries. */
bool tang_fpga_desktop_abi(uint32_t *abi)
{
    if (tang_fpga_link_open() != 0) {
        return false;
    }
    for (int attempt = 0; attempt < 3; attempt++) {
        uint32_t magic = 0, word = 0;
        uint8_t st0 = 1, st1 = 1;
        if (fpga_ext_read32(0x00u, &magic, &st0) && st0 == 0 &&
            fpga_ext_read32(0x04u, &word, &st1) && st1 == 0) {
            if (magic != 0x00544453u) {
                return false;
            }
            *abi = word;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
}

/* Write one 32-bit register through the same endpoint: opcode 2 carries the
 * value in bytes 8..11, big-endian, and the reply's status says whether the
 * register accepted it.  Same 14-byte request, same CRC, same 15-byte reply. */
static bool fpga_ext_write32(uint32_t address, uint32_t value, uint8_t *status)
{
    uint8_t req[14];
    memset(req, 0, sizeof(req));
    req[0] = 1;                                   /* extended-request version */
    req[1] = 2;                                   /* opcode: write32          */
    req[4] = (uint8_t)(address >> 24);
    req[5] = (uint8_t)(address >> 16);
    req[6] = (uint8_t)(address >> 8);
    req[7] = (uint8_t)address;
    req[8] = (uint8_t)(value >> 24);
    req[9] = (uint8_t)(value >> 16);
    req[10] = (uint8_t)(value >> 8);
    req[11] = (uint8_t)value;
    uint16_t crc = fpga_crc16(0xffffu, 0x10u);
    for (int i = 0; i < 12; i++) crc = fpga_crc16(crc, req[i]);
    req[12] = (uint8_t)(crc >> 8);
    req[13] = (uint8_t)crc;

    if (!tang_fpga_lock_within(FPGA_REG_LOCK_MS)) return false;
    const int sent = tang_fpga_frame(0x10, req, sizeof(req));
    uint8_t rep[16];
    const int got = sent == 0 ? tang_fpga_wait(0x10, rep, sizeof(rep), FPGA_REG_REPLY_MS) : 0;
    tang_fpga_unlock();
    if (sent != 0) return false;
    if (got != 15) return false;
    if (status) *status = rep[2];
    return true;
}

/* Public register access, so a driver can claim the UART window or any other
 * register without growing a copy of the request format.  Returns false when
 * the core did not answer or answered with a non-zero status. */
bool tang_fpga_read32(uint32_t address, uint32_t *value)
{
    uint8_t status = 0xff;
    if (!fpga_ext_read32(address, value, &status)) return false;
    return status == 0;
}

bool tang_fpga_write32(uint32_t address, uint32_t value)
{
    uint8_t status = 0xff;
    if (!fpga_ext_write32(address, value, &status)) return false;
    return status == 0;
}

/* ------------------------------------------------------------ the link's rate
 *
 * Every core comes out of configuration at 2 Mbaud.  The desktop core from ABI
 * 1.10 (fpga/desktop/link_uart.sv) also runs at 5 or 10 Mbaud when asked with
 * register opcode 0x03 (EXTCTL-001): it answers at the old rate and moves once
 * the answer's stop bit has left, and this end moves when the answer arrives.
 * Those are the only rates both ends make exactly: the core times bits on its
 * 50 MHz crystal, this UART on 40 MHz with a whole-number divider (25 and 20
 * clocks at 2 Mbaud, 10 and 8 at 5, 5 and 4 at 10).
 *
 * The way back needs no answer.  At 5 or 10 Mbaud the core takes a line held
 * low for 3 us as the BL616 talking at 2 Mbaud and drops to it; a 0x00 at
 * 2 Mbaud is 4.5 us low.  So zeros at 2 Mbaud bring both ends back from any
 * state, and every core's frame parser ignores them between frames. */
#define FPGA_LINK_FAST_BAUD   10000000u
#define FPGA_LINK_SAFE_BAUD    5000000u
#define FPGA_REG_LINK_RATE    0x1Cu       /* the core's rate, bits per second */

static void link_zeros(void)
{
    static const uint8_t zeros[4] = { 0, 0, 0, 0 };
    (void)uart_write(zeros, sizeof zeros);
    while (bflb_uart_txempty(s_uart) == 0) {
    }
    /* The core's fallback crosses two clock domains: well under a
     * microsecond.  The zeros it then reads at 2 Mbaud are ignored. */
    bflb_mtimer_delay_us(20);
    tang_fpga_drain();
}

void tang_fpga_link_slow(void)
{
    if (tang_fpga_link_open() != 0) {
        return;
    }
    tang_fpga_lock();
    if (s_baud != FPGA_UART_BAUD) {
        (void)tang_fpga_set_baud(FPGA_UART_BAUD);
    }
    link_zeros();
    tang_fpga_unlock();
}

/* One opcode-0x03 request, read back at the new rate.  The core's answer
 * names the rate it accepted; after both ends move, register 0x1C must say the
 * same thing, CRC-checked, at the new rate.  Anything less and both ends go
 * back to 2 Mbaud. */
static bool link_negotiate(uint32_t baud)
{
    uint8_t req[14];
    memset(req, 0, sizeof(req));
    req[0] = 1;                                   /* extended-request version */
    req[1] = 3;                                   /* opcode: negotiated baud  */
    req[8] = (uint8_t)(baud >> 24);
    req[9] = (uint8_t)(baud >> 16);
    req[10] = (uint8_t)(baud >> 8);
    req[11] = (uint8_t)baud;
    uint16_t crc = fpga_crc16(0xffffu, 0x10u);
    for (int i = 0; i < 12; i++) crc = fpga_crc16(crc, req[i]);
    req[12] = (uint8_t)(crc >> 8);
    req[13] = (uint8_t)crc;

    tang_fpga_drain();
    if (tang_fpga_frame(0x10, req, sizeof(req)) != 0) {
        return false;
    }
    uint8_t rep[16];
    const int got = tang_fpga_wait(0x10, rep, sizeof(rep), FPGA_REG_REPLY_MS);
    if (got != 15 || rep[1] != 0x83u || rep[2] != 0) {
        /* Refused, or an answer lost: the core may or may not have moved,
         * and the zeros are harmless either way. */
        link_zeros();
        return false;
    }
    uint16_t rcrc = fpga_crc16(0xffffu, 0x10u);
    for (int i = 0; i < 13; i++) rcrc = fpga_crc16(rcrc, rep[i]);
    const uint32_t agreed = ((uint32_t)rep[9] << 24) | ((uint32_t)rep[10] << 16) |
                            ((uint32_t)rep[11] << 8) | (uint32_t)rep[12];
    if (rcrc != (uint16_t)((rep[13] << 8) | rep[14]) || agreed != baud) {
        link_zeros();                             /* it may have moved: undo */
        return false;
    }
    /* The answer's last byte is in; the core moves a few of its clocks after
     * that stop bit. */
    bflb_mtimer_delay_us(20);
    (void)tang_fpga_set_baud(baud);
    for (int attempt = 0; attempt < 2; attempt++) {
        uint32_t v = 0;
        if (tang_fpga_read32(FPGA_REG_LINK_RATE, &v) && v == baud) {
            return true;
        }
    }
    (void)tang_fpga_set_baud(FPGA_UART_BAUD);
    link_zeros();
    return false;
}

uint32_t tang_fpga_link_fast(void)
{
    if (tang_fpga_link_open() != 0) {
        return s_baud;
    }
    tang_fpga_lock();
    if (s_baud == FPGA_UART_BAUD) {
        if (!link_negotiate(FPGA_LINK_FAST_BAUD)) {
            (void)link_negotiate(FPGA_LINK_SAFE_BAUD);
        }
    }
    const uint32_t now = s_baud;
    tang_fpga_unlock();
    return now;
}

int tang_fpga_link_rate(uint32_t baud)
{
    if (baud != FPGA_UART_BAUD && baud != FPGA_LINK_SAFE_BAUD && baud != FPGA_LINK_FAST_BAUD) {
        return -1;
    }
    if (tang_fpga_link_open() != 0) {
        return -1;
    }
    tang_fpga_lock();
    int rc = 0;
    /* Already there, at a rate that was negotiated and confirmed: nothing to
     * do.  2 Mbaud is never taken on trust, since a BL616 that was reset or
     * retuned with -raw believes it while the core may not: asking for it
     * always sends the zeros that bring the core back. */
    if (baud != s_baud || baud == FPGA_UART_BAUD) {
        /* From 2 Mbaud, always: the request then goes at the rate every core
         * reads, and a failure leaves both ends there. */
        tang_fpga_link_slow();
        if (baud != FPGA_UART_BAUD && !link_negotiate(baud)) {
            rc = -1;
        }
    }
    tang_fpga_unlock();
    return rc;
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

/* The TinyGBA core's loader.
 *
 * It sends the same three frame types nesload does, because both cores are
 * nand2mario's and `iosys_bl616.v`'s command table is the same in each: 0x06
 * to hold the core in reset and to release it, 0x07 for cartridge bytes, 0x08
 * for the overlay.  What differs is what the file is.  A GBA cartridge is a
 * raw image with no container header, so the size is the file's size and the
 * only thing worth refusing is a file that is not a cartridge at all -- a GBA
 * header carries a fixed 0x96 at 0xB2, which one read settles.  The BIOS on the
 * cartridge checks the Nintendo logo itself, so validating it here would only
 * repeat work the machine is about to do.
 *
 * The 1024-byte chunk and the taskYIELD between chunks are load-bearing: the
 * core's receiver drops back to hunting for a magic byte when a frame's length
 * high byte reaches 8, and the BL616 needs to run its USB and console tasks or
 * the host sees the link die part-way through a large cartridge. */
/* TANG_GBA_MAX_IMAGE, TANG_GBA_HEADER_* and TANG_GBA_BACKUP_* live in
 * tang_gba_load.h, because the desk app needs the save-type numbers too and two
 * copies of a wire protocol is how they drift. */

/* Save hardware, as the core's `config_backup_type` numbers them
 * (fpga/gba/src/memory/gba_memory.sv).  Getting this wrong is not cosmetic:
 * with `BACKUP_NONE` the core treats the cartridge's save window as read-only,
 * every write the game makes is discarded, and a game that checks its save at
 * boot -- Pokemon Emerald does -- stops on the BIOS handover's white screen.
 * That is what BRD-080 recorded and BRD-088 diagnosed. */

/* Every GBA cart that uses save hardware carries a marker string from the
 * library it was linked against, and that string is how the save type is
 * determined: `FLASH1M_V` for 1 Mbit flash, `FLASH512_V` and `FLASH_V` for
 * 512 Kbit, `SRAM_V` and `SRAM_F_V` for SRAM (`_F_` is the fast variant), and
 * `EEPROM_V` for EEPROM.  Longer, more specific markers are matched first, so
 * `FLASH1M_V` cannot be mistaken for `FLASH_V`.  Measured on 2026-11-14 against
 * thirteen cartridges in this project's collection: all thirteen are detected,
 * which is why the list carries both SRAM spellings -- Castlevania: Aria of
 * Sorrow and Fire Emblem use `SRAM_F_V` and were missed by `SRAM_V` alone. */
static const struct {
    const char *mark;
    uint8_t     type;
} s_gba_save_marks[] = {
    { "FLASH1M_V",  TANG_GBA_BACKUP_FLASH1M   },
    { "FLASH512_V", TANG_GBA_BACKUP_FLASH512K },
    { "FLASH_V",    TANG_GBA_BACKUP_FLASH512K },
    { "SRAM_F_V",   TANG_GBA_BACKUP_SRAM      },
    { "SRAM_V",     TANG_GBA_BACKUP_SRAM      },
    { "EEPROM_V",   TANG_GBA_BACKUP_EEPROM    },
};
#define TANG_GBA_SAVE_MARK_MAX 10u   /* longest marker, for the overlap carry */

/* Find a save marker in one chunk, carrying the tail of the previous chunk so a
 * marker split across a read boundary is still seen.  Returns the type, or 0
 * when this chunk holds none.
 *
 * Two searches, because one is not enough: the whole chunk, and then the join
 * of the previous chunk's tail with the start of this one.  A first version
 * searched only a 20-byte window at the start of the chunk and missed
 * `FLASH1M_V` in Pokemon Emerald, which sits at offset 192 of its chunk --
 * the marker can be anywhere in the image, so the scan has to cover all of it.
 */
static uint8_t gba_scan_saves(const uint8_t *buf, size_t len, uint8_t *tail,
                              size_t *tail_len)
{
    const size_t nmarks = sizeof s_gba_save_marks / sizeof s_gba_save_marks[0];

    /* 1. anywhere in this chunk */
    for (size_t i = 0; i < len; i++) {
        for (size_t m = 0; m < nmarks; m++) {
            const char  *mark = s_gba_save_marks[m].mark;
            const size_t ml   = strlen(mark);
            if (i + ml <= len && memcmp(&buf[i], mark, ml) == 0)
                return s_gba_save_marks[m].type;
        }
    }

    /* 2. straddling the boundary between the previous chunk and this one */
    uint8_t join[TANG_GBA_SAVE_MARK_MAX * 2];
    size_t  n = 0;
    for (size_t i = 0; i < *tail_len && n < sizeof join; i++)
        join[n++] = tail[i];
    for (size_t i = 0; i < len && n < sizeof join; i++)
        join[n++] = buf[i];
    for (size_t i = 0; i < n; i++) {
        for (size_t m = 0; m < nmarks; m++) {
            const char  *mark = s_gba_save_marks[m].mark;
            const size_t ml   = strlen(mark);
            if (i + ml <= n && memcmp(&join[i], mark, ml) == 0)
                return s_gba_save_marks[m].type;
        }
    }

    *tail_len = len < TANG_GBA_SAVE_MARK_MAX ? len : TANG_GBA_SAVE_MARK_MAX;
    if (*tail_len)
        memcpy(tail, &buf[len - *tail_len], *tail_len);
    return 0u;
}

/* Defined below, next to the rest of the save path; declared here because the
 * loader calls it before the core is released. */
/* Says what it did through `gba_say`, so the desk app's window learns whether
 * the save came back as well as the console does. */
static void gba_save_restore(char *why, size_t why_len, const char *rom_real,
                             uint8_t type);

/* Load the cartridge at `real` (a FatFS path, /sd/...) into the running
 * TinyGBA core and start it.  0 on success; otherwise non-zero with the reason
 * in `why`.  `label` is what the caller called the file, used in messages.
 *
 * TinySNES has tang_snes_load() for exactly this reason and its header says why:
 * the shell command and the desk app both call one function, so the two cannot
 * drift apart.  This is the same arrangement, kept in this file rather than moved
 * to a new one so that the load path this project has proved on hardware many
 * times is edited in place rather than relocated. */
/* Say one line to both the console and the caller's `why` buffer.  The desk app
 * has no console of its own while it is hidden behind a game, so the same text
 * has to reach a window; the shell wants it on the terminal.  One function for
 * both keeps the two from drifting, which is the whole reason the load is shared
 * rather than duplicated. */
static void gba_say(char *why, size_t why_len, const char *fmt, ...)
{
    char    line[160];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    tdsh_printf("%s", line);
    if (why != NULL && why_len > 0) {
        /* The trailing CRLF belongs on a console, not in a widget. */
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        snprintf(why, why_len, "%s", line);
    }
}

/* Frame 0x1A in the TinyGBA core: the pad map, seven bytes
 * (tang_gba_ini.h). */
#define GBA_PAD_MAP 0x1Au

int tang_gba_load(const char *label, const char *real, char *why, size_t why_len)
{

    FIL file;
    if (f_open(&file, real, FA_READ) != FR_OK) {
        gba_say(why, why_len, "cannot open %s\r\n", label);
        return 1;
    }

    const uint32_t size = (uint32_t)f_size(&file);
    if (size < TANG_GBA_HEADER_MIN) {
        f_close(&file);
        gba_say(why, why_len, "%s is %u bytes, too short to be a cartridge\r\n",
                    label, (unsigned)size);
        return 1;
    }
    if (size > TANG_GBA_MAX_IMAGE) {
        f_close(&file);
        gba_say(why, why_len, "%u bytes is more than the 32 MiB a GBA cartridge can be\r\n",
                    (unsigned)size);
        return 1;
    }

    uint8_t fixed = 0;
    UINT got = 0;
    if (f_lseek(&file, TANG_GBA_HEADER_FIXED) != FR_OK ||
        f_read(&file, &fixed, 1, &got) != FR_OK || got != 1u ||
        fixed != TANG_GBA_HEADER_VALUE) {
        f_close(&file);
        gba_say(why, why_len, "%s is not a GBA cartridge (no 0x96 at 0xB2)\r\n", label);
        return 1;
    }
    if (f_lseek(&file, 0) != FR_OK) {
        f_close(&file);
        gba_say(why, why_len, "cannot rewind\r\n");
        return 1;
    }

    if (tang_fpga_link_open() != 0) {
        f_close(&file);
        gba_say(why, why_len, "cannot bring up UART1\r\n");
        return 1;
    }

    gba_say(why, why_len, "%u bytes -> the core at %u baud\r\n",
                (unsigned)size, (unsigned)s_baud);

    /* The pad map, before the cartridge: the pads are decoded in fabric, so a
     * remapping can only be honoured by the core, and the file's [input]
     * section goes as frame 0x1A (tinygba_pads.sv).  A failure is not fatal --
     * a core that never hears the frame keeps the mapping it has always had --
     * but it is said. */
    tang_gba_settings_t pads;
    (void)tang_gba_settings_load(&pads, TANG_GBA_INI_REAL);
    uint8_t map_body[7];
    tang_gba_map_bytes(pads.map, map_body);

    tang_fpga_lock();
    tang_fpga_drain();
    if (tang_fpga_frame(GBA_PAD_MAP, map_body, sizeof map_body) != 0) {
        gba_say(why, why_len, "could not send the pad map (continuing)\r\n");
    }

    /* Non-zero holds the core in reset while the cartridge goes in. */
    if (tang_fpga_frame(FPGA_CMD_SET_LOAD, (const uint8_t[]){ 1 }, 1) != 0) {
        tang_fpga_unlock();
        f_close(&file);
        gba_say(why, why_len, "cannot start the load\r\n");
        return 1;
    }

    /* Save-type detection state, carried across chunks so a marker split over
     * a read boundary is still found. */
    uint8_t  save_type = TANG_GBA_BACKUP_NONE;
    uint8_t  save_tail[TANG_GBA_SAVE_MARK_MAX];
    size_t   save_tail_len = 0;
    bool     save_scan = true;

    uint32_t sent_total = 0;
    for (;;) {
        UINT br = 0;
        if (f_read(&file, s_rom_chunk, FPGA_ROM_CHUNK, &br) != FR_OK) {
            tang_fpga_unlock();
            f_close(&file);
            gba_say(why, why_len, "SD read failed at %u\r\n", (unsigned)sent_total);
            return 1;
        }
        if (br == 0) {
            break;
        }
        if (tang_fpga_frame(FPGA_CMD_ROM_DATA, s_rom_chunk, br) != 0) {
            tang_fpga_unlock();
            f_close(&file);
            gba_say(why, why_len, "the core stopped accepting data at %u\r\n",
                        (unsigned)sent_total);
            return 1;
        }
        sent_total += br;
        /* The save marker is looked for as the image goes past, so the file is
         * read once.  Detection has to finish before the stream does, because
         * the type is sent in the configuration stage afterwards. */
        if (save_scan) {
            save_type = gba_scan_saves(s_rom_chunk, br, save_tail, &save_tail_len);
            if (save_type != TANG_GBA_BACKUP_NONE)
                save_scan = false;
        }
        if (br < FPGA_ROM_CHUNK) {
            break;
        }
        tang_fpga_unlock();
        taskYIELD();                    /* keep the console and USB alive */
        tang_fpga_lock();
    }

    tang_fpga_unlock();

    /* Configure the save hardware.  The core takes `config_backup_type` from
     * the first byte of the loader's configuration stage (`loading == 3`), and
     * a loader that never enters that stage leaves it `BACKUP_NONE` -- which
     * makes the cartridge's save window read-only and stops any game that
     * checks its save at boot.  Pokemon Emerald is exactly that case: BRD-080
     * recorded it stopping at the BIOS handover, BRD-088 showed the save type
     * alone fixes it.  Sending the stage does not disturb the image; the core
     * only re-seeds its own address counter. */
    if (save_type != TANG_GBA_BACKUP_NONE) {
        static const char *const names[] = { "none", "flash512K", "flash1M",
                                             "SRAM", "EEPROM" };
        tang_fpga_lock();
        int cfg = tang_fpga_frame(FPGA_CMD_SET_LOAD, (const uint8_t[]){ 3 }, 1);
        if (cfg == 0)
            cfg = tang_fpga_frame(FPGA_CMD_ROM_DATA,
                                  (const uint8_t[]){ save_type, 0, 0, 0 }, 4);
        tang_fpga_unlock();
        if (cfg != 0) {
            gba_say(why, why_len, "could not set the save type (continuing)\r\n");
        } else {
            gba_say(why, why_len, "save hardware = %s\r\n",
                        names[save_type <= TANG_GBA_BACKUP_EEPROM ? save_type : 0]);
        }
    } else {
        gba_say(why, why_len, "no save marker found; save hardware left off\r\n");
    }

    /* Put the cartridge's stored save back while the core is still held.  The
     * engine writes straight into the save window, so this has to happen after
     * the image is in and before the core is released -- and it must not happen
     * before the save type is configured, because the EEPROM is only reachable
     * once the core knows the cartridge has one. */
    if (save_type != TANG_GBA_BACKUP_NONE)
        gba_save_restore(why, why_len, real, save_type);

    /* Hide the OSD before the core starts, through the OSD module so that its
     * own idea of the overlay stays true -- the same reason nesload does it
     * here rather than with a bare 0x08 frame.  Dropping the lock first,
     * because the OSD takes it itself. */
    if (tang_osd_set(false) != 0) {
        f_close(&file);
        gba_say(why, why_len, "streamed %u bytes but could not clear the OSD\r\n",
                    (unsigned)sent_total);
        return 1;
    }

    /* Releasing the load starts the cartridge. */
    tang_fpga_lock();
    const int released = tang_fpga_frame(FPGA_CMD_SET_LOAD, (const uint8_t[]){ 0 }, 1);
    tang_fpga_unlock();
    f_close(&file);

    if (released != 0) {
        gba_say(why, why_len, "streamed %u bytes but could not start the core\r\n",
                    (unsigned)sent_total);
        return 1;
    }

    gba_say(why, why_len, "%u bytes in; the core is running\r\n", (unsigned)sent_total);
    return 0;
}

/* The shell front end.  The desk app calls tang_gba_load() directly, so this is
 * only the path resolution and the message, exactly as cmd_snesload is. */
static int cmd_gbaload(tdsh_session_t *session, int argc, char **argv)
{
    if (argc < 2) {
        tdsh_printf("usage: gbaload <path>\r\n");
        return 1;
    }
    char real[TDSH_MAX_REAL_PATH];
    if (!resolve_path(session, argv[1], real, sizeof(real))) {
        tdsh_printf("gbaload: bad path %s\r\n", argv[1]);
        return 1;
    }
    char why[160] = "";
    return tang_gba_load(argv[1], real, why, sizeof why);
}

/* ------------------------------------------------------ save persistence
 *
 * A cartridge's save memory is reachable from here through the core's save
 * engine (`fpga/gba/src/memory/gba_save.v`), 128 bytes at a time, in the window
 * that starts at 0x700000.  The engine is the lowest-priority user of the SDRAM
 * controller, so a dump taken while a game is running steals cycles rather than
 * stalling the machine -- which is the point of asking for it on demand instead
 * of capturing it at load time.
 *
 * Every save type the core knows is a whole number of 128-byte chunks, which is
 * why the chunk is that size and why neither side needs a partial-chunk case:
 *
 *      SRAM 32 KB    flash512K 64 KB    flash1M 128 KB    EEPROM 8 KB
 *
 * Each exchange is one request followed by one answer, so a chunk either
 * arrives or it times out; the address is echoed in the answer and checked, so
 * an out-of-order or stale frame cannot be filed under the wrong offset.
 */
#define TANG_GBA_SAVE_CHUNK      128u
#define TANG_GBA_SAVE_BASE       0x700000u
#define TANG_GBA_SAVE_TIMEOUT_MS 250u

static uint8_t s_save_chunk[TANG_GBA_SAVE_CHUNK];

static uint32_t gba_save_bytes(uint8_t type)
{
    switch (type) {
    case TANG_GBA_BACKUP_SRAM:      return 32u * 1024u;
    case TANG_GBA_BACKUP_FLASH512K: return 64u * 1024u;
    case TANG_GBA_BACKUP_FLASH1M:   return 128u * 1024u;
    case TANG_GBA_BACKUP_EEPROM:    return 8u * 1024u;
    default:                        return 0u;
    }
}

static void gba_addr24(uint32_t addr, uint8_t out[3])
{
    out[0] = (uint8_t)(addr >> 16);
    out[1] = (uint8_t)(addr >> 8);
    out[2] = (uint8_t)addr;
}

/* One 128-byte chunk out of the cartridge.  Caller holds the lock. */
static int gba_save_read(uint32_t addr, uint8_t *out)
{
    uint8_t req[3];
    uint8_t rsp[3 + TANG_GBA_SAVE_CHUNK];

    /* Drop anything queued before this exchange.  The core sends unprompted
     * joypad reports while a dump runs; `fpga_frames_take` leaves those in the
     * queue, and once sixteen have accumulated `rx_decode` stops feeding the
     * decoder, so the reply we want can never be decoded and every remaining
     * chunk times out.  Nothing legitimate is pending between exchanges of a
     * strict request/response protocol. */
    fpga_frames_flush_queue();

    gba_addr24(addr, req);
    if (tang_fpga_frame(FPGA_CMD_SAVE_READ, req, sizeof req) != 0)
        return -1;
    if (tang_fpga_wait(FPGA_RESP_SAVE_DATA, rsp, sizeof rsp,
                       TANG_GBA_SAVE_TIMEOUT_MS) != (int)sizeof rsp)
        return -1;
    if (rsp[0] != req[0] || rsp[1] != req[1] || rsp[2] != req[2])
        return -1;
    memcpy(out, &rsp[3], TANG_GBA_SAVE_CHUNK);
    return 0;
}

/* One 128-byte chunk into the cartridge.  Caller holds the lock. */
static int gba_save_write(uint32_t addr, const uint8_t *in)
{
    uint8_t req[3 + TANG_GBA_SAVE_CHUNK];
    uint8_t ack[4];

    fpga_frames_flush_queue();      /* see gba_save_read */
    gba_addr24(addr, req);
    memcpy(&req[3], in, TANG_GBA_SAVE_CHUNK);
    if (tang_fpga_frame(FPGA_CMD_SAVE_WRITE, req, sizeof req) != 0)
        return -1;
    if (tang_fpga_wait(FPGA_RESP_SAVE_ACK, ack, sizeof ack,
                       TANG_GBA_SAVE_TIMEOUT_MS) != (int)sizeof ack)
        return -1;
    if (ack[0] != req[0] || ack[1] != req[1] || ack[2] != req[2] || ack[3] != 0)
        return -1;
    return 0;
}

/* The save type a cartridge declares, found by scanning its image for the
 * library marker -- the same scan, over the same list, that gbaload runs while
 * it streams.  Returns TANG_GBA_BACKUP_NONE when the image declares none. */
static uint8_t gba_save_type_of(const char *rom_real)
{
    FIL rom;
    if (f_open(&rom, rom_real, FA_READ) != FR_OK)
        return TANG_GBA_BACKUP_NONE;

    uint8_t type = TANG_GBA_BACKUP_NONE;
    uint8_t tail[TANG_GBA_SAVE_MARK_MAX];
    size_t  tail_len = 0;

    for (;;) {
        UINT br = 0;
        if (f_read(&rom, s_rom_chunk, FPGA_ROM_CHUNK, &br) != FR_OK || br == 0)
            break;
        type = gba_scan_saves(s_rom_chunk, br, tail, &tail_len);
        if (type != TANG_GBA_BACKUP_NONE)
            break;
        if (br < FPGA_ROM_CHUNK)
            break;
    }
    f_close(&rom);
    return type;
}

static const char *gba_save_type_name(uint8_t type)
{
    static const char *const names[] = { "none", "flash512K", "flash1M",
                                         "SRAM", "EEPROM" };
    return names[type <= TANG_GBA_BACKUP_EEPROM ? type : 0];
}

/* ---------------------------------------------- save facts for the desk app
 *
 * The TinyGBA window's Saves page says what kind of save a cartridge has and
 * how large it should be, as TinySNES's does from its header.  A GBA cartridge
 * carries no such header: the type is a marker string somewhere in the image,
 * which is what gbaload finds while it streams.  The page cannot borrow that
 * scan -- it runs on the loader's shared `s_rom_chunk` -- so this one reads with
 * its own buffer, and tests a marker's first letter before comparing, because
 * the page reads a whole image straight from the card and a cartridge with no
 * save at all is read to its end. */
uint32_t tang_gba_save_bytes(uint8_t type) { return gba_save_bytes(type); }

const char *tang_gba_save_type_name(uint8_t type) { return gba_save_type_name(type); }

int tang_gba_save_info(const char *rom_real, uint8_t *type)
{
    enum { CHUNK = 4096u };
    const size_t nmarks = sizeof s_gba_save_marks / sizeof s_gba_save_marks[0];
    *type = TANG_GBA_BACKUP_NONE;

    FIL rom;
    if (f_open(&rom, rom_real, FA_READ) != FR_OK)
        return 1;
    uint8_t *buf = (uint8_t *)malloc(CHUNK + TANG_GBA_SAVE_MARK_MAX);
    if (buf == NULL) {
        (void)f_close(&rom);
        return 1;
    }

    /* The buffer holds the previous read's last bytes ahead of this read's, so
     * a marker split across two reads is one string here. */
    size_t carry = 0;
    for (;;) {
        UINT br = 0;
        if (f_read(&rom, buf + carry, CHUNK, &br) != FR_OK || br == 0)
            break;
        const size_t n = carry + br;
        for (size_t i = 0; i < n && *type == TANG_GBA_BACKUP_NONE; i++) {
            const uint8_t c = buf[i];
            if (c != 'F' && c != 'S' && c != 'E')
                continue;
            for (size_t m = 0; m < nmarks; m++) {
                const char  *mark = s_gba_save_marks[m].mark;
                const size_t ml   = strlen(mark);
                if (i + ml <= n && memcmp(&buf[i], mark, ml) == 0) {
                    *type = s_gba_save_marks[m].type;
                    break;
                }
            }
        }
        if (*type != TANG_GBA_BACKUP_NONE || br < CHUNK)
            break;
        carry = n < TANG_GBA_SAVE_MARK_MAX ? n : TANG_GBA_SAVE_MARK_MAX;
        memmove(buf, buf + n - carry, carry);
        taskYIELD();                /* a whole image is long; let others run */
    }
    free(buf);
    (void)f_close(&rom);
    return 0;
}

/* The save log, /root/saves/last.log, in the shape td_saves_log.h parses: an
 * arm line when a save starts and a finish line when it ends, each stamped
 * with the RTC-kept clock by tang_ae350_note.  FatFS gives a file no date, so
 * this is how the Saves page knows when a save was written and whether it
 * finished.  The GBA save is not read back, so there is no CRC to report and
 * both fields are zero; the verdict is one word, as the parser reads it. */
static void gba_log_arm(const char *path, uint32_t bytes)
{
    char line[TDSH_MAX_REAL_PATH + 64];
    snprintf(line, sizeof line, "arm %s: %lu bytes, crc %08lx, save",
             path, (unsigned long)bytes, 0ul);
    tang_ae350_note(line);
}

static void gba_log_finish(const char *path, uint32_t got, uint32_t want,
                           const char *verdict)
{
    char line[TDSH_MAX_REAL_PATH + 80];
    snprintf(line, sizeof line, "finish %s: %lu of %lu bytes, crc %08lx of %08lx, %s",
             path, (unsigned long)got, (unsigned long)want, 0ul, 0ul, verdict);
    tang_ae350_note(line);
}

/* Write the running cartridge's save to `<rom>.sav`.
 *
 * The save is read from the machine, not from a file the emulator kept: what
 * comes back is the save window exactly as the game left it, so a title that
 * saves only when it feels like it needs no cooperation here. */
/* Dump the running cartridge's save window to `<real>.sav`.  `label` is what
 * the caller called the cartridge, for messages; 0 on success, otherwise
 * non-zero with the reason in `why`.
 *
 * Extracted from cmd_gbasave for the same reason tang_gba_load() was extracted
 * from cmd_gbaload: the desk app has to save a cartridge when the player asks to
 * leave a game, and the shell command has to save on demand, and two copies of a
 * chunked transfer would drift.  The transfer itself is untouched, including the
 * yield between chunks that TCL-064 records as necessary -- that record also
 * notes that every console death this project has seen happened during an SD
 * write, so this path is the one that has wedged the board before. */
int tang_gba_save(const char *label, const char *real, char *why, size_t why_len)
{

    const uint8_t type = gba_save_type_of(real);
    if (type == TANG_GBA_BACKUP_NONE) {
        gba_say(why, why_len, "%s declares no save hardware, so there is nothing to read\r\n",
                    label);
        return 1;
    }
    const uint32_t bytes = gba_save_bytes(type);

    char path[TDSH_MAX_REAL_PATH + 8];
    (void)snprintf(path, sizeof path, "%s.sav", real);

    if (tang_fpga_link_open() != 0) {
        gba_say(why, why_len, "cannot bring up UART1\r\n");
        return 1;
    }

    FIL sav;
    if (f_open(&sav, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        gba_say(why, why_len, "cannot create %s\r\n", path);
        return 1;
    }
    gba_log_arm(path, bytes);

    /* Say what is about to happen before the first exchange, so that a command
     * which produces no further output is telling us something specific
     * ("silent after the header" = stuck in the transfer) instead of leaving a
     * blank screen that cannot be diagnosed.  An earlier version printed nothing
     * until it finished, and when the console died mid-dump there was no way to
     * tell whether the transfer, the SD write or the lock had stalled. */
    gba_say(why, why_len, "dumping %u bytes of %s\r\n",
                (unsigned)bytes, gba_save_type_name(type));
    fflush(stdout);

    /* Bounded: if another task holds the transmit lock, report it rather than
     * blocking this console forever.  `tang_fpga_lock()` waits portMAX_DELAY,
     * and a shell command that never returns is indistinguishable from a wedged
     * board. */
    if (!tang_fpga_lock_within(5000)) {
        f_close(&sav);
        f_unlink(path);
        gba_log_finish(path, 0, bytes, "failed");
        gba_say(why, why_len, "the FPGA link is held by another task; try again\r\n");
        return 1;
    }
    tang_fpga_drain();

    const uint32_t chunks = (bytes + TANG_GBA_SAVE_CHUNK - 1) / TANG_GBA_SAVE_CHUNK;
    uint32_t off = 0;
    int      rc  = 0;
    int      consec = 0;
    for (off = 0; off < bytes; off += TANG_GBA_SAVE_CHUNK) {
        if (gba_save_read(TANG_GBA_SAVE_BASE + off, s_save_chunk) != 0) {
            /* Three in a row is a dead link, not a hiccup; one retry would be
             * reasonable but the core is not expected to drop a reply. */
            if (++consec >= 3) { rc = 1; break; }
            continue;
        }
        consec = 0;
        UINT bw = 0;
        if (f_write(&sav, s_save_chunk, TANG_GBA_SAVE_CHUNK, &bw) != FR_OK ||
            bw != TANG_GBA_SAVE_CHUNK) {
            rc = 2;
            break;
        }
        /* Yield every chunk.  The previous version yielded every 64th, which
         * for an 8 KB EEPROM dump is never -- the whole transfer ran with the
         * console and USB starved.  That is the shape that can wedge the shell,
         * and it costs one scheduling round trip per 128 bytes. */
        tang_fpga_unlock();
        taskYIELD();
        if (!tang_fpga_lock_within(5000)) { rc = 3; break; }

        if ((off / TANG_GBA_SAVE_CHUNK) % 64 == 0)
            gba_say(why, why_len, "%u/%u chunks\r\n",
                        (unsigned)(off / TANG_GBA_SAVE_CHUNK), (unsigned)chunks);
    }
    tang_fpga_unlock();
    f_close(&sav);

    if (rc == 1) {
        f_unlink(path);             /* half a save is worse than none */
        gba_log_finish(path, off, bytes, "failed");
        gba_say(why, why_len, "the core stopped answering at offset %u; %s not written\r\n",
                    (unsigned)off, path);
        return 1;
    }
    if (rc == 2) {
        f_unlink(path);
        gba_log_finish(path, off, bytes, "failed");
        gba_say(why, why_len, "SD write failed at offset %u; %s not written\r\n",
                    (unsigned)off, path);
        return 1;
    }
    if (rc == 3) {
        f_unlink(path);
        gba_log_finish(path, off, bytes, "failed");
        gba_say(why, why_len, "lost the FPGA link at offset %u; %s not written\r\n",
                    (unsigned)off, path);
        return 1;
    }

    gba_log_finish(path, bytes, bytes, "written");
    gba_say(why, why_len, "%u bytes of %s -> %s\r\n",
                (unsigned)bytes, gba_save_type_name(type), path);
    return 0;
}

/* The shell front end, kept thin so the app and the console take one path. */
static int cmd_gbasave(tdsh_session_t *session, int argc, char **argv)
{
    if (argc < 2) {
        tdsh_printf("usage: gbasave <rom-path>   (writes <rom-path>.sav)\r\n");
        return 1;
    }
    char real[TDSH_MAX_REAL_PATH];
    if (!resolve_path(session, argv[1], real, sizeof(real))) {
        tdsh_printf("gbasave: bad path %s\r\n", argv[1]);
        return 1;
    }
    char why[160] = "";
    return tang_gba_save(argv[1], real, why, sizeof why);
}

/* Print the pad word the core is reporting, and keep printing it while it
 * changes until the player presses Enter.
 *
 * This exists because a pad change cannot be checked any other way: the word is
 * the only place the core's report becomes visible, and until now nothing on the
 * host could read it back, so every claim about an input gesture rested on
 * somebody pressing buttons and describing what happened.  A single press of a
 * key that does not show up here is a fact; a gesture that "sometimes works" is
 * not.  Cost is one shell command and no RTL.
 *
 * Bits 11:0 are the keypad in (R L X A RT LT DN UP START SELECT Y B) order, bit
 * 10 is the desk toggle the core synthesises from a HOME tap, and bit 15 is the
 * exit chord, set while HOME and SELECT are both held. */
static int cmd_pad(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    int seconds = 10;
    if (argc > 1) {
        seconds = atoi(argv[1]);
        if (seconds <= 0) {
            seconds = 10;
        }
    }

    uint16_t last1 = 0xFFFF, last2 = 0xFFFF;
    tdsh_printf("pad: watching for %d s; press buttons.  ctrl-c stops early.\r\n",
                seconds);
    tdsh_printf("pad: bits 11:0 = (R L X A RT LT DN UP START SELECT Y B),"
                " bit 10 = desk tap, bit 15 = exit chord\r\n");
    for (int i = 0; i < seconds * 20; i++) {
        uint16_t joy1 = 0, joy2 = 0;
        tang_fpga_joypad(&joy1, &joy2);
        if (joy1 != last1) {
            /* Name the bits that are set, so a held chord is readable without
             * decoding hex at a terminal. */
            char bits[64] = "";
            static const char *const names[12] = {
                "B", "Y", "SELECT", "START", "UP", "DOWN",
                "LEFT", "RIGHT", "A", "X", "DESK_TAP", "R",
            };
            for (int b = 0; b < 12; b++) {
                if ((joy1 & (1u << b)) != 0) {
                    if (bits[0] != '\0') {
                        strncat(bits, "+", sizeof bits - strlen(bits) - 1);
                    }
                    strncat(bits, names[b], sizeof bits - strlen(bits) - 1);
                }
            }
            if ((joy1 & TANG_GBA_PAD_EXIT) != 0) {
                if (bits[0] != '\0') {
                    strncat(bits, "+", sizeof bits - strlen(bits) - 1);
                }
                strncat(bits, "EXIT", sizeof bits - strlen(bits) - 1);
            }
            tdsh_printf("pad: %04x  %s\r\n", (unsigned)joy1,
                        bits[0] != '\0' ? bits : "(none)");
            last1 = joy1;
            (void)last2;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    tdsh_printf("pad: done\r\n");
    return 0;
}

/* Put a stored save back, called by gbaload before the cartridge is started.
 * Prints its own result; a cartridge with no .sav simply says nothing. */
static void gba_save_restore(char *why, size_t why_len, const char *rom_real,
                             uint8_t type)
{

    char path[TDSH_MAX_REAL_PATH + 8];
    (void)snprintf(path, sizeof path, "%s.sav", rom_real);

    FIL sav;
    if (f_open(&sav, path, FA_READ) != FR_OK)
        return;                     /* no save for this title yet */

    const uint32_t want = gba_save_bytes(type);
    const uint32_t have = (uint32_t)f_size(&sav);
    if (have != want) {
        gba_say(why, why_len, "%s is %u bytes but this cartridge's %s save needs %u; not restored\r\n",
                    path, (unsigned)have, gba_save_type_name(type), (unsigned)want);
        f_close(&sav);
        return;
    }

    gba_say(why, why_len, "restoring %u bytes of %s\r\n",
                (unsigned)want, gba_save_type_name(type));

    if (!tang_fpga_lock_within(5000)) {
        f_close(&sav);
        gba_say(why, why_len, "the FPGA link is held by another task; save not restored\r\n");
        return;
    }
    tang_fpga_drain();

    uint32_t off = 0;
    int      rc  = 0;
    int      consec = 0;
    for (off = 0; off < want; off += TANG_GBA_SAVE_CHUNK) {
        UINT br = 0;
        if (f_read(&sav, s_save_chunk, TANG_GBA_SAVE_CHUNK, &br) != FR_OK ||
            br != TANG_GBA_SAVE_CHUNK) {
            rc = 1;
            break;
        }
        if (gba_save_write(TANG_GBA_SAVE_BASE + off, s_save_chunk) != 0) {
            if (++consec >= 3) { rc = 2; break; }
            continue;
        }
        consec = 0;
        /* Yield every chunk, for the reason given in cmd_gbasave. */
        tang_fpga_unlock();
        taskYIELD();
        if (!tang_fpga_lock_within(5000)) { rc = 3; break; }
    }
    tang_fpga_unlock();
    f_close(&sav);

    if (rc == 1)
        gba_say(why, why_len, "SD read failed at %u of the save\r\n", (unsigned)off);
    else if (rc == 2)
        gba_say(why, why_len, "the core stopped taking save data at %u\r\n", (unsigned)off);
    else if (rc == 3)
        gba_say(why, why_len, "lost the FPGA link at %u of the save\r\n", (unsigned)off);
    else
        gba_say(why, why_len, "save restored, %u bytes of %s\r\n",
                    (unsigned)want, gba_save_type_name(type));
}

/* The TinySNES core's loader, shared with the desk app (tang_snes_load.c). */
static int cmd_snesload(tdsh_session_t *session, int argc, char **argv)
{
    if (argc < 2) {
        tdsh_printf("usage: snesload <path>\r\n");
        return 1;
    }
    char real[TDSH_MAX_REAL_PATH];
    if (!resolve_path(session, argv[1], real, sizeof(real))) {
        tdsh_printf("snesload: bad path %s\r\n", argv[1]);
        return 1;
    }
    char why[96];
    const int rc = tang_snes_load(real, why, sizeof(why));
    tdsh_printf("snesload: %s\r\n", why);
    return rc;
}

/* ------------------------------------------------------------- fpgapipe */

/* Pass the console straight through to UART1 and back until the host closes
 * the port (DTR drops).  Nothing is framed, so a core with its own protocol
 * on the link -- TinyNES's AE350 loader -- is driven by the host's own tool.
 * The TX lock is held throughout, so the desk layer and the other commands
 * wait rather than interleave their frames with the host's stream, and the
 * probe is passed through as data because the stream is binary.
 *
 * The host's bytes are also watched, unchanged, for an overlay frame: a host
 * loader hides the overlay itself at the end of a load, and the OSD module's
 * record has to follow or the next L / HOME tap does nothing
 * (tang_pipe_sniff.h).  The record is changed under the lock this holds. */
static int cmd_fpgapipe(tdsh_session_t *session, int argc, char **argv)
{
    (void)session; (void)argc; (void)argv;
    tang_pipe_sniff_t sniff;
    tang_pipe_sniff_reset(&sniff);

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
        if (sent) {
            const int overlay = tang_pipe_sniff(&sniff, up, n);
            if (overlay >= 0) {
                tang_osd_note_locked(overlay != 0);
            }
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
/* TinyNES's one save state per cartridge (TinyNES core-log entry 49). */

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
 * says "TinyNES WANT ...", TinyGBC's "TinyGBC WANT ...", and TinyCreature's
 * fixed scene names itself the same way. One set of parsers serves them, so a
 * line is matched against the name it carries and then past it, and the caller
 * reads the same fields either way. The NVRAM and STATE channels below serve
 * products that keep files; each product's settings request is its own,
 * because each blob is that player's ABI. */
static const char *ae350_past_name(const char *line)
{
    static const char *const names[] = {
        "TinyNES ", "TinyGBC ", "Tiny89 ", "TinyCreature "
    };

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

/* The system a program names in its own lines.  The name is the program's
 * declaration of what it is, and it decides which folder the card keeps its
 * files in; the calculator arrived last and got its own. */
static const char *ae350_system(const char *line)
{
    if (starts_with(line, "TinyNES ")) return "nes";
    if (starts_with(line, "TinyGBC ")) return "gbc";
    if (starts_with(line, "Tiny89 ")) return "ti89";
    return NULL;
}

/* "<product> <channel> <rest>", the shape every file line takes:
 * "Tiny89 NVRAM SAVE 262144 8d90ffa5", "TinyNES STATE DATA ...",
 * "Tiny89 FLASH WANT ...".  NVRAM is the program's RAM, FLASH is the
 * calculator's 4 MiB flash image, STATE is a player's save state, which
 * TinyNES and TinyGBC both keep one of per cartridge, and RTC is the clock of
 * a Game Boy cartridge that has one, kept beside its battery save. */
static bool ae350_channel_line(const char *line, const char **system,
                               const char **channel, const char **rest)
{
    static const char *const channels[] = { "NVRAM ", "FLASH ", "STATE ",
                                            "RTC ", "SEND " };
    const char *sys = ae350_system(line);
    const char *tail;

    if (sys == NULL) {
        return false;
    }
    tail = strchr(line, ' ');
    if (tail == NULL) {
        return false;
    }
    ++tail;
    for (size_t i = 0; i < sizeof channels / sizeof channels[0]; i++) {
        const size_t n = strlen(channels[i]);
        if (strncmp(tail, channels[i], n) == 0) {
            *system = sys;
            *channel = channels[i];
            *rest = tail + n;
            return true;
        }
    }
    return false;
}

/* The run's data file without its extension: every file the card keeps for
 * this cartridge is named after it, as saves always were. */
static char s_ae350_base[64];

/* The file a session was asked to hand the calculator over its link.  Unlike
 * the saves, its name is the user's choice rather than a derivation, so it is
 * remembered here and served when the program asks for it. */
static char s_ae350_send_path[TDSH_MAX_REAL_PATH];

void tang_ae350_set_send_file(const char *path_real)
{
    if (path_real == NULL) {
        s_ae350_send_path[0] = '\0';
        return;
    }
    (void)snprintf(s_ae350_send_path, sizeof s_ae350_send_path, "%s", path_real);
}

/* What a channel's file is called in the console's words. */
static const char *ae350_channel_kind(const char *channel)
{
    if (strcmp(channel, "STATE ") == 0) {
        return "state";
    }
    return strcmp(channel, "RTC ") == 0 ? "clock" : "save";
}

static bool ae350_path_for(const char *system, const char *channel,
                           char *out, size_t size)
{
    const bool state = strcmp(channel, "STATE ") == 0;
    if (strcmp(channel, "SEND ") == 0) {
        return false;    /* its name comes from the caller, not from the base */
    }
    const bool flash = strcmp(channel, "FLASH ") == 0;
    const bool rtc = strcmp(channel, "RTC ") == 0;
    const char *dir = state ? "/sd/root/states/" : "/sd/root/saves/";
    const char *ext = state ? ".state"
                    : flash ? ".flash"
                    : rtc ? ".rtc"
                    : (strcmp(system, "ti89") == 0 ? ".ram" : ".sav");
    const int n = snprintf(out, size, "%s%s/%s%s", dir, system, s_ae350_base, ext);
    return n > 0 && (size_t)n < size;
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
 * Every file is named for the cartridge and the system the program named:
 * /root/saves/<system>/<cartridge><ext> and /root/states/<system>/<cartridge>.state,
 * with .sav for a game's battery save, .ram for the calculator's memory and
 * .flash for its flash image.  All of them come back through the one receiver
 * below, which writes whichever s_ae350_dest names. */
static char s_ae350_dest[TDSH_MAX_REAL_PATH];
static const char *s_ae350_dest_kind = "save";

/* ------------------------------------------------------------------ clock */

/* The desktop's time zone, seconds east of UTC.  It is the same file the
 * desktop reads (/tinydesk_tz, td_desktop_bl616.c) and the shell's tangi2c
 * reads; a missing or unreadable file is UTC, not an error.  tang_clock.c
 * holds the one reader, which the core-directed time frame also needs, so this
 * is a name for it here rather than a second copy of the parse. */
static long ae350_zone(void)
{
    return tang_clock_zone();
}

/* Days since 1970-01-01 to a civil date (Howard Hinnant's algorithm), so the
 * stamp needs no time library and no per-call locale. */
static void ae350_civil(int64_t days, int *year, int *month, int *day)
{
    int64_t z = days + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    *day = (int)(doy - (153 * mp + 2) / 5 + 1);
    *month = (int)(mp + (mp < 10 ? 3 : -9));
    *year = (int)((int64_t)yoe + era * 400 + (*month <= 2 ? 1 : 0));
}

/* A log line's time stamp: the RTC-kept clock in the desktop's zone, ISO-8601
 * with the offset, or "no-clock" when the RTC has never been set -- the clock
 * says when it does not know rather than writing a date it invented.  During a
 * calculator session the AE350 program holds the link, so tang_clock_now finds
 * no I2C bus (it touches no link traffic) and returns the running clock the
 * last desktop-core sync left behind, which is the right time to stamp. */
static void ae350_stamp(char *out, size_t cap)
{
    int64_t utc;
    int64_t local;
    int64_t days;
    int64_t sod;
    int year = 0;
    int month = 0;
    int day = 0;
    long zone;
    long az;

    if (!tang_clock_now(&utc)) {
        (void)snprintf(out, cap, "no-clock");
        return;
    }
    zone = ae350_zone();
    local = utc + zone;
    days = local / 86400;
    sod = local % 86400;
    if (sod < 0) {
        sod += 86400;
        days -= 1;
    }
    ae350_civil(days, &year, &month, &day);
    az = zone < 0 ? -zone : zone;
    (void)snprintf(out, cap, "%04d-%02d-%02dT%02d:%02d:%02d%c%02ld:%02ld",
                   year, month, day, (int)(sod / 3600), (int)(sod / 60 % 60),
                   (int)(sod % 60), zone < 0 ? '-' : '+', az / 3600,
                   az % 3600 / 60);
}

/* One line per save attempt, on the card.  The console is not always where a
 * session's output can be read, and a save that never finishes leaves no trace
 * but a .part file: this says what was armed, what arrived and why it stopped.
 * Each line carries the local date and time from the RTC-kept clock, so the
 * Tiny89 window's Saves page can show when the last attempt ran and whether it
 * verified.  A run's first line is "--- run", a separator; the file grows. */
static void ae350_log(const char *fmt, ...)
{
    char text[176];
    char stamp[48];
    char line[240];
    va_list ap;
    int n;
    size_t len;
    FIL file;
    UINT wrote = 0;

    va_start(ap, fmt);
    n = vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    if (n <= 0) {
        return;
    }
    ae350_stamp(stamp, sizeof stamp);
    n = snprintf(line, sizeof line, "%s %s", stamp, text);
    if (n <= 0) {
        return;
    }
    len = strnlen(line, sizeof line - 1);
    /* One write per line: a frame's worth of log must never be left without
     * its newline, or the Saves page's line scanner joins it to the next. */
    if (len == 0 || line[len - 1] != '\n') {
        line[len < sizeof line - 1 ? len : sizeof line - 2] = '\n';
        line[len < sizeof line - 1 ? len + 1 : sizeof line - 1] = '\0';
        len = strlen(line);
    }
    if (f_open(&file, "/sd/root/saves/last.log",
               FA_OPEN_APPEND | FA_WRITE) != FR_OK) {
        return;
    }
    (void)f_write(&file, line, (UINT)len, &wrote);
    (void)f_close(&file);
}

/* The log, for a caller outside this file: the same stamped line an AE350
 * run writes, so a window can leave a note where a console cannot be read. */
void tang_ae350_note(const char *text)
{
    ae350_log("%s\n", text);
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
static bool s_ae350_save_first_logged;   /* the first byte has been noted */
static uint32_t s_ae350_save_left;       /* payload bytes still due */
static unsigned s_ae350_save_chunk_len;
static uint8_t s_ae350_save_chunk[AE350_SAVE_CHUNK];

/* What the card cost this save, and what it cost at its worst.  The save is
 * paced from the other end, so the number that matters is the longest window
 * the card held the CPU with interrupts masked: every byte that arrives in
 * that window is lost, and no counter sees it.  One line in the log per save
 * (a line per write would cost card time to write down what the card cost). */
#define AE350_WRITE_SLOW_MS 5u
static uint32_t s_ae350_write_ms;        /* card time for the whole save */
static uint32_t s_ae350_write_max_ms;    /* the slowest single write */
static uint32_t s_ae350_write_slow;      /* writes over AE350_WRITE_SLOW_MS */
static uint32_t s_ae350_write_count;

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
    const uint32_t began = (uint32_t)bflb_mtimer_get_time_ms();
    const FRESULT result = f_write(&s_ae350_save_file, bytes, n, &wrote);
    const uint32_t took = (uint32_t)bflb_mtimer_get_time_ms() - began;

    s_ae350_write_ms += took;
    s_ae350_write_count++;
    if (took > s_ae350_write_max_ms) {
        s_ae350_write_max_ms = took;
    }
    if (took >= AE350_WRITE_SLOW_MS) {
        s_ae350_write_slow++;
    }
    if (result != FR_OK || wrote != n) {
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
            ae350_log("refused line at %lu bytes: \"%.24s\" open %d\n",
                      (unsigned long)s_ae350_save_got, line,
                      s_ae350_save_open ? 1 : 0);
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
        if ((s_ae350_save_got & 0x7fffu) < n) {
            ae350_log("progress: %lu of %lu bytes\n",
                      (unsigned long)s_ae350_save_got,
                      (unsigned long)s_ae350_save_expect);
        }
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
    if (!s_ae350_save_first_logged) {
        s_ae350_save_first_logged = true;
        ae350_log("first byte %02x of %lu\n", (unsigned)byte,
                  (unsigned long)s_ae350_save_expect);
    }
    s_ae350_save_left--;
    if (!s_ae350_save_open || s_ae350_save_failed) {
        s_ae350_save_failed = true;
        return;
    }
    s_ae350_save_chunk[s_ae350_save_chunk_len++] = byte;
    if (s_ae350_save_chunk_len == sizeof s_ae350_save_chunk) {
        ae350_save_flush();
        if ((s_ae350_save_got & 0x7fffu) < s_ae350_save_chunk_len) {
            ae350_log("progress %s: %lu of %lu bytes\n", s_ae350_dest,
                      (unsigned long)s_ae350_save_got,
                      (unsigned long)s_ae350_save_expect);
        }
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
    s_ae350_save_first_logged = false;
    s_ae350_save_active = true;
    if (sscanf(counts, "%lu %lx", &expect, &crc) != 2 || expect == 0 ||
        expect > AE350_SAVE_MAX) {
        ae350_log("arm %s: refused, counts \"%s\" (max %lu)\n", dest, counts,
                  (unsigned long)AE350_SAVE_MAX);
        s_ae350_save_expect = 0;
        s_ae350_save_crc = 0;
        s_ae350_save_failed = true;
        return;
    }
    s_ae350_save_expect = (uint32_t)expect;
    s_ae350_save_crc = (uint32_t)crc;
    s_ae350_save_left = (uint32_t)expect;
    s_ae350_write_ms = 0;
    s_ae350_write_max_ms = 0;
    s_ae350_write_slow = 0;
    s_ae350_write_count = 0;
    if (s_ae350_dest[0] == '\0') {
        ae350_log("arm: no destination\n");
        s_ae350_save_failed = true;
        return;
    }
    ae350_log("arm %s: %lu bytes, crc %08lx, %s\n", s_ae350_dest,
              (unsigned long)expect, (unsigned long)crc,
              binary ? "bytes" : "hex");
    if (!ae350_save_open_file()) {
        ae350_log("arm %s: cannot open the temporary\n", s_ae350_dest);
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
    ae350_log("finish %s: %lu of %lu bytes, crc %08lx of %08lx, %s\n",
              s_ae350_dest, (unsigned long)s_ae350_save_got,
              (unsigned long)s_ae350_save_expect,
              (unsigned long)s_ae350_save_running,
              (unsigned long)s_ae350_save_crc,
              whole ? "whole" : "short");
    /* The card's side of the same attempt: how long it held the CPU, which is
     * the window in which arriving bytes were lost.  A max far above the
     * emulator's pause per chunk is the shortfall the pacing has to cover. */
    ae350_log("card %s: %lu writes, %lu ms in all, max %lu ms, %lu over %lu ms\n",
              s_ae350_dest, (unsigned long)s_ae350_write_count,
              (unsigned long)s_ae350_write_ms,
              (unsigned long)s_ae350_write_max_ms,
              (unsigned long)s_ae350_write_slow,
              (unsigned long)AE350_WRITE_SLOW_MS);
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
/* The same thing with the bytes themselves as the payload: TinyNES's framing
 * with the data put in whole.  The length the header carries says where the
 * payload ends, so nothing in it needs escaping and nothing is special -- CR,
 * LF and 0xAA are bytes here, and the text reader that would have mangled
 * them is not the one reading this. */
/* A save state, TinyNES's one per cartridge, as bytes: "TinyNES STATE DATA"
 * and the same length and CRC-32, written to the cartridge's .state through
 * the same verified replace as a battery save. */
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
    {
        const char *system = NULL, *channel = NULL, *rest = NULL;
        if (ae350_channel_line(s_ae350_save_line, &system, &channel, &rest) &&
            starts_with(rest, "DONE")) {
            ae350_log("done line: %lu of %lu bytes\n",
                      (unsigned long)s_ae350_save_got,
                      (unsigned long)s_ae350_save_expect);
            ae350_save_finish();
            s_ae350_save_active = false;
        } else {
            ae350_save_hex(s_ae350_save_line);
        }
    }
    s_ae350_save_line_len = 0;
}

/* After "<product> LINK": the core answers in frames now, so the link goes back
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
            {
                const char *system = NULL, *channel = NULL, *rest = NULL;
                if (ae350_channel_line(run->line, &system, &channel, &rest)) {
                    char dest[TDSH_MAX_REAL_PATH];
                    const char *kind = ae350_channel_kind(channel);
                    const bool named =
                        ae350_path_for(system, channel, dest, sizeof dest);
                    if (starts_with(rest, "DATA ")) {
                        if (named) {
                            ae350_save_arm(rest + 5, true, dest, kind);
                        } else {
                            s_ae350_save_active = false;
                        }
                    } else if (starts_with(rest, "SAVE ")) {
                        if (named) {
                            ae350_save_arm(rest + 5, false, dest, kind);
                        } else {
                            s_ae350_save_active = false;
                        }
                    }
                    continue;
                }
            }
            done = ae350_line_ends(run, &code);
        }
        if (done) {
            break;
        }
    }

    if (s_ae350_save_active) {
        ae350_log("abandoned %s: %lu of %lu bytes\n", s_ae350_dest,
                  (unsigned long)s_ae350_save_got,
                  (unsigned long)s_ae350_save_expect);
        if (s_ae350_save_open) {
            (void)f_close(&s_ae350_save_file);
            s_ae350_save_open = false;
        }
        s_ae350_save_active = false;
    }
    (void)tang_osd_set(was_shown);
    tang_fpga_lock();
    taskENTER_CRITICAL();
    s_pipe = true;
    taskEXIT_CRITICAL();
    return code;
}

/* The load protocol's block buffer, 8 KB, held only while a program runs
 * (tang_ae350_run) instead of for ever. */
static uint8_t *s_ae350_block;
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
        ae350_log("want %s: none at %s\n", kind, path);
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
            ae350_log("want %s: sent %s, %lu bytes\n", kind, path,
                      (unsigned long)run->save_length);
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
/* What the cartridge turned out to be, from the player's CORE line; -1 until a
 * run reaches the core, and reset at the start of each run. */
static int s_cart_cgb = -1;

int tang_ae350_cartridge_cgb(void)
{
    return s_cart_cgb;
}

static int ae350_run(const char *player, const char *data);

int tang_ae350_run(const char *player, const char *data)
{
    s_ae350_block = malloc(AE350_LOAD_BLOCK);
    if (s_ae350_block == NULL) {
        tdsh_printf("ae350run: not enough memory to load a program\r\n");
        return 1;
    }
    const int code = ae350_run(player, data);
    free(s_ae350_block);
    s_ae350_block = NULL;
    return code;
}

static int ae350_run(const char *player, const char *data)
{
    s_cart_cgb = -1;    if (player == NULL || player[0] == '\0') {
        tdsh_printf("usage: ae350run <program.elf> [<file>]\r\n");
        return 1;
    }
    if (f_open(&s_ae350_elf_file, player, FA_READ) != FR_OK) {
        tdsh_printf("ae350run: cannot open %s\r\n", player);
        return 1;
    }
    ae350_run_t run = { .elf_file = &s_ae350_elf_file };
    /* The settings this launch will use, read once.  /tinynes.ini and the AE350's
     * mapping are the same choice seen from two sides. */
    static tang_nes_settings_t nes;
    (void)tang_nes_load(&nes, TANG_NES_INI_REAL);
    /* No cartridge last time does not mean one now: the name must not
     * survive from a previous run. */
    s_ae350_base[0] = '\0';
    ae350_log("--- run\n");
    if (data != NULL) {
        if (f_open(&s_ae350_data_file, data, FA_READ) != FR_OK) {
            f_close(&s_ae350_elf_file);
            tdsh_printf("ae350run: cannot open %s\r\n", data);
            return 1;
        }
        run.data_file = &s_ae350_data_file;
        run.data_length = (uint32_t)f_size(&s_ae350_data_file);
        {
            const char *slash = strrchr(data, '/');
            snprintf(s_ae350_base, sizeof s_ae350_base, "%s",
                     slash != NULL ? slash + 1 : data);
            char *dot = strrchr(s_ae350_base, '.');
            if (dot != NULL && dot != s_ae350_base) {
                *dot = '\0';
            }
        }
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
        {
            const char *system = NULL, *channel = NULL, *rest = NULL;
            if (ae350_channel_line(run.line, &system, &channel, &rest) &&
                starts_with(rest, "WANT ")) {
                char dest[TDSH_MAX_REAL_PATH];
                if (strcmp(channel, "SEND ") == 0) {
                    /* The file to hand the calculator: whatever the caller
                     * named, and nothing at all when it named none. */
                    if (s_ae350_send_path[0] != '\0') {
                        ae350_send_named(&run, &link, rest + 5,
                                         s_ae350_send_path, "file");
                    } else {
                        ae350_send_named(&run, &link, rest + 5, "", "file");
                    }
                } else if (ae350_path_for(system, channel, dest, sizeof dest)) {
                    ae350_send_named(&run, &link, rest + 5, dest,
                                     ae350_channel_kind(channel));
                }
                continue;
            }
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
        /* The player names the cartridge it loaded in its CORE line, which is
         * how the window learns that a palette setting cannot touch it. */
        if (starts_with(run.line, "TinyGBC GB CORE=")) {
            const char *cgb = strstr(run.line, " CGB=");

            if (cgb != NULL) {
                s_cart_cgb = (cgb[5] == '1') ? 1 : 0;
            }
        }
        if (starts_with(run.line, "TinyGBC SETTINGS WANT ")) {
            /* TinyGBC's blob (TinyGBC firmware/apps/gb_bench/main.cpp,
             * HostSettings): the master volume and the mute, from
             * /tinygbc.ini, then the picture's palette and colour correction,
             * which take two of the bytes that were reserved, then the clock as
             * UTC seconds since 1970, little-endian, 0 while it has never been
             * set.  The player runs its time() from that, so a cartridge's own
             * clock counts real time.  Then the button mapping, one byte per
             * Game Boy button in the player's pad_joypad[] order, each naming a
             * controller button by its index in the player's pad_source[].
             * Anything after the volume and mute is optional to the player: it
             * keeps its defaults when fewer bytes arrive, which is why nothing
             * has ever been inserted before them. */
            static tang_gbc_settings_t gbc;
            static uint8_t blob[16 + TANG_GBC_BUTTONS];
            int64_t utc = 0;
            unsigned long address = 0, room = 0;
            if (sscanf(run.line + 22, "%lx %lx", &address, &room) != 2 || room < 1) {
                tdsh_printf("ae350run: the settings request is malformed\r\n");
                continue;
            }
            (void)tang_gbc_load(&gbc, TANG_GBC_INI_REAL);
            memset(blob, 0, sizeof blob);
            blob[0] = gbc.audio.volume;
            blob[1] = gbc.audio.mute;
            blob[6] = gbc.palette;
            blob[7] = gbc.colour;
            if (!tang_clock_now(&utc) || utc < 0) {
                utc = 0;
            }
            for (int i = 0; i < 8; i++) {
                blob[8 + i] = (uint8_t)((uint64_t)utc >> (8 * i));
            }
            memcpy(blob + 16, gbc.pad_map, TANG_GBC_BUTTONS);
            const size_t sending = room < sizeof blob ? (size_t)room : sizeof blob;
            rc = ae350_send_blob(&run, &link, address, blob, sending, &reason);
            tdsh_printf("ae350run: settings %s, volume=%u%s, clock %s, %s=%s, "
                        "palette %s, colour %s\r\n",
                        gbc.found ? TANG_GBC_INI_SHOWN : "(defaults)",
                        (unsigned)gbc.audio.volume, gbc.audio.mute ? " muted" : "",
                        utc != 0 ? "sent" : "not set",
                        tang_gbc_button_name(TANG_GBC_A),
                        tang_gbc_pad_name(gbc.pad_map[TANG_GBC_A]),
                        tang_gbc_palette_name(gbc.palette),
                        tang_gbc_colour_name(gbc.colour));
            if (rc != AE350_OK) {
                tdsh_printf("ae350run: sending the settings: %s\r\n", ae350_error(rc));
            }
            continue;
        }
        if (starts_with(run.line, "Tiny89 SETTINGS WANT ")) {
            /* Tiny89's blob (Tiny89 firmware/apps/tiemu_smoke/main.c): UTC
             * seconds since 1970 as a little-endian 32-bit value, then the
             * desktop's zone as a signed little-endian 16-bit count of minutes
             * east of UTC, zero for the seconds while the RTC has never been
             * set.  The calculator's own clock chip is not fitted in that
             * core's fabric, so this is the only date it gets; its emulated
             * clock counts the seconds from here on its own timer, which is
             * why one blob per launch is enough.  Same shape as TinyGBC's
             * clock above and the same rule: a board that does not know the
             * time sends zero rather than inventing one. */
            static uint8_t blob[6];
            int64_t utc = 0;
            unsigned long address = 0, room = 0;
            bool known;
            if (sscanf(run.line + 21, "%lx %lx", &address, &room) != 2 || room < 1) {
                tdsh_printf("ae350run: the settings request is malformed\r\n");
                continue;
            }
            known = tang_clock_now(&utc) && utc > 0 && utc <= 0xffffffffll;
            const int32_t zone_min = (int32_t)(tang_clock_zone() / 60);
            memset(blob, 0, sizeof blob);
            if (known) {
                const uint32_t secs = (uint32_t)utc;
                blob[0] = (uint8_t)secs;
                blob[1] = (uint8_t)(secs >> 8);
                blob[2] = (uint8_t)(secs >> 16);
                blob[3] = (uint8_t)(secs >> 24);
            }
            blob[4] = (uint8_t)zone_min;
            blob[5] = (uint8_t)((uint16_t)zone_min >> 8);
            const size_t sending = room < sizeof blob ? (size_t)room : sizeof blob;
            rc = ae350_send_blob(&run, &link, address, blob, sending, &reason);
            tdsh_printf("ae350run: settings, clock %s, zone %ld minutes\r\n",
                        known ? "sent" : "not set", (long)zone_min);
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
    char send[TDSH_MAX_REAL_PATH];
    tang_ae350_set_send_file(NULL);
    if (argc > 3) {
        if (!resolve_path(session, argv[3], send, sizeof(send))) {
            tdsh_printf("ae350run: cannot open %s\r\n", argv[3]);
            return 1;
        }
        tang_ae350_set_send_file(send);
    }
    if (argc > 2) {
        if (!resolve_path(session, argv[2], data, sizeof(data))) {
            tdsh_printf("ae350run: cannot open %s\r\n", argv[2]);
            return 1;
        }
        return tang_ae350_run(player, data);
    }
    return tang_ae350_run(player, NULL);
}

/* `fpgabaud` shows or changes the link's rate.  With the desktop core from
 * ABI 1.10 a change is negotiated (register opcode 0x03), so both ends move
 * together and a rate that does not hold falls back to 2 Mbaud by itself.  Any
 * other core cannot be asked, so `fpgabaud -raw <rate>` only retunes UART1 to
 * a rate the core was built for -- the old behaviour, kept for that.
 *
 * Rates are the ones both ends make exactly: this UART divides 40 MHz by a
 * whole number (2, 5 and 10 Mbaud; 12 and 16 would really be 13.3). */
static int cmd_fpgabaud(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;

    if (tang_fpga_link_open() != 0) {
        tdsh_printf("fpgabaud: cannot bring up UART1\r\n");
        return 1;
    }
    const bool raw = argc >= 3 && strcmp(argv[1], "-raw") == 0;
    if (argc < 2 || (argc >= 3 && !raw)) {
        uint32_t core = 0, counts = 0;
        tdsh_printf("fpgabaud: UART1 at %u baud", (unsigned)s_baud);
        if (tang_fpga_read32(FPGA_REG_LINK_RATE, &core) &&
            tang_fpga_read32(0x20u, &counts)) {
            tdsh_printf("; the desktop core at %u, %u fallbacks, %u framing errors",
                        (unsigned)core, (unsigned)(counts >> 16), (unsigned)(counts & 0xFFFFu));
        }
        tdsh_printf("\r\nusage: fpgabaud <2000000|5000000|10000000>"
                    " | fpgabaud -raw <rate>\r\n");
        return argc < 2 ? 0 : 1;
    }

    const unsigned long baud = strtoul(argv[raw ? 2 : 1], NULL, 0);
    if (baud != 2000000ul && baud != 5000000ul && baud != 10000000ul) {
        tdsh_printf("fpgabaud: %lu is not a rate both ends make exactly"
                    " (2000000 5000000 10000000)\r\n", baud);
        return 1;
    }
    if (raw) {
        tang_fpga_lock();
        const int rc = tang_fpga_set_baud((uint32_t)baud);
        tang_fpga_unlock();
        if (rc != 0) {
            tdsh_printf("fpgabaud: could not change the rate\r\n");
            return 1;
        }
        tdsh_printf("fpgabaud: UART1 now at %lu baud; the core must be at the same"
                    " rate\r\n", baud);
        return 0;
    }
    if (tang_fpga_link_rate((uint32_t)baud) != 0) {
        tdsh_printf("fpgabaud: the core did not move to %lu; both ends at %u\r\n",
                    baud, (unsigned)s_baud);
        return 1;
    }
    tdsh_printf("fpgabaud: both ends at %lu baud\r\n", baud);
    return 0;
}

/* `planefill` draws a test picture into the desktop core's pixel plane from
 * this end and times the link alone: no card, no decoder, no host.  The
 * picture is tools/plane_sweep.py's, pixel for pixel, so `plane_sweep.py
 * --no-draw --soak` can then check every frame the core shows against it by
 * the core's own checksum.  A gradient, a white grid every 64 pixels and a
 * one-pixel checkerboard in the middle. */
static uint16_t planefill_pixel(int x, int y, int w, int h)
{
    if (x % 64 == 0 || y % 64 == 0) {
        return 0xFFFFu;
    }
    if (x >= w / 2 - 64 && x < w / 2 + 64 && y >= h / 2 - 64 && y < h / 2 + 64) {
        return ((x ^ y) & 1) ? 0xFFFFu : 0x0000u;
    }
    const unsigned r = (unsigned)(x * 31 / (w - 1));
    const unsigned g = (unsigned)(y * 63 / (h - 1));
    const unsigned b = (unsigned)((x ^ y) & 31);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static int cmd_planefill(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    (void)argc;
    (void)argv;
    static uint16_t row[TANG_PLANE_MAX_WIDTH];
    int w = 0, h = 0;
    if (!tang_plane_size(&w, &h)) {
        tdsh_printf("planefill: the loaded core has no pixel plane\r\n");
        return 1;
    }
    uint64_t busy_us = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            row[x] = planefill_pixel(x, y, w, h);
        }
        const uint64_t t0 = bflb_mtimer_get_time_us();
        if (tang_plane_write(0, y, row, w) != 0) {
            tdsh_printf("planefill: row %d did not go\r\n", y);
            return 1;
        }
        busy_us += bflb_mtimer_get_time_us() - t0;
    }
    (void)tang_plane_show(true);
    const uint32_t bytes = (uint32_t)w * (uint32_t)h * 2u;
    const uint32_t ms = (uint32_t)(busy_us / 1000u);
    tdsh_printf("planefill: %dx%d, %u bytes in %u ms (%u KB/s) at %u baud\r\n",
                w, h, (unsigned)bytes, (unsigned)ms,
                (unsigned)(ms ? bytes / ms : 0), (unsigned)s_baud);
    return 0;
}

/* ------------------------------------------------------------ registration */

static const tdsh_command_t s_fpga_commands[] = {
    { "nesload", "nesload <path>", "Stream an iNES ROM into the running core and start it",
      cmd_nesload, 0 },
    { "snesload", "snesload <path>", "Stream a SNES cartridge into the TinySNES core and start it",
      cmd_snesload, 0 },
    { "gbaload", "gbaload <path>", "Stream a GBA cartridge into the TinyGBA core and start it",
      cmd_gbaload, 0 },
    { "gbasave", "gbasave <rom-path>", "Write the cartridge's save from the running core to <rom-path>.sav",
      cmd_gbasave, 0 },
    { "pad", "pad [seconds]", "Print the pad word the core reports while buttons are pressed",
      cmd_pad, 0 },
    { "fpga", "fpga", "Ask the loaded core for its ID over UART1",
      cmd_fpga, 0 },
    { "fmon", "fmon [hold-ms]", "Read the core's clock-monitor counters over UART1",
      cmd_fmon, 0 },
    { "fpgabaud", "fpgabaud [<rate> | -raw <rate>]", "Show or negotiate the link's rate (2, 5 or 10 Mbaud)",
      cmd_fpgabaud, 0 },
    { "planefill", "planefill", "Draw a test picture into the pixel plane and time the link",
      cmd_planefill, 0 },
    { "fpgar", "fpgar <hex-addr>", "Read one desktop register over UART1",
      cmd_fpgar, 0 },
    { "fpgapipe", "fpgapipe", "Pass console bytes to and from UART1 until the port closes",
      cmd_fpgapipe, 0 },
    { "ae350run", "ae350run <program.elf> [<file>] [<file-to-send>]",
      "Load an AE350 program, serve its files, send one named file",
      cmd_ae350run, 0 },
};

int tdsh_bl616_fpga_register(void)
{
    return tdsh_register_commands(s_fpga_commands,
                                  sizeof(s_fpga_commands) / sizeof(s_fpga_commands[0]));
}
