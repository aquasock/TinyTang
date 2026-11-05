/* TinyTang — host test for the LoRa transport (ports/bl616/tang_lora.c).
 *
 * The module and the FPGA both belong to the board, so this bench takes their
 * place: it implements the four PMOD UART registers as a byte FIFO, and plays
 * the RYLR998's part by answering an AT+SEND with the +OK and +RCV lines the
 * module would produce.  What that buys is the layer that actually fails
 * quietly -- base64 framing, the hold-then-burst write, frame reassembly, and
 * the rule that only the reply to the sequence asked for is accepted.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bflb_mtimer.h"
#include "tdsh.h"

/* ------------------------------------------------------------ the stubs -- */

static uint32_t now_ms;

uint32_t bflb_mtimer_get_time_ms(void) { return now_ms; }
void vTaskDelay(TickType_t ticks) { now_ms += (uint32_t)ticks; }

static int lock_taken;
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait)
{
    (void)s; (void)wait;
    if (lock_taken) return pdFALSE;      /* the real one is not re-entrant */
    lock_taken = 1;
    return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t s)
{
    (void)s;
    lock_taken = 0;
    return pdTRUE;
}

/* The shell's output, collected instead of printed. */
static char out_text[8192];
static size_t out_len;
int tdsh_printf(const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(out_text + out_len, sizeof out_text - out_len, fmt, ap);
    va_end(ap);
    if (n > 0) out_len += (size_t)n;
    return n;
}

static int registered;
int tdsh_register_commands(const tdsh_command_t *cmds, size_t count)
{
    (void)cmds;
    registered = (int)count;
    return 0;
}

/* ------------------------------------------------- the PMOD UART model -- */
/*
 * 0x170 write pushes a transmit byte, 0x174 read pops a received byte,
 * 0x178 is the status word and 0x17c the control word.  The receive FIFO is
 * filled by the test as "what the module said"; the transmit FIFO is drained
 * into a capture buffer so a test can assert exactly what left the pin.
 */

#define REG_TX     0x170u
#define REG_RX     0x174u
#define REG_STATUS 0x178u
#define REG_CTRL   0x17cu

#define ST_RX_AVAIL (1u << 0)
#define ST_TX_FULL  (1u << 1)
#define ST_OVERRUN  (1u << 2)
#define ST_BUSY     (1u << 3)

#define CTRL_NRST_LOW (1u << 0)
#define CTRL_FLUSH    (1u << 1)
#define CTRL_LOOPBACK (1u << 2)
#define CTRL_HOLD     (1u << 3)

static unsigned char rx_fifo[4096];
static size_t rx_head, rx_tail;
static unsigned char tx_capture[16384];
static size_t tx_len;
static size_t tx_pending;      /* bytes queued but not yet released */
static int tx_inflight;
static uint32_t ctrl_word;
static int flush_pulses;
static unsigned char tx_last;

/* Every control write, so the burst sequence can be asserted rather than
 * inferred from the bytes alone. */
static uint32_t ctrl_log[64];
static size_t ctrl_len;

/* When set, the module answers AT+SEND with +OK as soon as the line is
 * released, which is what send_message() waits for. */
static int module_answers_ok = 1;

static void fifo_push_rx(const char *s)
{
    while (*s) {
        if (rx_head < sizeof rx_fifo) rx_fifo[rx_head++] = (unsigned char)*s;
        s++;
    }
}

static uint32_t status_word(void)
{
    const uint32_t rx_count = (uint32_t)(rx_head - rx_tail);
    uint32_t w = 0;
    if (rx_count) w |= ST_RX_AVAIL;
    if (tx_inflight) w |= ST_BUSY;
    w |= (rx_count & 0xfffu) << 8;
    w |= ((uint32_t)(tx_pending & 0xfffu)) << 20;
    return w;
}

bool tang_fpga_read32(uint32_t address, uint32_t *value)
{
    switch (address) {
    case REG_RX:
        if (rx_tail >= rx_head) return true;      /* empty reads as zero */
        *value = rx_fifo[rx_tail++];
        return true;
    case REG_STATUS:
        *value = status_word();
        return true;
    case REG_CTRL:
        *value = ctrl_word;
        return true;
    case REG_TX:
        *value = tx_last;
        return true;
    default:
        return false;
    }
}

bool tang_fpga_write32(uint32_t address, uint32_t value)
{
    switch (address) {
    case REG_TX:
        if (tx_len < sizeof tx_capture) tx_capture[tx_len++] = (unsigned char)value;
        tx_pending++;
        tx_last = (unsigned char)value;
        if (!(ctrl_word & CTRL_HOLD)) tx_inflight = 1;
        return true;
    case REG_CTRL:
        ctrl_word = value;
        if (ctrl_len < sizeof ctrl_log / sizeof ctrl_log[0]) ctrl_log[ctrl_len++] = value;
        if (value & CTRL_FLUSH) {
            flush_pulses++;
            rx_head = rx_tail = 0;
            tx_pending = 0;
        }
        /* A released transmitter empties, as the real FIFO does; the module
         * then answers, which is what makes the wait in send_message() end. */
        if (!(value & CTRL_HOLD)) {
            tx_inflight = 0;
            tx_pending = 0;
            if (module_answers_ok && tx_len > 0) {
                fifo_push_rx("+OK\r\n");
            }
        }
        return true;
    default:
        return false;
    }
}

/* The driver itself, so its statics are reachable from the checks below. */
#include "../../ports/bl616/tang_lora.c"

/* --------------------------------------------------------------- checks -- */

static int failures;

#define CHECK(x)                                                              \
    do {                                                                      \
        if (!(x)) {                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                \
            failures++;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        long _a = (long)(a), _b = (long)(b);                                   \
        if (_a != _b) {                                                       \
            printf("FAIL %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, \
                   #a, _a, _b);                                               \
            failures++;                                                       \
        }                                                                     \
    } while (0)

static void reset_world(void)
{
    rx_head = rx_tail = 0;
    tx_len = 0;
    tx_pending = 0;
    tx_inflight = 0;
    ctrl_word = 0;
    flush_pulses = 0;
    tx_last = 0;
    ctrl_len = 0;
    out_len = 0;
    out_text[0] = '\0';
    now_ms = 1000;
    lock_taken = 0;
    module_answers_ok = 1;
    rx_reset();
}

/* Encode one frame the way send_message() does, so the test knows what the
 * module would have to reassemble. */
static size_t build_frame(uint8_t *out, size_t cap, uint8_t id, int more,
                          const uint8_t *payload, size_t n)
{
    if (n + 1 > cap) return 0;
    out[0] = (uint8_t)(((id & 0x07) | (more ? 0x08 : 0)));
    memcpy(out + 1, payload, n);
    return n + 1;
}

/* Play the module: one +RCV line carrying `frame` as base64. */
static void deliver_frame(const uint8_t *frame, size_t n, int from)
{
    char line[512];
    char b64[400];
    /* base64 with the driver's own table, so an error there shows up here. */
    b64_encode(frame, n, b64);
    snprintf(line, sizeof line, "+RCV=%d,%u,%s,42,9\r\n", from, (unsigned)(n == 0 ? 4 : ((n + 2) / 3) * 4), b64);
    /* len field is the base64 character count */
    snprintf(line, sizeof line, "+RCV=%d,%zu,%s,42,9\r\n", from, strlen(b64), b64);
    fifo_push_rx(line);
}

int main(void)
{
    /* ------------------------------------------- base64, both directions -- */
    reset_world();
    {
        /* 179 bytes is the chunk size; 0, 1 and 2 remainder cases all appear
         * inside it, so one round trip covers every padding case. */
        uint8_t src[179];
        char enc[4 * 179 / 3 + 8];
        uint8_t dec[179 + 8];
        for (int i = 0; i < 179; i++) src[i] = (uint8_t)(i * 31 + 7);
        const size_t n = b64_encode(src, sizeof src, enc);
        CHECK_EQ(n % 4, 0);
        CHECK_EQ(b64_decode(enc, n, dec, sizeof dec), sizeof src);
        CHECK(memcmp(src, dec, sizeof src) == 0);

        /* The padding rules, at each length modulo 3. */
        for (size_t k = 1; k <= 6; k++) {
            const size_t m = b64_encode(src, k, enc);
            CHECK_EQ(m % 4, 0);
            CHECK_EQ(b64_decode(enc, m, dec, sizeof dec), k);
            CHECK(memcmp(src, dec, k) == 0);
        }
        /* A '+' and a '/' must survive: they are in the table, not escaped. */
        {
            const uint8_t raw[3] = {0xfb, 0xff, 0x3e};
            const size_t m = b64_encode(raw, 3, enc);
            CHECK(memchr(enc, '+', m) != NULL || memchr(enc, '/', m) != NULL);
            CHECK_EQ(b64_decode(enc, m, dec, sizeof dec), 3);
            CHECK(memcmp(raw, dec, 3) == 0);
        }
        /* Characters outside the table are skipped, as the module's own
         * line formatting can leave whitespace in the field. */
        CHECK_EQ(b64_decode("QUJD", 4, dec, sizeof dec), 3);
        CHECK_EQ(b64_decode("QU JD", 5, dec, sizeof dec), 3);
        CHECK_EQ(b64_decode("!!", 2, dec, sizeof dec), 0);
        /* Truncated input decodes what it can rather than failing: the caller
         * checks the length it got. */
        CHECK_EQ(b64_decode("QUJ", 3, dec, sizeof dec), 2);
    }

    /* ------------------------------------------- the burst write sequence --
     * The reason the hold bit exists: the line must leave back-to-back.  What
     * this asserts is that the driver holds the transmitter, writes every
     * byte, and only then releases it. */
    reset_world();
    {
        const int rc = cmd_lora(NULL, 3, (char *[]){"lora", "at", "AT"});
        CHECK_EQ(rc, 0);
        /* Capture holds "AT\r\n" and nothing else. */
        CHECK_EQ(tx_len, 4);
        CHECK(memcmp(tx_capture, "AT\r\n", 4) == 0);
        /* First control write holds; the last releases. */
        CHECK(ctrl_len >= 2);
        CHECK(ctrl_log[0] & CTRL_HOLD);
        CHECK(ctrl_log[0] & CTRL_FLUSH);
        CHECK((ctrl_log[ctrl_len - 1] & CTRL_HOLD) == 0);
        /* The reply the module gave reached the console. */
        CHECK(strstr(out_text, "+OK") != NULL);
    }

    /* `lora at` joins its arguments with one space and takes a whole line. */
    reset_world();
    (void)cmd_lora(NULL, 3, (char *[]){"lora", "at", "AT+ADDRESS?"});
    CHECK_EQ(tx_len, 13);
    CHECK(memcmp(tx_capture, "AT+ADDRESS?\r\n", 13) == 0);

    /* `lora ver` is AT+VER? with no argument. */
    reset_world();
    (void)cmd_lora(NULL, 2, (char *[]){"lora", "ver"});
    CHECK(memcmp(tx_capture, "AT+VER?\r\n", 9) == 0);

    /* A command past the line buffer is refused, not truncated onto the wire:
     * a truncated AT command is a different command. */
    reset_world();
    {
        char big[200];
        memset(big, 'A', sizeof big - 1);
        big[sizeof big - 1] = '\0';
        CHECK_EQ(cmd_lora(NULL, 3, (char *[]){"lora", "at", big}), 1);
        CHECK_EQ(tx_len, 0);
        CHECK(strstr(out_text, "longer than") != NULL);
    }

    /* No reply inside the window is reported, and the prompt returns. */
    reset_world();
    module_answers_ok = 0;
    CHECK_EQ(cmd_lora(NULL, 3, (char *[]){"lora", "at", "AT"}), 1);
    CHECK(strstr(out_text, "no reply") != NULL);

    /* ---------------------------------------------------- +RCV reassembly --
     * One message split across three frames must reassemble; a frame from
     * another address is not ours. */
    reset_world();
    {
        uint8_t payload[400];
        uint8_t frame[200];
        for (int i = 0; i < 300; i++) payload[i] = (uint8_t)(i + 1);

        size_t n = build_frame(frame, sizeof frame, 3, 1, payload, 179);
        deliver_frame(frame, n, 1);
        CHECK(!rcv_consume("+RCV=1,0,,0,0"));    /* not the line we parse */
        /* Feed the lines the way wait_reply would, through rcv_consume. */
        char line[512], b64[400];
        b64_encode(frame, n, b64);
        snprintf(line, sizeof line, "+RCV=1,%zu,%s,42,9", strlen(b64), b64);
        CHECK(!rcv_consume(line));               /* more to come */
        CHECK_EQ(s_rx_len, 179);

        n = build_frame(frame, sizeof frame, 3, 1, payload + 179, 120);
        b64_encode(frame, n, b64);
        snprintf(line, sizeof line, "+RCV=1,%zu,%s,42,9", strlen(b64), b64);
        CHECK(!rcv_consume(line));
        CHECK_EQ(s_rx_len, 299);

        n = build_frame(frame, sizeof frame, 3, 0, payload + 299, 1);
        b64_encode(frame, n, b64);
        snprintf(line, sizeof line, "+RCV=1,%zu,%s,42,9", strlen(b64), b64);
        CHECK(rcv_consume(line));                /* the last frame: whole */
        CHECK_EQ(s_rx_len, 300);
        CHECK(memcmp(s_rx_buf, payload, 300) == 0);
    }

    /* A frame from another address is refused, and does not disturb the
     * message being assembled. */
    reset_world();
    {
        uint8_t frame[200], payload[300];
        char line[512], b64[400];
        for (int i = 0; i < 300; i++) payload[i] = (uint8_t)(i);
        size_t n = build_frame(frame, sizeof frame, 1, 1, payload, 100);
        b64_encode(frame, n, b64);
        snprintf(line, sizeof line, "+RCV=1,%zu,%s,0,0", strlen(b64), b64);
        CHECK(!rcv_consume(line));
        CHECK_EQ(s_rx_len, 100);
        /* Same frame id, but addressed to someone else. */
        snprintf(line, sizeof line, "+RCV=7,%zu,%s,0,0", strlen(b64), b64);
        CHECK(!rcv_consume(line));
        CHECK_EQ(s_rx_len, 100);                 /* unchanged */
    }

    /* A new frame id restarts the assembly rather than appending to a
     * half-received message, which would splice two messages together. */
    reset_world();
    {
        uint8_t frame[200], payload[100];
        char line[512], b64[400];
        memset(payload, 'A', sizeof payload);
        size_t n = build_frame(frame, sizeof frame, 2, 1, payload, 60);
        b64_encode(frame, n, b64);
        snprintf(line, sizeof line, "+RCV=1,%zu,%s,0,0", strlen(b64), b64);
        CHECK(!rcv_consume(line));
        CHECK_EQ(s_rx_len, 60);
        n = build_frame(frame, sizeof frame, 5, 1, payload, 20);
        b64_encode(frame, n, b64);
        snprintf(line, sizeof line, "+RCV=1,%zu,%s,0,0", strlen(b64), b64);
        CHECK(!rcv_consume(line));
        CHECK_EQ(s_rx_len, 20);                  /* restarted, not 80 */
    }

    /* Malformed lines are refused without touching the state. */
    reset_world();
    CHECK(!rcv_consume("+OK"));
    CHECK(!rcv_consume("+RCV="));
    CHECK(!rcv_consume("+RCV=1"));
    CHECK(!rcv_consume("+RCV=x,4,QUJD,0,0"));
    CHECK(!rcv_consume("+RCV=1,0,QUJD,0,0"));   /* zero characters */
    CHECK_EQ(s_rx_len, 0);
    /* A header byte with bit 7 set is not this framing: ignored, and the
     * assembler is left alone. */
    {
        uint8_t frame[4] = {0x80, 1, 2, 3};
        char line[64], b64[16];
        b64_encode(frame, 4, b64);
        snprintf(line, sizeof line, "+RCV=1,%zu,%s,0,0", strlen(b64), b64);
        CHECK(rcv_consume(line));                /* consumed, not ours */
        CHECK_EQ(s_rx_len, 0);
    }

    /* ------------------------------------------------- the retry budget --
     * A reply to another sequence is discarded and the window keeps running;
     * the reply to the sequence asked for is the one returned. */
    reset_world();
    {
        char out[128];
        /* Sequence 1 is what tang_request sends first. */
        uint8_t body[64];
        const size_t n = tang_proto_encode(TANG_REPLY | TANG_REQ_STATUS, 9,
                                           (const uint8_t *)"stale", 5,
                                           body, sizeof body);
        CHECK(n > 0);
        {
            uint8_t frame[200];
            char line[400], b64[300];
            const size_t fn = build_frame(frame, sizeof frame, 1, 0, body, n);
            b64_encode(frame, fn, b64);
            snprintf(line, sizeof line, "+RCV=1,%zu,%s,0,0", strlen(b64), b64);
            fifo_push_rx(line);
            fifo_push_rx("\r\n");
        }
        /* Then the real one, same sequence the client used. */
        const size_t m = tang_proto_encode(TANG_REPLY | TANG_REQ_STATUS, 1,
                                           (const uint8_t *)"fresh", 5,
                                           body, sizeof body);
        {
            uint8_t frame[200];
            char line[400], b64[300];
            const size_t fn = build_frame(frame, sizeof frame, 1, 0, body, m);
            b64_encode(frame, fn, b64);
            snprintf(line, sizeof line, "+RCV=1,%zu,%s,0,0", strlen(b64), b64);
            fifo_push_rx(line);
            fifo_push_rx("\r\n");
        }
        const size_t got = wait_reply(1, (uint8_t *)out, sizeof out, 500);
        CHECK_EQ(got, 5);
        CHECK(memcmp(out, "fresh", 5) == 0);
    }

    /* When only a stale sequence arrives, the window closes with nothing. */
    reset_world();
    {
        char out[64];
        uint8_t body[64];
        const size_t n = tang_proto_encode(TANG_REPLY | TANG_REQ_STATUS, 77,
                                           (const uint8_t *)"stale", 5,
                                           body, sizeof body);
        uint8_t frame[200];
        char line[400], b64[300];
        const size_t fn = build_frame(frame, sizeof frame, 1, 0, body, n);
        b64_encode(frame, fn, b64);
        snprintf(line, sizeof line, "+RCV=1,%zu,%s,0,0", strlen(b64), b64);
        fifo_push_rx(line);
        fifo_push_rx("\r\n");
        CHECK_EQ(wait_reply(1, (uint8_t *)out, sizeof out, 200), 0);
    }

    /* A request type rather than a reply is not accepted as an answer, even
     * with the right sequence. */
    reset_world();
    {
        char out[64];
        uint8_t body[64];
        const size_t n = tang_proto_encode(TANG_REQ_STATUS, 1,
                                           (const uint8_t *)"echo", 4,
                                           body, sizeof body);
        uint8_t frame[200];
        char line[400], b64[300];
        const size_t fn = build_frame(frame, sizeof frame, 1, 0, body, n);
        b64_encode(frame, fn, b64);
        snprintf(line, sizeof line, "+RCV=1,%zu,%s,0,0", strlen(b64), b64);
        fifo_push_rx(line);
        fifo_push_rx("\r\n");
        CHECK_EQ(wait_reply(1, (uint8_t *)out, sizeof out, 200), 0);
    }

    /* ------------------------------------------------ the client's lock --
     * Only one caller may be on the half-duplex link; a second is told so
     * rather than queued behind a request that can take half a minute. */
    reset_world();
    {
        char out[64];
        lock_taken = 1;
        CHECK_EQ(tang_node_ask(TANG_REQ_STATUS, NULL, 0, out, sizeof out, 1, 10), -1);
        lock_taken = 0;
    }

    /* A zero-length or undersized output buffer is refused. */
    CHECK_EQ(tang_node_ask(TANG_REQ_STATUS, NULL, 0, NULL, 32, 1, 10), -1);
    {
        char out[1];
        CHECK_EQ(tang_node_ask(TANG_REQ_STATUS, NULL, 0, out, 1, 1, 10), -1);
    }

    /* ------------------------------------------------- registration ---- */
    CHECK_EQ(tang_lora_register(), 0);
    CHECK_EQ(registered, 2);

    if (failures) {
        printf("tb_tang_lora: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_tang_lora: all checks passed\n");
    return 0;
}
