// TinyNode <-> Tang application protocol.  See tang_proto.h.
//
// SPDX-License-Identifier: MIT

#include "tang_proto.h"

#include <string.h>

uint16_t tang_proto_crc16(uint16_t crc, uint8_t data)
{
    crc ^= (uint16_t)data << 8;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                              : (uint16_t)(crc << 1);
    }
    return crc;
}

size_t tang_proto_encode(uint8_t type, uint8_t seq,
                         const uint8_t *body, uint16_t length,
                         uint8_t *out, size_t capacity)
{
    if (length > TANG_PROTO_MAX_BODY) {
        return 0;
    }
    const size_t total = (size_t)TANG_PROTO_OVERHEAD + length;
    if (out == NULL || capacity < total) {
        return 0;
    }
    if (length != 0 && body == NULL) {
        return 0;
    }

    out[0] = TANG_PROTO_VERSION;
    out[1] = type;
    out[2] = seq;
    out[3] = 0;
    out[4] = (uint8_t)(length >> 8);
    out[5] = (uint8_t)length;
    if (length != 0) {
        memcpy(out + TANG_PROTO_HEADER, body, length);
    }

    /* The CRC covers the header and the body, and nothing of itself. */
    uint16_t crc = 0xffffu;
    const size_t covered = total - TANG_PROTO_CRC;
    for (size_t i = 0; i < covered; i++) {
        crc = tang_proto_crc16(crc, out[i]);
    }
    out[covered] = (uint8_t)(crc >> 8);
    out[covered + 1] = (uint8_t)crc;
    return total;
}

bool tang_proto_decode(const uint8_t *in, size_t length, tang_proto_t *out)
{
    if (in == NULL || out == NULL) {
        return false;
    }
    if (length < TANG_PROTO_OVERHEAD) {
        return false;
    }
    const uint16_t body_length = (uint16_t)(((uint16_t)in[4] << 8) | in[5]);
    if (body_length > TANG_PROTO_MAX_BODY) {
        return false;
    }
    /* The buffer must be exactly what the header claims: a message that lost a
     * frame is shorter than its own length field says, and is refused here. */
    if (length != (size_t)TANG_PROTO_OVERHEAD + body_length) {
        return false;
    }

    const size_t covered = length - TANG_PROTO_CRC;
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < covered; i++) {
        crc = tang_proto_crc16(crc, in[i]);
    }
    const uint16_t carried = (uint16_t)(((uint16_t)in[covered] << 8) | in[covered + 1]);
    if (crc != carried) {
        return false;
    }

    if (in[0] != TANG_PROTO_VERSION) {
        return false;
    }

    out->version = in[0];
    out->type = in[1];
    out->seq = in[2];
    out->flags = in[3];
    out->length = body_length;
    out->body = in + TANG_PROTO_HEADER;
    return true;
}

uint8_t tang_proto_reply_type(uint8_t request_type)
{
    return (uint8_t)(request_type | TANG_REPLY);
}

bool tang_proto_is_reply(uint8_t type)
{
    return (type & TANG_REPLY) != 0;
}
