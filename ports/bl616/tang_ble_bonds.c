// TinyTang — Bluetooth pairings kept on the SD card.  See tang_ble_bonds.h.
//
// SPDX-License-Identifier: MIT

#include "tang_ble_bonds.h"

#include <string.h>

uint32_t tang_bonds_crc32(const uint8_t *p, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* One record: valid, addr_type, addr[6], name[24], enc_size, flags, keys,
 * ltk_rand[8], ltk_ediv[2], ltk_val[16], irk_val[16], then zero padding --
 * 78 bytes of fields in an 80-byte record. */
static void encode_record(const tang_bond_t *b, uint8_t *r)
{
    memset(r, 0, TANG_BOND_RECORD_LEN);
    if (!b->valid) {
        return;
    }
    uint8_t *p = r;
    *p++ = 1;
    *p++ = b->addr_type;
    memcpy(p, b->addr, 6);
    p += 6;
    for (int i = 0; i < TANG_BOND_NAME_LEN - 1 && b->name[i]; i++) {
        p[i] = (uint8_t)b->name[i];
    }
    p += TANG_BOND_NAME_LEN;
    *p++ = b->enc_size;
    *p++ = b->flags;
    put_u16(p, b->keys);
    p += 2;
    memcpy(p, b->ltk_rand, 8);
    p += 8;
    memcpy(p, b->ltk_ediv, 2);
    p += 2;
    memcpy(p, b->ltk_val, 16);
    p += 16;
    memcpy(p, b->irk_val, 16);
}

static void decode_record(const uint8_t *r, tang_bond_t *b)
{
    memset(b, 0, sizeof(*b));
    const uint8_t *p = r;
    b->valid = *p++ == 1;
    if (!b->valid) {
        return;
    }
    b->addr_type = *p++;
    memcpy(b->addr, p, 6);
    p += 6;
    memcpy(b->name, p, TANG_BOND_NAME_LEN);
    b->name[TANG_BOND_NAME_LEN - 1] = '\0';
    p += TANG_BOND_NAME_LEN;
    b->enc_size = *p++;
    b->flags = *p++;
    b->keys = (uint16_t)(p[0] | (p[1] << 8));
    p += 2;
    memcpy(b->ltk_rand, p, 8);
    p += 8;
    memcpy(b->ltk_ediv, p, 2);
    p += 2;
    memcpy(b->ltk_val, p, 16);
    p += 16;
    memcpy(b->irk_val, p, 16);
}

size_t tang_bonds_encode(const tang_bond_t b[TANG_BOND_SLOTS], uint8_t *out)
{
    memcpy(out, "TTBL", 4);
    out[4] = TANG_BOND_VERSION;
    out[5] = TANG_BOND_SLOTS;
    out[6] = 0;
    out[7] = 0;
    for (int i = 0; i < TANG_BOND_SLOTS; i++) {
        encode_record(&b[i], out + 8 + i * TANG_BOND_RECORD_LEN);
    }
    const size_t body = 8 + TANG_BOND_SLOTS * TANG_BOND_RECORD_LEN;
    put_u32(out + body, tang_bonds_crc32(out, body));
    return TANG_BOND_FILE_LEN;
}

int tang_bonds_decode(const uint8_t *in, size_t len, tang_bond_t b[TANG_BOND_SLOTS])
{
    memset(b, 0, sizeof(tang_bond_t) * TANG_BOND_SLOTS);
    const size_t body = 8 + TANG_BOND_SLOTS * TANG_BOND_RECORD_LEN;
    if (len != TANG_BOND_FILE_LEN || memcmp(in, "TTBL", 4) != 0 ||
        in[4] != TANG_BOND_VERSION || in[5] != TANG_BOND_SLOTS ||
        get_u32(in + body) != tang_bonds_crc32(in, body)) {
        return -1;
    }
    for (int i = 0; i < TANG_BOND_SLOTS; i++) {
        decode_record(in + 8 + i * TANG_BOND_RECORD_LEN, &b[i]);
    }
    return 0;
}
