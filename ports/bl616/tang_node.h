// The Tang's client for TinyNode, for callers other than the console.
//
// SPDX-License-Identifier: MIT
//
// One application message out and its reply back, over the RYLR998 link on the
// PMOD UART.  The message types are in tang_proto.h; the radio, the framing and
// the retry policy are in tang_lora.c.
//
// The console's `node` command and a window share this client, and only one of
// them can be on the link at a time -- two exchanges would interleave frames
// and each would read the other's reply -- so a call while another is running
// answers -1 immediately rather than waiting.

#ifndef TANG_NODE_H
#define TANG_NODE_H

#include <stddef.h>
#include <stdint.h>

// Send one request and wait for its reply.  `out` receives the reply body,
// NUL-terminated, up to cap-1 bytes; the answer is the body length, or -1 when
// the client was busy, the request could not be sent, or no reply came within
// `attempts` tries of `timeout_ms`.
int tang_node_ask(uint8_t type, const uint8_t *body, uint16_t len,
                  char *out, size_t cap, int attempts, uint32_t timeout_ms);

#endif // TANG_NODE_H
