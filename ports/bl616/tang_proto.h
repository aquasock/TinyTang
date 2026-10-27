// TinyNode <-> Tang application protocol, above the LoRa framing.
//
// SPDX-License-Identifier: MIT
//
// The RYLR998 link carries TinyNode's framing (nano-node/include/lora_frame.h):
// one header byte, up to 179 bytes of payload per frame, base64 into the AT
// field, reassembled until the more flag clears.  That framing has no length
// field and no checksum, so a message that loses a middle frame reassembles
// into a complete-looking, silently short packet (core-reference FRM-008).
//
// This layer fixes that.  Every application message carries its body length
// and a CRC over everything before it, so a truncated or corrupt message is
// detected and discarded instead of displayed:
//
//   0      version     TANG_PROTO_VERSION
//   1      type        request 0x01-0x0f, reply 0x81-0x8f
//   2      sequence    chosen by the Tang, echoed in the reply
//   3      flags       reserved, zero
//   4..5   length      body length, big-endian
//   6..    body
//   end-2..end-1       CRC-16/CCITT-FALSE over bytes 0..end-3
//
// The Tang is the only party that initiates: every request comes from it and
// the node only answers.  A request and its reply carry the same sequence
// number, so a stale reply is recognisable.
//
// This file is deliberately plain C, with no platform, Arduino or C++ library
// dependency beyond <stdint.h> and <stddef.h>, so the same two files serve both
// ends: nano-node/src/tang_proto.* on the ESP32 and ports/bl616/tang_proto.*
// on the Tang.

#ifndef TANG_PROTO_H
#define TANG_PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TANG_PROTO_VERSION   1
#define TANG_PROTO_HEADER    6      /* version..length */
#define TANG_PROTO_CRC       2      /* trailing CRC-16 */
#define TANG_PROTO_OVERHEAD  (TANG_PROTO_HEADER + TANG_PROTO_CRC)

/* The framing carries at most a Reticulum MTU (500 bytes, lora_frame.h), so an
 * application message cannot exceed that.  A body is the rest. */
#define TANG_PROTO_MAX_PACKET 500
#define TANG_PROTO_MAX_BODY   (TANG_PROTO_MAX_PACKET - TANG_PROTO_OVERHEAD)

/* Request types, all Tang -> node. */
#define TANG_REQ_STATUS     0x01    /* body empty; reply: the node's status text */
#define TANG_REQ_COMMAND    0x02    /* body: a command word, e.g. "env" */
#define TANG_REQ_MESSAGES   0x03    /* body: 4-byte sequence point */
#define TANG_REQ_BODY       0x04    /* body: 2-byte index into the last list */
#define TANG_REQ_SEND       0x05    /* body: 16-byte address, then the text */

/* Reply types, node -> Tang: the request type with the high bit set. */
#define TANG_REPLY          0x80

typedef struct {
    uint8_t version;
    uint8_t type;
    uint8_t seq;
    uint8_t flags;
    uint16_t length;                 /* body length */
    const uint8_t *body;             /* points into the caller's buffer */
} tang_proto_t;

/* CRC-16/CCITT-FALSE: polynomial 0x1021, initial value 0xFFFF, no reflection
 * and no final xor.  The same function a core's desktop_regs uses. */
uint16_t tang_proto_crc16(uint16_t crc, uint8_t data);

/* Encode one message into `out`.  Returns the number of bytes written, or 0 if
 * the body is too long or the buffer is too small. */
size_t tang_proto_encode(uint8_t type, uint8_t seq,
                         const uint8_t *body, uint16_t length,
                         uint8_t *out, size_t capacity);

/* Validate and parse a received message.  Returns false on a short buffer, a
 * version this build does not speak, a length that disagrees with the buffer,
 * or a CRC mismatch -- which is what a message missing a frame looks like. */
bool tang_proto_decode(const uint8_t *in, size_t length, tang_proto_t *out);

/* The reply type for a request type, and whether a type is a reply. */
uint8_t tang_proto_reply_type(uint8_t request_type);
bool tang_proto_is_reply(uint8_t type);

#endif /* TANG_PROTO_H */
