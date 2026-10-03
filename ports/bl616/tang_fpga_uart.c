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
//
// The sequence for a cartridge is three steps and is the reference
// firmware's, unchanged:
//
//     loading = 1;  stream the file in 1024-byte frames;  loading = 0
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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "bflb_gpio.h"
#include "bflb_irq.h"
#include "bflb_mtimer.h"
#include "bflb_uart.h"
#include "ff.h"

#include "tdsh.h"
#include "tang_fpga_link.h"
#include "tang_osd.h"

int tdsh_printf(const char *fmt, ...);

/* ------------------------------------------------------------- board facts */

#define FPGA_UART_BAUD     2000000u
#define FPGA_UART_TX_PIN   GPIO_PIN_28
#define FPGA_UART_RX_PIN   GPIO_PIN_27

#define FPGA_FRAME_MAGIC   0xAAu

/* The reference firmware reads the ROM in 1024-byte pieces; keep its size.
 * 1024 data bytes make a 1025-byte frame, comfortably under the ceiling. */
#define FPGA_ROM_CHUNK     1024u

/* Single-producer (ISR) / single-consumer (shell task) ring.  head is written
 * only by the ISR and tail only by the reader, so no lock is needed. */
#define FPGA_RX_RING_SIZE  2048u
#define FPGA_RESP_MAX      256u

static struct bflb_device_s *s_uart;
static bool                 s_uart_ready;
static SemaphoreHandle_t    s_tx_lock;

static uint8_t           s_rx_ring[FPGA_RX_RING_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;

static uint8_t s_rom_chunk[FPGA_ROM_CHUNK];

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

/* ------------------------------------------------------------------ startup */

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

    const struct bflb_uart_config_s cfg = {
        .baudrate = FPGA_UART_BAUD,
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

    s_uart_ready = true;
    return 0;
}

/* ------------------------------------------------------------- TX to core */

static int uart_write(const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        if (bflb_uart_putchar(s_uart, data[i]) != 0) {
            return -1;      /* the FIFO stayed full for 100 ms: byte dropped */
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
    enum { RS_MAGIC, RS_LEN_HI, RS_LEN_LO, RS_TYPE, RS_PAYLOAD } state = RS_MAGIC;
    uint8_t  body[FPGA_RESP_MAX];
    uint16_t len = 0;
    uint16_t got = 0;
    uint8_t  type = 0;

    const uint32_t deadline = bflb_mtimer_get_time_ms() + timeout_ms;

    for (;;) {
        uint8_t byte;
        if (rx_pop(&byte)) {
            switch (state) {
            case RS_MAGIC:
                if (byte == FPGA_FRAME_MAGIC) state = RS_LEN_HI;
                break;
            case RS_LEN_HI:
                len = (uint16_t)(byte << 8);
                state = (byte < 8) ? RS_LEN_LO : RS_MAGIC;
                break;
            case RS_LEN_LO:
                len |= byte;
                got = 0;
                state = (len >= 1) ? RS_TYPE : RS_MAGIC;
                break;
            case RS_TYPE:
                type = byte;
                if (len == 1) {                     /* header-only frame */
                    if (type == want_type) return 0;
                    state = RS_MAGIC;
                } else {
                    state = RS_PAYLOAD;
                }
                break;
            case RS_PAYLOAD:
                if (got < sizeof(body)) body[got] = byte;
                got++;
                if (got == (uint16_t)(len - 1)) {
                    if (type == want_type) {
                        if (got > cap) return -1;
                        memcpy(out, body, got);
                        return (int)got;
                    }
                    state = RS_MAGIC;
                }
                break;
            }
        } else {
            if ((int32_t)(bflb_mtimer_get_time_ms() - deadline) >= 0) {
                return -1;
            }
            vTaskDelay(1);
        }
    }
}

/* ------------------------------------------------------------------ commands */

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
                    (unsigned)id, (unsigned)FPGA_UART_BAUD);
        return 0;
    }

    tdsh_printf("fpga: no answer on UART1 at %u baud (is a core loaded?)\r\n",
                (unsigned)FPGA_UART_BAUD);
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
                (unsigned)size, mapper, (unsigned)FPGA_UART_BAUD);

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

/* ------------------------------------------------------------ registration */

static const tdsh_command_t s_fpga_commands[] = {
    { "nesload", "nesload <path>", "Stream an iNES ROM into the running core and start it",
      cmd_nesload, 0 },
    { "fpga", "fpga", "Ask the loaded core for its ID over UART1",
      cmd_fpga, 0 },
};

int tdsh_bl616_fpga_register(void)
{
    return tdsh_register_commands(s_fpga_commands,
                                  sizeof(s_fpga_commands) / sizeof(s_fpga_commands[0]));
}
