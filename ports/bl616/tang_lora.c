// TinyTang — the PMOD UART, and the RYLR998 LoRa module on it.
//
// Desktop ABI 1.6 gives a socket personality 8: an 8N1 UART on the lower row,
// NRST on pin 8, TX on pin 9, RX on pin 10.  That is the wiring the module was
// proven against when the socket carried i2c_uart (PMOD-008), kept because the
// module's five-wire header goes on by name.
//
// Register map (desktop_regs.sv):
//   0x170 write: one byte into the transmit FIFO
//   0x174 read:  one byte from the receive FIFO; the read pops it
//   0x178 read:  status, [0] rx available, [1] tx full, [2] rx overrun,
//                [3] byte in flight, [19:8] rx count, [31:20] tx count
//   0x17c rw:    bit 0 NRST low, bit 1 flush (a pulse), bit 2 loopback,
//                bit 3 hold
//
// The hold bit is not a convenience.  A register round trip over the BL616
// link is slower than one character at 115200, so writing a command a byte at
// a time leaves the line idle between bytes and the module parses each byte as
// its own command, answering +ERR=1 once per byte (PMOD-008, core-log entry 3).
// Every write here therefore fills the FIFO with the transmitter held and then
// releases it, so the module sees one back-to-back line.
//
// This driver is only the transport: the module's own command set stays the
// module's, and `lora at <text>` passes it through untouched.
//
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bflb_mtimer.h"

#include "tdsh.h"
#include "tang_fpga_link.h"
#include "tang_node.h"
#include "tang_proto.h"

int tdsh_printf(const char *fmt, ...);

#define REG_TX      0x170u
#define REG_RX      0x174u
#define REG_STATUS  0x178u
#define REG_CTRL    0x17cu

#define ST_RX_AVAIL (1u << 0)
#define ST_TX_FULL  (1u << 1)
#define ST_OVERRUN  (1u << 2)
#define ST_BUSY     (1u << 3)

#define CTRL_NRST_LOW (1u << 0)
#define CTRL_FLUSH    (1u << 1)
#define CTRL_LOOPBACK (1u << 2)
#define CTRL_HOLD     (1u << 3)

/* --------------------------------------------------------------- helpers */

static bool read_reg(uint32_t address, uint32_t *value)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        if (tang_fpga_read32(address, value)) {
            return true;
        }
    }
    return false;
}

static bool write_reg(uint32_t address, uint32_t value)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        if (tang_fpga_write32(address, value)) {
            return true;
        }
    }
    return false;
}

static bool send_byte(uint8_t byte)
{
    return write_reg(REG_TX, byte);
}

/* Drain received bytes for `window_ms`.  The module's own output is ASCII;
 * anything else prints as '.' so a binary answer cannot scramble the shell. */
static int drain(uint32_t window_ms)
{
    const uint32_t start = bflb_mtimer_get_time_ms();
    int count = 0;
    while (bflb_mtimer_get_time_ms() - start < window_ms) {
        uint32_t status;
        if (!read_reg(REG_STATUS, &status)) {
            return -1;
        }
        if (status & ST_RX_AVAIL) {
            uint32_t byte;
            if (!read_reg(REG_RX, &byte)) {
                return -1;
            }
            const unsigned char c = (unsigned char)(byte & 0xffu);
            tdsh_printf("%c", (c >= 0x20 && c < 0x7f) || c == '\r' || c == '\n' ? c : '.');
            count++;
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return count;
}

static bool read_ctrl(uint32_t *ctrl)
{
    uint32_t word;
    if (!read_reg(REG_CTRL, &word)) {
        return false;
    }
    *ctrl = word & (CTRL_NRST_LOW | CTRL_LOOPBACK);
    return true;
}

/* -------------------------------------------------------------- commands */

static int cmd_lora(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;

    if (argc < 2) {
        tdsh_printf("usage: lora at <AT command>   send one line and show the reply\r\n");
        tdsh_printf("       lora ctrl              read and write the control register\r\n");
        tdsh_printf("       lora loop on|off       the UART's internal loopback\r\n");
        tdsh_printf("       lora read [ms]         drain the receive FIFO\r\n");
        tdsh_printf("       lora flush             empty both FIFOs\r\n");
        tdsh_printf("       lora status            the UART's FIFO counters\r\n");
        tdsh_printf("       lora reset             pulse the module's NRST\r\n");
        tdsh_printf("       lora ver               AT+VER?, the module's identity\r\n");
        return 1;
    }

    if (strcmp(argv[1], "status") == 0) {
        uint32_t status;
        if (!read_reg(REG_STATUS, &status)) {
            tdsh_printf("lora: the core did not answer at 0x%03lx; is a core with\r\n",
                        (unsigned long)REG_STATUS);
            tdsh_printf("lora: ABI 1.6 and personality 8 loaded?\r\n");
            return 1;
        }
        tdsh_printf("lora: status 0x%08lx, rx %lu, tx %lu%s%s%s\r\n",
                    (unsigned long)status,
                    (unsigned long)((status >> 8) & 0xfffu),
                    (unsigned long)((status >> 20) & 0xfffu),
                    (status & ST_TX_FULL) ? ", tx full" : "",
                    (status & ST_OVERRUN) ? ", rx overrun" : "",
                    (status & ST_BUSY) ? ", sending" : "");
        return 0;
    }

    if (strcmp(argv[1], "ctrl") == 0) {
        /* Separate the two halves: a read that fails points at the address
         * decode, a write that fails points at the write path, and both are
         * inside the FPGA so the wiring cannot be the cause. */
        uint32_t word = 0;
        const bool read_ok = tang_fpga_read32(REG_CTRL, &word);
        tdsh_printf("lora: read  0x%03lx -> %s 0x%08lx\r\n",
                    (unsigned long)REG_CTRL, read_ok ? "ok" : "FAIL",
                    (unsigned long)word);
        const bool write_ok = tang_fpga_write32(REG_CTRL, word);
        tdsh_printf("lora: write 0x%03lx -> %s\r\n",
                    (unsigned long)REG_CTRL, write_ok ? "ok" : "FAIL");
        uint32_t status = 0;
        if (tang_fpga_read32(REG_STATUS, &status)) {
            tdsh_printf("lora: status 0x%08lx\r\n", (unsigned long)status);
        }
        return (read_ok && write_ok) ? 0 : 1;
    }

    if (strcmp(argv[1], "loop") == 0) {
        /* The UART's internal loopback: a byte written comes straight back
         * without any wiring involved, which is the only way to tell an RTL
         * fault from a wiring fault. */
        if (argc < 3 || (strcmp(argv[2], "on") != 0 && strcmp(argv[2], "off") != 0)) {
            tdsh_printf("usage: lora loop on|off\r\n");
            return 1;
        }
        uint32_t ctrl;
        if (!read_ctrl(&ctrl)) {
            tdsh_printf("lora: cannot read the control register\r\n");
            return 1;
        }
        const uint32_t next = strcmp(argv[2], "on") == 0
                            ? (ctrl | CTRL_LOOPBACK)
                            : (ctrl & ~CTRL_LOOPBACK);
        if (!write_reg(REG_CTRL, next)) {
            tdsh_printf("lora: cannot write the control register\r\n");
            return 1;
        }
        tdsh_printf("lora: loopback %s\r\n", strcmp(argv[2], "on") == 0 ? "on" : "off");
        return 0;
    }

    if (strcmp(argv[1], "read") == 0) {
        uint32_t window = 500;
        if (argc >= 3) {
            const int ms = atoi(argv[2]);
            if (ms > 0) window = (uint32_t)ms;
        }
        const int got = drain(window);
        if (got < 0) {
            tdsh_printf("\r\nlora: the core did not answer during the read\r\n");
            return 1;
        }
        tdsh_printf("\r\nlora: %d byte(s) in %lu ms\r\n", got, (unsigned long)window);
        return 0;
    }

    if (strcmp(argv[1], "flush") == 0) {
        uint32_t ctrl;
        if (!read_ctrl(&ctrl) || !write_reg(REG_CTRL, ctrl | CTRL_FLUSH)) {
            tdsh_printf("lora: cannot flush the FIFOs\r\n");
            return 1;
        }
        tdsh_printf("lora: FIFOs flushed\r\n");
        return 0;
    }

    if (strcmp(argv[1], "reset") == 0) {
        uint32_t ctrl;
        if (!read_ctrl(&ctrl)) {
            tdsh_printf("lora: cannot reach the UART's control register\r\n");
            return 1;
        }
        if (!write_reg(REG_CTRL, ctrl | CTRL_NRST_LOW)) return 1;
        vTaskDelay(pdMS_TO_TICKS(120));
        if (!write_reg(REG_CTRL, ctrl & ~CTRL_NRST_LOW)) return 1;
        vTaskDelay(pdMS_TO_TICKS(400));
        tdsh_printf("lora: NRST pulsed\r\n");
        (void)drain(300);
        return 0;
    }

    /* `lora at ...` and `lora ver` both end up sending one line. */
    char line[96];
    size_t used = 0;
    if (strcmp(argv[1], "ver") == 0) {
        used = (size_t)snprintf(line, sizeof(line), "AT+VER?");
    } else if (strcmp(argv[1], "at") == 0) {
        for (int i = 2; i < argc; i++) {
            const size_t n = strlen(argv[i]);
            if (used + n + 2 >= sizeof(line)) {
                tdsh_printf("lora: the command is longer than %u bytes\r\n",
                            (unsigned)(sizeof(line) - 2));
                return 1;
            }
            if (used != 0) line[used++] = ' ';
            memcpy(line + used, argv[i], n);
            used += n;
        }
        line[used] = '\0';
    } else {
        tdsh_printf("lora: unknown argument %s\r\n", argv[1]);
        return 1;
    }

    uint32_t ctrl;
    if (!read_ctrl(&ctrl) || !write_reg(REG_CTRL, ctrl | CTRL_FLUSH | CTRL_HOLD)) {
        tdsh_printf("lora: cannot hold the transmitter\r\n");
        return 1;
    }
    for (size_t i = 0; i < used; i++) {
        if (!send_byte((uint8_t)line[i])) {
            tdsh_printf("lora: the transmit FIFO did not drain\r\n");
            return 1;
        }
    }
    if (!send_byte('\r') || !send_byte('\n')) {
        tdsh_printf("lora: the transmit FIFO did not take the line\r\n");
        return 1;
    }
    if (!write_reg(REG_CTRL, ctrl & ~CTRL_HOLD)) {
        tdsh_printf("lora: cannot release the transmitter\r\n");
        return 1;
    }

    const int got = drain(800);
    if (got < 0) {
        tdsh_printf("\r\nlora: the core did not answer during the read\r\n");
        return 1;
    }
    if (got == 0) {
        tdsh_printf("\r\nlora: no reply in 800 ms\r\n");
        return 1;
    }
    tdsh_printf("\r\n");
    return 0;
}


/* ------------------------------------------------- the Tang <-> node link --
 *
 * The node is a full Reticulum server; the Tang is not a peer on it. The two
 * exchange application messages (tang_proto.h) inside the LoRa framing the
 * node already speaks: one header byte, up to 179 bytes per frame, base64 into
 * the AT field. Frames go to the node's address (1); its replies arrive as
 * +RCV lines addressed to us.
 *
 * The Tang initiates every exchange. Nothing here transmits unprompted.
 */

#define NODE_ADDRESS   1
#define FRAME_CHUNK    179
#define FRAME_MAX_MSG  500
#define REPLY_TIMEOUT     4000
#define REQUEST_ATTEMPTS 10
#define RETRY_GAP_MS      1200

/* Anything stale is swept before each attempt so an old reply cannot be read
 * as this one, but only so much of it: the receive FIFO holds 512 bytes, and
 * a module whose line is streaming -- a floating receive pin does exactly
 * that -- keeps it full, so a sweep that ended only when the FIFO fell empty
 * would never let the request finish.  Past a full FIFO this is not a stale
 * reply any more, and the attempt has to go ahead and fail. */
#define STALE_DRAIN_BYTES 512
#define STALE_DRAIN_MS     20

static const char s_b64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t b64_encode(const uint8_t *in, size_t n, char *out)
{
	size_t o = 0;
	for (size_t i = 0; i < n; i += 3) {
		const uint32_t v = ((uint32_t)in[i] << 16) |
		                   (i + 1 < n ? ((uint32_t)in[i + 1] << 8) : 0) |
		                   (i + 2 < n ? in[i + 2] : 0);
		out[o++] = s_b64[(v >> 18) & 0x3F];
		out[o++] = s_b64[(v >> 12) & 0x3F];
		out[o++] = i + 1 < n ? s_b64[(v >> 6) & 0x3F] : '=';
		out[o++] = i + 2 < n ? s_b64[v & 0x3F] : '=';
	}
	out[o] = '\0';
	return o;
}

static int b64_value(char c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

static size_t b64_decode(const char *in, size_t n, uint8_t *out, size_t cap)
{
	size_t o = 0;
	uint32_t acc = 0;
	int bits = 0;
	for (size_t i = 0; i < n && in[i] != '='; i++) {
		const int v = b64_value(in[i]);
		if (v < 0) continue;
		acc = (acc << 6) | (uint32_t)v;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			if (o < cap) out[o++] = (uint8_t)((acc >> bits) & 0xFF);
		}
	}
	return o;
}

/* Send one application message to the node, in as many frames as it needs.
 * Each frame goes out as one burst: a register round trip is slower than a
 * character at 115200, so filling the FIFO with the transmitter held and then
 * releasing it is what keeps the module's AT parser happy (PMOD-009). */
static bool send_message(const uint8_t *msg, size_t len)
{
	uint32_t ctrl;
	uint8_t nonce = 5;
	const uint8_t id = 3;
	for (size_t off = 0; off < len; ) {
		size_t chunk = len - off;
		if (chunk > FRAME_CHUNK) chunk = FRAME_CHUNK;
		const bool more = off + chunk < len;

		uint8_t frame[1 + FRAME_CHUNK];
		frame[0] = (uint8_t)(((nonce++ & 0x0F) << 4) | (more ? 0x08 : 0) | (id & 0x07));
		memcpy(frame + 1, msg + off, chunk);

		char body[4 * (1 + FRAME_CHUNK) / 3 + 8];
		const size_t blen = b64_encode(frame, chunk + 1, body);

		char line[32 + sizeof(body)];
		const int head = snprintf(line, 32, "AT+SEND=%d,%u,", NODE_ADDRESS, (unsigned)blen);
		if (head <= 0) return false;
		memcpy(line + head, body, blen);
		line[head + blen] = '\0';

		if (!read_ctrl(&ctrl) || !write_reg(REG_CTRL, ctrl | CTRL_FLUSH | CTRL_HOLD))
			return false;
		for (size_t i = 0; i < (size_t)head + blen; i++) {
			if (!send_byte((uint8_t)line[i])) return false;
		}
		if (!send_byte('\r') || !send_byte('\n')) return false;
		if (!write_reg(REG_CTRL, ctrl & ~CTRL_HOLD)) return false;

		/* Wait for the module to accept the frame before the next one. */
		const uint32_t until = bflb_mtimer_get_time_ms() + 1500;
		bool ok = false;
		while (bflb_mtimer_get_time_ms() < until) {
			uint32_t st = 0, b = 0;
			if (!read_reg(REG_STATUS, &st)) return false;
			if (st & ST_RX_AVAIL) {
				if (!read_reg(REG_RX, &b)) return false;
				/* Anything the module says here is its +OK; it is not the
				 * reply, which arrives after the node answers. */
				if ((b & 0xFF) == 'K') { ok = true; break; }
			} else {
				vTaskDelay(pdMS_TO_TICKS(1));
			}
		}
		if (!ok) return false;
		off += chunk;
	}
	return true;
}

/* Reassembly: frames of one message share an id and arrive until the more
 * flag clears. Anything stranded is dropped by the caller's timeout. */
static uint8_t s_rx_buf[FRAME_MAX_MSG];
static size_t  s_rx_len;
static int     s_rx_id = -1;
static bool    s_rx_active;

static void rx_reset(void) { s_rx_len = 0; s_rx_id = -1; s_rx_active = false; }

/* Parse "+RCV=<from>,<length>,<data>,<rssi>,<snr>" and push the frame. */
static bool rcv_consume(const char *line)
{
	if (strncmp(line, "+RCV=", 5) != 0) return false;
	const char *p = line + 5;
	char *end = NULL;
	const long from = strtol(p, &end, 10);
	if (end == p || *end != ',') return false;
	p = end + 1;
	const long chars = strtol(p, &end, 10);
	if (end == p || *end != ',') return false;
	p = end + 1;
	if (chars <= 0 || chars > 240) return false;

	uint8_t frame[1 + FRAME_CHUNK];
	const size_t got = b64_decode(p, (size_t)chars, frame, sizeof frame);
	if (got < 1) return false;
	if (from != NODE_ADDRESS) return false;  /* not our link; keep waiting */

	const uint8_t header = frame[0];
	if (header & 0x80) {                     /* not our framing: ignore */
		return true;
	}
	const uint8_t id = (uint8_t)(header & 0x07);
	const bool more = (header & 0x08) != 0;
	if (!s_rx_active || id != s_rx_id) { rx_reset(); s_rx_active = true; s_rx_id = id; }
	if (s_rx_len + got - 1 > sizeof s_rx_buf) { rx_reset(); return false; }
	memcpy(s_rx_buf + s_rx_len, frame + 1, got - 1);
	s_rx_len += got - 1;
	return !more;                            /* true when the message is whole */
}

/* Read until the reply to `seq` arrives, or the window closes. Anything else
 * that completes -- a Reticulum announce from the node, a message with another
 * sequence, a body that fails its CRC -- is discarded and the wait continues:
 * the node shares this radio with its own network. */
static size_t wait_reply(uint8_t seq, uint8_t *out, size_t cap, uint32_t window_ms)
{
	const uint32_t start = bflb_mtimer_get_time_ms();
	char line[64 + 4 * (1 + FRAME_CHUNK) / 3];
	size_t line_len = 0;
	size_t got = 0;

	while (bflb_mtimer_get_time_ms() - start < window_ms) {
		uint32_t st = 0;
		if (!read_reg(REG_STATUS, &st)) return 0;
		if (!(st & ST_RX_AVAIL)) { vTaskDelay(pdMS_TO_TICKS(1)); continue; }

		uint32_t b = 0;
		if (!read_reg(REG_RX, &b)) return 0;
		const char c = (char)(b & 0xFF);
		if (c != '\r' && c != '\n') {
			if (line_len + 1 < sizeof line) line[line_len++] = c;
			continue;
		}
		if (line_len == 0) continue;

		line[line_len] = '\0';
		line_len = 0;

		if (!rcv_consume(line)) continue;   /* not ours, or still arriving */

		/* A complete message: take it only if it is the reply we asked for.
		 * The node transmits its own Reticulum traffic on this radio, so
		 * anything else has to be discarded and the wait continued. */
		tang_proto_t in;
		if (tang_proto_decode(s_rx_buf, s_rx_len, &in) &&
		    tang_proto_is_reply(in.type) && in.seq == seq) {
			got = in.length < cap ? in.length : cap;
			memcpy(out, in.body, got);
			rx_reset();
			break;
		}
		rx_reset();
	}
	return got;
}

/* One request and its reply.  The budget is the caller's: a prompt can afford
 * ten patient attempts, a window or a script cannot, and the same code serves
 * both.  Returns the reply body length, or 0. */
static size_t tang_request(uint8_t type, const uint8_t *body, uint16_t body_len,
                           uint8_t *out, uint16_t *out_len,
                           int attempts, uint32_t reply_timeout_ms)
{
	static uint8_t seq = 1;
	uint8_t msg[FRAME_MAX_MSG];
	uint8_t reply[FRAME_MAX_MSG];

	const size_t n = tang_proto_encode(type, seq, body, body_len, msg, sizeof msg);
	if (n == 0) return 0;

	/* The radio is half duplex with no carrier sense, and the node still
	 * transmits its own Reticulum traffic on it, so a request or its reply can
	 * collide with something else. The Tang is the initiator, so retrying is
	 * its job: the same sequence number is reused, which makes a late reply to
	 * an earlier attempt still valid and a duplicate harmless. */
	for (int attempt = 0; attempt < attempts; attempt++) {
		/* Drop anything stale so an old reply cannot be read as this one,
		 * within a bound: see the note on STALE_DRAIN_BYTES. */
		uint32_t ctrl;
		if (read_ctrl(&ctrl)) {
			const uint32_t until = bflb_mtimer_get_time_ms() + STALE_DRAIN_MS;
			for (int i = 0; i < STALE_DRAIN_BYTES; i++) {
				if ((int32_t)(bflb_mtimer_get_time_ms() - until) >= 0) break;
				uint32_t st = 0;
				if (!read_reg(REG_STATUS, &st) || !(st & ST_RX_AVAIL)) break;
				uint32_t b = 0;
				if (!read_reg(REG_RX, &b)) break;
			}
		}
		rx_reset();

		if (send_message(msg, n)) {
			const size_t got = wait_reply(seq, reply, sizeof reply, reply_timeout_ms);
			if (got > 0) {
				const uint16_t copy = got < *out_len ? (uint16_t)got : *out_len;
				memcpy(out, reply, copy);
				*out_len = copy;
				seq++;
				return copy;
			}
		}
		if (attempt + 1 < attempts) {
			tdsh_printf("node: no reply, retrying (%d/%d)\r\n",
			            attempt + 2, attempts);
			vTaskDelay(pdMS_TO_TICKS(RETRY_GAP_MS));
		}
	}
	return 0;
}

/* ------------------------------------------------------------- commands */

static int cmd_tang(tdsh_session_t *session, int argc, char **argv)
{
	(void)session;
	if (argc < 2) {
		tdsh_printf("usage: node status        ask the node for its status\r\n");
		tdsh_printf("       node <command>     run one of the node's commands\r\n");
		return 1;
	}

	char body[96];
	size_t used = 0;
	for (int i = 1; i < argc; i++) {
		const size_t n = strlen(argv[i]);
		if (used + n + 2 >= sizeof body) {
			tdsh_printf("node: the command is too long\r\n");
			return 1;
		}
		if (used) body[used++] = ' ';
		memcpy(body + used, argv[i], n);
		used += n;
	}
	body[used] = '\0';

	const bool is_status = strcmp(body, "status") == 0;
	char reply[TANG_PROTO_MAX_BODY];

	const int got = tang_node_ask(is_status ? TANG_REQ_STATUS : TANG_REQ_COMMAND,
	                              is_status ? NULL : (const uint8_t *)body,
	                              is_status ? 0 : (uint16_t)used,
	                              (char *)reply, sizeof reply,
	                              REQUEST_ATTEMPTS, REPLY_TIMEOUT);
	if (got < 0) {
		tdsh_printf("node: no reply from TinyNode after %d attempts of %d ms\r\n",
		            REQUEST_ATTEMPTS, REPLY_TIMEOUT);
		return 1;
	}
	tdsh_printf("%s\r\n", (const char *)reply);
	return 0;
}

/* One caller at a time: the console command and a window's request would
 * otherwise interleave frames on a half-duplex link and read each other's
 * replies.  The lock is taken without waiting, so a caller is told it is busy
 * rather than being parked behind a request that can take half a minute. */
static SemaphoreHandle_t s_node_lock;

int tang_node_ask(uint8_t type, const uint8_t *body, uint16_t len,
                  char *out, size_t cap, int attempts, uint32_t timeout_ms)
{
	if (out == NULL || cap < 2 || attempts <= 0) {
		return -1;
	}
	if (s_node_lock == NULL) {
		s_node_lock = xSemaphoreCreateMutex();
	}
	if (s_node_lock != NULL && xSemaphoreTake(s_node_lock, 0) != pdTRUE) {
		return -1;                      /* someone else is on the link */
	}

	uint16_t got = (uint16_t)(cap - 1);
	const size_t n = tang_request(type, body, len, (uint8_t *)out, &got,
	                              attempts, timeout_ms);
	out[got] = '\0';

	if (s_node_lock != NULL) {
		xSemaphoreGive(s_node_lock);
	}
	return n == 0 ? -1 : (int)got;
}

/* ----------------------------------------------------------- registration */

static const tdsh_command_t s_lora_commands[] = {
    { "lora", "lora at|ctrl|loop|read|flush|status|reset|ver",
      "Drive the RYLR998 LoRa module on the PMOD UART", cmd_lora, 0 },
    { "node", "node status | <command>",
      "Ask TinyNode for its status or run one of its commands", cmd_tang, 0 },
};

int tang_lora_register(void)
{
    return tdsh_register_commands(s_lora_commands,
                                  sizeof(s_lora_commands) / sizeof(s_lora_commands[0]));
}
