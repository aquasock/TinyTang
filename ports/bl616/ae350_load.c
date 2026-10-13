// TinyTang — sending programs and files to a TinyNES core's AE350 boot ROM.
// See ae350_load.h for the protocol.

#include "ae350_load.h"

#include <string.h>

#define REPLY_TIMEOUT_MS   2000u     /* the boot ROM answers a block at once */
#define READBACK_MS_PER_MB 1000u     /* plus its read-back of the image */

uint32_t ae350_crc32(uint32_t crc, const uint8_t *data, size_t length)
{
    crc = ~crc;
    while (length--) {
        crc ^= *data++;
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

/* ACK, or NAK and its reason; anything else is the board's text. */
static int reply(const ae350_link_t *link, uint32_t timeout_ms, char *reason)
{
    for (;;) {
        const int byte = link->read_byte(link->ctx, timeout_ms);
        if (byte < 0) {
            return AE350_ERR_TIMEOUT;
        }
        if (byte == AE350_LOAD_ACK) {
            return AE350_OK;
        }
        if (byte == AE350_LOAD_NAK) {
            const int why = link->read_byte(link->ctx, timeout_ms);
            if (reason) {
                *reason = why < 0 ? 0 : (char)why;
            }
            return AE350_ERR_NAK;
        }
        if (link->text) {
            link->text(link->ctx, (uint8_t)byte);
        }
    }
}

int ae350_send_image(const ae350_link_t *link, uint32_t address,
                     uint32_t length, uint32_t entry, int retries,
                     uint8_t *block, char *reason)
{
    if (reason) {
        *reason = 0;
    }
    if (address < AE350_SDRAM_BASE || length == 0 ||
        length > AE350_SDRAM_END - address || entry < address ||
        entry - address >= length) {
        return AE350_ERR_RANGE;
    }

    /* The whole image's CRC goes in the header, so read it once first. */
    uint32_t crc = 0;
    for (uint32_t done = 0; done < length;) {
        uint32_t count = length - done;
        if (count > AE350_LOAD_BLOCK) {
            count = AE350_LOAD_BLOCK;
        }
        if (link->image(link->ctx, done, block, count) != 0) {
            return AE350_ERR_IMAGE;
        }
        crc = ae350_crc32(crc, block, count);
        done += count;
    }

    uint8_t header[24];
    put32(header, AE350_LOAD_MAGIC);
    put32(header + 4, address);
    put32(header + 8, length);
    put32(header + 12, entry);
    put32(header + 16, crc);
    put32(header + 20, ae350_crc32(0, header, 20));
    if (link->write(link->ctx, header, sizeof(header)) != 0) {
        return AE350_ERR_WRITE;
    }
    int rc = reply(link, REPLY_TIMEOUT_MS, reason);
    if (rc != AE350_OK) {
        return rc;
    }

    for (uint32_t done = 0; done < length;) {
        uint32_t count = length - done;
        if (count > AE350_LOAD_BLOCK) {
            count = AE350_LOAD_BLOCK;
        }
        if (link->image(link->ctx, done, block, count) != 0) {
            return AE350_ERR_IMAGE;
        }
        uint8_t tail[4];
        put32(tail, ae350_crc32(0, block, count));
        for (int attempt = 0;; attempt++) {
            if (link->write(link->ctx, block, count) != 0 ||
                link->write(link->ctx, tail, sizeof(tail)) != 0) {
                return AE350_ERR_WRITE;
            }
            rc = reply(link, REPLY_TIMEOUT_MS, reason);
            if (rc == AE350_OK) {
                break;
            }
            if (rc != AE350_ERR_NAK || attempt >= retries) {
                return rc;
            }
        }
        done += count;
    }

    /* The boot ROM reads the image back from the SDRAM and answers again. */
    return reply(link, REPLY_TIMEOUT_MS + READBACK_MS_PER_MB * (length >> 20) +
                       READBACK_MS_PER_MB, reason);
}

/* ----------------------------------------------------------------- ELF */

#define PT_LOAD   1u
#define EM_RISCV  243u

int ae350_elf_parse(ae350_elf_t *elf,
                    int (*read)(void *ctx, uint32_t offset, uint8_t *dst,
                                uint32_t length),
                    void *ctx)
{
    uint8_t h[52];
    memset(elf, 0, sizeof(*elf));
    if (read(ctx, 0, h, sizeof(h)) != 0) {
        return AE350_ERR_IMAGE;
    }
    if (memcmp(h, "\x7f" "ELF", 4) != 0 || h[4] != 1 || h[5] != 1 ||
        get16(h + 18) != EM_RISCV) {
        return AE350_ERR_IMAGE;
    }
    const uint32_t phoff = get32(h + 28);
    const uint16_t phentsize = get16(h + 42);
    const uint16_t phnum = get16(h + 44);
    if (phentsize < 32) {
        return AE350_ERR_IMAGE;
    }
    elf->entry = get32(h + 24);
    for (unsigned i = 0; i < phnum; i++) {
        uint8_t p[32];
        if (read(ctx, phoff + i * phentsize, p, sizeof(p)) != 0) {
            return AE350_ERR_IMAGE;
        }
        const uint32_t size = get32(p + 16);
        if (get32(p) != PT_LOAD || size == 0) {
            continue;
        }
        if (elf->count == AE350_ELF_MAX_SEGMENTS) {
            return AE350_ERR_IMAGE;
        }
        ae350_segment_t *s = &elf->segment[elf->count++];
        s->offset = get32(p + 4);
        s->address = get32(p + 12);        /* p_paddr */
        s->size = size;
        if (s->address > UINT32_MAX - size) {
            return AE350_ERR_RANGE;
        }
    }
    if (elf->count == 0) {
        return AE350_ERR_IMAGE;
    }
    elf->base = UINT32_MAX;
    for (unsigned i = 0; i < elf->count; i++) {
        const ae350_segment_t *s = &elf->segment[i];
        if (s->address < elf->base) {
            elf->base = s->address;
        }
        if (s->address + s->size > elf->end) {
            elf->end = s->address + s->size;
        }
    }
    if (elf->base < AE350_SDRAM_BASE || elf->end > AE350_SDRAM_END ||
        elf->entry < elf->base || elf->entry >= elf->end) {
        return AE350_ERR_RANGE;
    }
    return AE350_OK;
}

int ae350_elf_image(const ae350_elf_t *elf,
                    int (*read)(void *ctx, uint32_t offset, uint8_t *dst,
                                uint32_t length),
                    void *ctx, uint32_t offset, uint8_t *dst, uint32_t length)
{
    /* Zero, then copy in whatever part of each segment falls in the range;
     * a later segment wins where two overlap, as load_ae350.py does. */
    memset(dst, 0, length);
    const uint32_t lo = elf->base + offset, hi = lo + length;
    for (unsigned i = 0; i < elf->count; i++) {
        const ae350_segment_t *s = &elf->segment[i];
        const uint32_t a = s->address > lo ? s->address : lo;
        const uint32_t b = s->address + s->size < hi ? s->address + s->size : hi;
        if (a >= b) {
            continue;
        }
        if (read(ctx, s->offset + (a - s->address), dst + (a - lo), b - a) != 0) {
            return AE350_ERR_IMAGE;
        }
    }
    return AE350_OK;
}
