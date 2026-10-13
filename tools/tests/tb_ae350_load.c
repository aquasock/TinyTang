/*
 * Host test for ports/bl616/ae350_load.c, the sender ae350run uses to load
 * programs and files into a TinyNES core's AE350.
 *
 * The receiver here models TinyNES's boot ROM (firmware/boot/load.c): it
 * checks the header's CRC, the address range, each block's CRC and the
 * whole image's CRC, answers ACK or NAK and a reason, and keeps the bytes it
 * accepted. Faults are injected on the wire: a corrupted block is NAKed and
 * must be sent again, a receiver that never answers must time out, a block
 * corrupted more often than the retries allow must fail, and text between
 * replies must reach the text callback.
 *
 *   tools/tests/test_ae350_load.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae350_load.h"

static int fails, checks;

static void check_int(const char *what, long got, long want)
{
    checks++;
    if (got != want) {
        fails++;
        printf("  FAIL: %s (got %ld, want %ld)\n", what, got, want);
    }
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}

/* ------------------------------------------------------------ receiver */

#define MEM_BYTES (4u << 20)

typedef struct {
    /* What the sender wrote, gathered until a whole unit is in. */
    uint8_t in[AE350_LOAD_BLOCK + 64];
    size_t in_len;
    int state;                  /* 0 header, 1 blocks, 2 done */
    uint32_t address, length, entry, image_crc, done;
    uint8_t *mem;               /* MEM_BYTES from AE350_SDRAM_BASE */
    /* Replies waiting to be read. */
    uint8_t out[256];
    size_t out_head, out_tail;
    /* Faults. */
    int corrupt_blocks;         /* flip a bit in this many blocks */
    int corrupt_only_block;     /* -1: any; else only this block index */
    int silent;                 /* never answer */
    const char *chatter;        /* text sent before the first reply */
    /* Counts. */
    int naks, blocks_seen;
    int text_bytes;
} receiver_t;

static void say(receiver_t *r, uint8_t byte)
{
    if (!r->silent) {
        r->out[r->out_tail++ % sizeof(r->out)] = byte;
    }
}

static void take(receiver_t *r)
{
    if (r->state == 0 && r->in_len >= 24) {
        const uint8_t *h = r->in;
        if (get32(h) != AE350_LOAD_MAGIC ||
            ae350_crc32(0, h, 20) != get32(h + 20)) {
            say(r, AE350_LOAD_NAK); say(r, 'H');
            r->naks++;
            r->state = 2;
            return;
        }
        r->address = get32(h + 4);
        r->length = get32(h + 8);
        r->entry = get32(h + 12);
        r->image_crc = get32(h + 16);
        if (r->address < AE350_SDRAM_BASE ||
            r->address - AE350_SDRAM_BASE + r->length > MEM_BYTES) {
            say(r, AE350_LOAD_NAK); say(r, 'A');
            r->state = 2;
            return;
        }
        if (r->chatter) {
            for (const char *c = r->chatter; *c; c++) say(r, (uint8_t)*c);
        }
        say(r, AE350_LOAD_ACK);
        r->state = 1;
        r->done = 0;
        memmove(r->in, r->in + 24, r->in_len - 24);
        r->in_len -= 24;
    }
    while (r->state == 1) {
        uint32_t count = r->length - r->done;
        if (count > AE350_LOAD_BLOCK) count = AE350_LOAD_BLOCK;
        if (r->in_len < count + 4) return;
        const int index = r->blocks_seen++;
        uint8_t *data = r->in;
        if (r->corrupt_blocks > 0 &&
            (r->corrupt_only_block < 0 ||
             (uint32_t)r->corrupt_only_block * AE350_LOAD_BLOCK == r->done)) {
            r->corrupt_blocks--;
            data[count / 2] ^= 0x10;
        }
        (void)index;
        if (ae350_crc32(0, data, count) != get32(data + count)) {
            say(r, AE350_LOAD_NAK); say(r, 'C');
            r->naks++;
        } else {
            memcpy(r->mem + (r->address - AE350_SDRAM_BASE) + r->done, data, count);
            r->done += count;
            say(r, AE350_LOAD_ACK);
        }
        memmove(r->in, r->in + count + 4, r->in_len - count - 4);
        r->in_len -= count + 4;
        if (r->done == r->length) {
            const uint32_t crc = ae350_crc32(
                0, r->mem + (r->address - AE350_SDRAM_BASE), r->length);
            if (crc == r->image_crc) {
                say(r, AE350_LOAD_ACK);
            } else {
                say(r, AE350_LOAD_NAK); say(r, 'I');
            }
            r->state = 2;
        }
    }
}

static int rx_write(void *ctx, const uint8_t *data, size_t length)
{
    receiver_t *r = ctx;
    while (length) {
        size_t room = sizeof(r->in) - r->in_len;
        size_t n = length < room ? length : room;
        memcpy(r->in + r->in_len, data, n);
        r->in_len += n;
        data += n;
        length -= n;
        take(r);
    }
    return 0;
}

static int rx_read(void *ctx, uint32_t timeout_ms)
{
    receiver_t *r = ctx;
    (void)timeout_ms;
    if (r->out_head == r->out_tail) return -1;
    return r->out[r->out_head++ % sizeof(r->out)];
}

static void rx_text(void *ctx, uint8_t byte)
{
    (void)byte;
    ((receiver_t *)ctx)->text_bytes++;
}

/* ------------------------------------------------------------ images */

static const uint8_t *s_image;

static int image_bytes(void *ctx, uint32_t offset, uint8_t *dst, uint32_t length)
{
    (void)ctx;
    memcpy(dst, s_image + offset, length);
    return 0;
}

static void reset(receiver_t *r, uint8_t *mem)
{
    memset(r, 0, sizeof(*r));
    r->mem = mem;
    r->corrupt_only_block = -1;
    memset(mem, 0xee, MEM_BYTES);
}

static int send(receiver_t *r, uint32_t address, uint32_t length,
                uint32_t entry, int retries, char *reason)
{
    static uint8_t block[AE350_LOAD_BLOCK];
    const ae350_link_t link = {
        .ctx = r, .write = rx_write, .read_byte = rx_read,
        .text = rx_text, .image = image_bytes,
    };
    return ae350_send_image(&link, address, length, entry, retries, block,
                            reason);
}

/* A file in memory, read through the ELF reader's callback. */
static const uint8_t *s_file;
static size_t s_file_len;

static int file_read(void *ctx, uint32_t offset, uint8_t *dst, uint32_t length)
{
    (void)ctx;
    if (offset > s_file_len || length > s_file_len - offset) return -1;
    memcpy(dst, s_file + offset, length);
    return 0;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}

int main(void)
{
    static receiver_t rx;
    uint8_t *mem = malloc(MEM_BYTES);
    const uint32_t length = 3 * AE350_LOAD_BLOCK + 1234;   /* a short last block */
    uint8_t *image = malloc(length);
    for (uint32_t i = 0; i < length; i++) image[i] = (uint8_t)(i * 7 + (i >> 9));
    s_image = image;
    const uint32_t base = AE350_SDRAM_BASE + 0x1000;
    char reason;

    printf("1. a clean load lands every byte\n");
    reset(&rx, mem);
    check_int("result", send(&rx, base, length, base, 5, &reason), AE350_OK);
    check_int("bytes", memcmp(mem + 0x1000, image, length), 0);
    check_int("no NAKs", rx.naks, 0);
    check_int("blocks", rx.blocks_seen, 4);

    printf("2. a corrupted block is NAKed and sent again\n");
    reset(&rx, mem);
    rx.corrupt_blocks = 2;
    rx.corrupt_only_block = 1;
    check_int("result", send(&rx, base, length, base, 5, &reason), AE350_OK);
    check_int("bytes", memcmp(mem + 0x1000, image, length), 0);
    check_int("NAKs", rx.naks, 2);
    check_int("blocks with resends", rx.blocks_seen, 6);

    printf("3. more corrupt copies than retries fails with the reason\n");
    reset(&rx, mem);
    rx.corrupt_blocks = 3;
    rx.corrupt_only_block = 2;
    check_int("result", send(&rx, base, length, base, 2, &reason), AE350_ERR_NAK);
    check_int("reason", reason, 'C');

    printf("4. a receiver that never answers times out\n");
    reset(&rx, mem);
    rx.silent = 1;
    check_int("result", send(&rx, base, length, base, 5, &reason), AE350_ERR_TIMEOUT);

    printf("5. text before a reply goes to the text callback\n");
    reset(&rx, mem);
    rx.chatter = "TinyNES LOAD\r\n";
    check_int("result", send(&rx, base, length, base, 5, &reason), AE350_OK);
    check_int("text bytes", rx.text_bytes, 14);

    printf("6. the range is checked before anything is sent\n");
    reset(&rx, mem);
    check_int("below the SDRAM", send(&rx, 0x3ffff000u, 16, 0x3ffff000u, 5, &reason), AE350_ERR_RANGE);
    check_int("past its end", send(&rx, 0x41fffff0u, 32, 0x41fffff0u, 5, &reason), AE350_ERR_RANGE);
    check_int("entry outside", send(&rx, base, 64, base + 64, 5, &reason), AE350_ERR_RANGE);
    check_int("nothing written", (long)rx.in_len, 0);

    printf("7. ELF: two segments with a gap become one zero-filled image\n");
    {
        static uint8_t file[4096];
        memset(file, 0, sizeof(file));
        memcpy(file, "\x7f" "ELF\x01\x01\x01", 7);
        put16(file + 18, 243);                 /* EM_RISCV */
        put32(file + 24, 0x40000100u);         /* entry */
        put32(file + 28, 52);                  /* phoff */
        put16(file + 42, 32);                  /* phentsize */
        put16(file + 44, 3);                   /* phnum */
        uint8_t *p = file + 52;
        put32(p, 1); put32(p + 4, 1024); put32(p + 12, 0x40000000u); put32(p + 16, 512);
        p += 32;
        put32(p, 4);                           /* PT_NOTE: skipped */
        p += 32;
        put32(p, 1); put32(p + 4, 2048); put32(p + 12, 0x40000400u); put32(p + 16, 256);
        for (int i = 0; i < 512; i++) file[1024 + i] = (uint8_t)(0xa0 + i);
        for (int i = 0; i < 256; i++) file[2048 + i] = (uint8_t)(0x30 + i);
        s_file = file;
        s_file_len = sizeof(file);
        ae350_elf_t elf;
        check_int("parse", ae350_elf_parse(&elf, file_read, NULL), AE350_OK);
        check_int("segments", elf.count, 2);
        check_int("base", (long)elf.base, 0x40000000L);
        check_int("end", (long)elf.end, 0x40000500L);
        check_int("entry", (long)elf.entry, 0x40000100L);
        uint8_t out[0x500];
        /* Read it in odd pieces, as blocks would cut it. */
        int ok = 1;
        for (uint32_t off = 0; off < sizeof(out); off += 300) {
            uint32_t n = sizeof(out) - off < 300 ? sizeof(out) - off : 300;
            ok &= ae350_elf_image(&elf, file_read, NULL, off, out + off, n) == AE350_OK;
        }
        check_int("image read", ok, 1);
        int good = 1;
        for (int i = 0; i < 512; i++) good &= out[i] == (uint8_t)(0xa0 + i);
        for (int i = 512; i < 1024; i++) good &= out[i] == 0;
        for (int i = 0; i < 256; i++) good &= out[1024 + i] == (uint8_t)(0x30 + i);
        check_int("image bytes", good, 1);

        put32(file + 52 + 12, 0x30000000u);    /* first segment below the SDRAM */
        check_int("outside the SDRAM", ae350_elf_parse(&elf, file_read, NULL), AE350_ERR_RANGE);
        file[0] = 0;
        check_int("not an ELF", ae350_elf_parse(&elf, file_read, NULL), AE350_ERR_IMAGE);
    }

    free(image);
    free(mem);
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "PASS", checks, fails);
    return fails ? 1 : 0;
}
