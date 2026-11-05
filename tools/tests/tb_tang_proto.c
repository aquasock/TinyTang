/* TinyTang — host test for the TinyNode application protocol (tang_proto.c).
 *
 * This is the layer that decides whether a reply is the reply the Tang asked
 * for, and whether a message that lost a frame in the LoRa link is refused
 * instead of displayed as a short answer.  Both are things that fail quietly
 * on the wire, so they are pinned here.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "tang_proto.h"

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
        if (_a != _b) {                                                        \
            printf("FAIL %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, \
                   #a, _a, _b);                                                \
            failures++;                                                       \
        }                                                                     \
    } while (0)

int main(void)
{
    unsigned char buf[TANG_PROTO_MAX_PACKET + 16];
    unsigned char body[TANG_PROTO_MAX_BODY + 16];
    tang_proto_t msg;

    /* ------------------------------------------------ CRC-16/CCITT-FALSE --
     * The standard check value: "123456789" gives 0x29B1.  A CRC that is
     * subtly wrong still round-trips against itself, so the only test that
     * catches it is a published vector, and the node implements the same one
     * independently. */
    {
        const char *s = "123456789";
        uint16_t crc = 0xffffu;
        for (int i = 0; i < 9; i++) {
            crc = tang_proto_crc16(crc, (uint8_t)s[i]);
        }
        CHECK_EQ(crc, 0x29b1);
    }
    /* Initial value 0xffff over nothing is 0xffff, and a single zero byte
     * gives the value the polynomial produces from 0xff00. */
    CHECK_EQ(tang_proto_crc16(0xffffu, 0x00u), 0xe1f0);

    /* ------------------------------------------------------- round trips -- */
    for (int i = 0; i < (int)sizeof(body); i++) {
        body[i] = (unsigned char)(i * 7 + 3);
    }

    /* An empty body: a status request carries nothing. */
    {
        const size_t n = tang_proto_encode(TANG_REQ_STATUS, 1, NULL, 0,
                                           buf, sizeof buf);
        CHECK_EQ(n, TANG_PROTO_OVERHEAD);
        CHECK(buf[0] == TANG_PROTO_VERSION);
        CHECK(buf[1] == TANG_REQ_STATUS);
        CHECK(buf[2] == 1);
        CHECK(buf[3] == 0);
        CHECK(buf[4] == 0 && buf[5] == 0);
        CHECK(tang_proto_decode(buf, n, &msg));
        CHECK_EQ(msg.length, 0);
        CHECK_EQ(msg.seq, 1);
        CHECK_EQ(msg.type, TANG_REQ_STATUS);
    }

    /* A command word, which is what `node env` sends. */
    {
        const char *cmd = "env";
        const size_t n = tang_proto_encode(TANG_REQ_COMMAND, 7,
                                           (const uint8_t *)cmd, 3,
                                           buf, sizeof buf);
        CHECK_EQ(n, TANG_PROTO_OVERHEAD + 3);
        CHECK(tang_proto_decode(buf, n, &msg));
        CHECK_EQ(msg.length, 3);
        CHECK_EQ(msg.seq, 7);
        CHECK(memcmp(msg.body, "env", 3) == 0);
    }

    /* Lengths across the one-byte boundary, so the big-endian field is
     * exercised on both sides of it. */
    for (uint16_t len = 0; len <= 300; len++) {
        const size_t n = tang_proto_encode(TANG_REQ_SEND, (uint8_t)(len & 0xff),
                                           body, len, buf, sizeof buf);
        if (n != (size_t)TANG_PROTO_OVERHEAD + len) {
            printf("FAIL encode len %u gave %zu\n", (unsigned)len, n);
            failures++;
            break;
        }
        if (!tang_proto_decode(buf, n, &msg) || msg.length != len ||
            msg.seq != (uint8_t)(len & 0xff) ||
            memcmp(msg.body, body, len) != 0) {
            printf("FAIL round trip len %u\n", (unsigned)len);
            failures++;
            break;
        }
    }

    /* The largest body the framing allows, and one byte past it. */
    {
        const size_t n = tang_proto_encode(TANG_REQ_COMMAND, 9, body,
                                           TANG_PROTO_MAX_BODY,
                                           buf, sizeof buf);
        CHECK_EQ(n, TANG_PROTO_MAX_PACKET);
        CHECK(tang_proto_decode(buf, n, &msg));
        CHECK_EQ(msg.length, TANG_PROTO_MAX_BODY);
        CHECK_EQ(tang_proto_encode(TANG_REQ_COMMAND, 9, body,
                                   TANG_PROTO_MAX_BODY + 1, buf, sizeof buf), 0);
    }

    /* --------------------------------------------------- encode refusals -- */
    CHECK_EQ(tang_proto_encode(TANG_REQ_COMMAND, 1, body, 4, NULL, 64), 0);
    /* The framing costs 8 bytes, so a 4-byte body needs exactly 12. */
    CHECK_EQ(tang_proto_encode(TANG_REQ_COMMAND, 1, body, 4, buf, 11), 0);
    CHECK_EQ(tang_proto_encode(TANG_REQ_COMMAND, 1, body, 4, buf, 12), 12);
    /* A length with no body pointer is a caller bug, not an empty message. */
    CHECK_EQ(tang_proto_encode(TANG_REQ_COMMAND, 1, NULL, 4, buf, 64), 0);

    /* --------------------------------------------------- decode refusals -- */
    {
        const size_t n = tang_proto_encode(TANG_REQ_COMMAND, 3,
                                           (const uint8_t *)"status", 6,
                                           buf, sizeof buf);
        CHECK(n > 0);
        CHECK(tang_proto_decode(buf, n, &msg));

        CHECK(!tang_proto_decode(NULL, n, &msg));
        CHECK(!tang_proto_decode(buf, n, NULL));
        CHECK(!tang_proto_decode(buf, TANG_PROTO_OVERHEAD - 1, &msg));
        /* One byte short: what a lost frame looks like when the length field
         * still claims the full body.  This is the whole reason the layer
         * exists.  Note what refuses it: the CRC, not the length check, because
         * the CRC's span is derived from the buffer's actual length, so a
         * truncated message covers different bytes and cannot match.  Planting
         * a fault in decode()'s exact-length check therefore does not fail
         * this test -- that check is defence in depth against a message whose
         * length field merely disagrees with a CRC that happens to validate,
         * which no natural truncation produces.  See docs/qualification.md. */
        CHECK(!tang_proto_decode(buf, n - 1, &msg));
        /* One byte long. */
        CHECK(!tang_proto_decode(buf, n + 1, &msg));

        /* A corrupted body byte fails the CRC. */
        unsigned char c[TANG_PROTO_MAX_PACKET];
        memcpy(c, buf, n);
        c[TANG_PROTO_HEADER] ^= 0x01;
        CHECK(!tang_proto_decode(c, n, &msg));

        /* A corrupted CRC fails too, and so does swapping its two bytes. */
        memcpy(c, buf, n);
        c[n - 1] ^= 0x01;
        CHECK(!tang_proto_decode(c, n, &msg));
        memcpy(c, buf, n);
        unsigned char t = c[n - 2];
        c[n - 2] = c[n - 1];
        c[n - 1] = t;
        CHECK(!tang_proto_decode(c, n, &msg));

        /* An unknown version, with a CRC recomputed over it so the version
         * check is what refuses it rather than the CRC. */
        memcpy(c, buf, n);
        c[0] = TANG_PROTO_VERSION + 1;
        {
            uint16_t crc = 0xffffu;
            for (size_t i = 0; i < n - TANG_PROTO_CRC; i++) {
                crc = tang_proto_crc16(crc, c[i]);
            }
            c[n - 2] = (unsigned char)(crc >> 8);
            c[n - 1] = (unsigned char)crc;
        }
        CHECK(!tang_proto_decode(c, n, &msg));

        /* A length field claiming more than the framing can carry, again
         * with a valid CRC so the bound is what refuses it. */
        memcpy(c, buf, n);
        c[4] = (unsigned char)((TANG_PROTO_MAX_BODY + 1) >> 8);
        c[5] = (unsigned char)(TANG_PROTO_MAX_BODY + 1);
        {
            uint16_t crc = 0xffffu;
            for (size_t i = 0; i < n - TANG_PROTO_CRC; i++) {
                crc = tang_proto_crc16(crc, c[i]);
            }
            c[n - 2] = (unsigned char)(crc >> 8);
            c[n - 1] = (unsigned char)crc;
        }
        CHECK(!tang_proto_decode(c, n, &msg));
    }

    /* ---------------------------------------------- reply type matching --
     * The client accepts a reply only when its type is a reply and its
     * sequence matches, so both halves of that rule are pinned here. */
    CHECK_EQ(tang_proto_reply_type(TANG_REQ_STATUS), 0x81);
    CHECK_EQ(tang_proto_reply_type(TANG_REQ_COMMAND), 0x82);
    CHECK_EQ(tang_proto_reply_type(TANG_REQ_MESSAGES), 0x83);
    CHECK_EQ(tang_proto_reply_type(TANG_REQ_BODY), 0x84);
    CHECK_EQ(tang_proto_reply_type(TANG_REQ_SEND), 0x85);
    CHECK(tang_proto_is_reply(0x81));
    CHECK(tang_proto_is_reply(0xff));
    CHECK(!tang_proto_is_reply(TANG_REQ_STATUS));
    CHECK(!tang_proto_is_reply(TANG_REQ_COMMAND));
    CHECK(!tang_proto_is_reply(0x00));
    CHECK(!tang_proto_is_reply(0x7f));
    /* Every request type maps to a reply type that is one, and no request
     * type is itself a reply. */
    for (uint8_t t = 0x01; t <= 0x0f; t++) {
        CHECK(!tang_proto_is_reply(t));
        CHECK(tang_proto_is_reply(tang_proto_reply_type(t)));
    }

    /* The request and reply for one exchange carry the same sequence, which is
     * how a late answer to an earlier attempt is recognised as stale. */
    {
        unsigned char replybuf[64];
        const size_t n = tang_proto_encode(TANG_REQ_STATUS, 42, NULL, 0,
                                           buf, sizeof buf);
        CHECK(tang_proto_decode(buf, n, &msg));
        CHECK_EQ(msg.seq, 42);
        const uint8_t reply_type = tang_proto_reply_type(msg.type);
        const size_t m = tang_proto_encode(reply_type, msg.seq,
                                           (const uint8_t *)"ok", 2,
                                           replybuf, sizeof replybuf);
        CHECK_EQ(m, TANG_PROTO_OVERHEAD + 2);
        CHECK(tang_proto_decode(replybuf, m, &msg));
        CHECK_EQ(msg.seq, 42);
        CHECK(tang_proto_is_reply(msg.type));
    }

    if (failures) {
        printf("tb_tang_proto: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_tang_proto: all checks passed\n");
    return 0;
}
