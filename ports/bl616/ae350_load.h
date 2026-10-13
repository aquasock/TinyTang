// TinyTang — sending programs and files to a TinyNES core's AE350 boot ROM.
//
// TinyNES's boot ROM (TinyNES firmware/boot/load.c, protocol in
// firmware/include/tinynes.h) takes a program over the core's UART: a 24-byte
// header (magic "TNL1", load address, length, entry, the image's CRC-32 and
// the header's own CRC-32, little-endian), then the image in 8 KiB blocks,
// each followed by its CRC-32. It answers the header and every block with ACK
// (0x06), or NAK (0x15) and a reason letter, and a NAKed block is sent again;
// after the last block it reads the image back from the SDRAM and answers
// once more. A running program can take a second image the same way.
//
// Nothing here touches a UART, a file or a task: the caller supplies the
// byte transport and the image's bytes, so the module runs on a host
// (tools/tests/test_ae350_load.sh).

#ifndef AE350_LOAD_H
#define AE350_LOAD_H

#include <stddef.h>
#include <stdint.h>

#define AE350_LOAD_MAGIC       0x314c4e54u   /* "TNL1" */
#define AE350_LOAD_BLOCK       8192u
#define AE350_LOAD_ACK         0x06u
#define AE350_LOAD_NAK         0x15u
#define AE350_SDRAM_BASE       0x40000000u
#define AE350_SDRAM_END        0x42000000u
/* The payload a program asks for with "TinyNES WANT": this magic, the
 * file's length and the file. */
#define AE350_FILE_MAGIC       0x31524e54u   /* "TNR1" */
#define AE350_FILE_HEADER      8u

typedef struct {
    void *ctx;
    /* Send bytes; 0 on success. */
    int (*write)(void *ctx, const uint8_t *data, size_t length);
    /* The next received byte, or -1 after timeout_ms with none. */
    int (*read_byte)(void *ctx, uint32_t timeout_ms);
    /* Bytes the board sent between replies (its text), or NULL. */
    void (*text)(void *ctx, uint8_t byte);
    /* The image's bytes [offset, offset + length); 0 on success. */
    int (*image)(void *ctx, uint32_t offset, uint8_t *dst, uint32_t length);
} ae350_link_t;

enum {
    AE350_OK = 0,
    AE350_ERR_TIMEOUT = -1,    /* no reply in time */
    AE350_ERR_NAK = -2,        /* refused, or refused too often */
    AE350_ERR_WRITE = -3,      /* the transport could not send */
    AE350_ERR_IMAGE = -4,      /* the image's bytes could not be read */
    AE350_ERR_RANGE = -5,      /* the image is not inside the SDRAM */
};

/* Send one image of `length` bytes to load at `address` and start at
 * `entry`, resending a NAKed block up to `retries` times. On a NAK the reason
 * letter is left in *reason (or 0). Uses `block` (AE350_LOAD_BLOCK bytes) as
 * scratch. */
int ae350_send_image(const ae350_link_t *link, uint32_t address,
                     uint32_t length, uint32_t entry, int retries,
                     uint8_t *block, char *reason);

/* IEEE CRC-32 (zlib's), continued from `crc` (0 to start). */
uint32_t ae350_crc32(uint32_t crc, const uint8_t *data, size_t length);

/* ----------------------------------------------------------------- ELF */

#define AE350_ELF_MAX_SEGMENTS 8

typedef struct {
    uint32_t address;
    uint32_t offset;        /* in the file */
    uint32_t size;          /* bytes in the file */
} ae350_segment_t;

typedef struct {
    uint32_t base, end, entry;
    unsigned count;
    ae350_segment_t segment[AE350_ELF_MAX_SEGMENTS];
} ae350_elf_t;

/* Read a 32-bit little-endian RISC-V ELF's loadable segments with `read`
 * (bytes [offset, offset + length) of the file; 0 on success). The image is
 * base to end, the gaps between segments zero. 0, or an AE350_ERR_ code. */
int ae350_elf_parse(ae350_elf_t *elf,
                    int (*read)(void *ctx, uint32_t offset, uint8_t *dst,
                                uint32_t length),
                    void *ctx);

/* The image's bytes [offset, offset + length), read from the file with
 * `read`; for ae350_link_t.image. */
int ae350_elf_image(const ae350_elf_t *elf,
                    int (*read)(void *ctx, uint32_t offset, uint8_t *dst,
                                uint32_t length),
                    void *ctx, uint32_t offset, uint8_t *dst, uint32_t length);

#endif
