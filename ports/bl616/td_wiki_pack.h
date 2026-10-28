// SPDX-License-Identifier: MIT
// TinyWiki pack format: constants and the pure parts of reading wiki.idx.
//
// tools/make_wiki_pack.py writes the pack; this header is the firmware's side
// of that contract and is kept free of FatFS so the host test can check it.
//
// wiki.meta   `key = value` lines (title, language, license, articles, ...)
// wiki.idx    wiki.idx record per article, sorted by case-folded title
// wiki.NNN    article bodies, chunked below the FAT32 4 GiB file limit
//
// One record, little-endian: title[title_bytes] NUL-padded, then
// u32 chunk, u32 offset, u32 length, u32 lines.
#ifndef TD_WIKI_PACK_H
#define TD_WIKI_PACK_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define WIKI_FORMAT_VERSION 1
#define WIKI_TITLE_BYTES_DEFAULT 64
#define WIKI_RECORD_OVERHEAD 16
#define WIKI_RECORD_BYTES(title_bytes) ((title_bytes) + WIKI_RECORD_OVERHEAD)
#define WIKI_MAX_TITLE_BYTES 255
#define WIKI_PACK_NAME_BYTES 32

typedef struct
{
    char title[WIKI_MAX_TITLE_BYTES + 1];
    uint32_t chunk;     // which wiki.NNN
    uint32_t offset;    // byte offset of the body inside that chunk
    uint32_t length;    // body length in bytes
    uint32_t lines;     // display rows the body occupies
} wiki_record_t;

/* Fold one printable ASCII character for ordering; the pack's index is sorted
 * by this, and the reader compares titles the same way. */
static inline int wiki_fold_char(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
}

/* Compare two titles in the index's own order. */
static inline int wiki_title_cmp(const char *a, const char *b)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    while (*pa != '\0' && *pb != '\0')
    {
        int d = wiki_fold_char(*pa) - wiki_fold_char(*pb);
        if (d != 0)
        {
            return d < 0 ? -1 : 1;
        }
        ++pa;
        ++pb;
    }
    int d = wiki_fold_char(*pa) - wiki_fold_char(*pb);
    return d == 0 ? 0 : (d < 0 ? -1 : 1);
}

static inline uint32_t wiki_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* Decode the fixed-size record starting at raw. The title is copied out and
 * NUL-terminated, cut to the pack's title width. */
static inline bool wiki_record_parse(const uint8_t *raw, uint16_t title_bytes,
                                     wiki_record_t *out)
{
    if (raw == NULL || out == NULL || title_bytes == 0 ||
        title_bytes > WIKI_MAX_TITLE_BYTES)
    {
        return false;
    }
    uint16_t copy = title_bytes;
    memcpy(out->title, raw, copy);
    out->title[copy] = '\0';
    out->chunk = wiki_le32(raw + title_bytes);
    out->offset = wiki_le32(raw + title_bytes + 4);
    out->length = wiki_le32(raw + title_bytes + 8);
    out->lines = wiki_le32(raw + title_bytes + 12);
    return out->title[0] != '\0';
}

/* True when a directory name under /root/wiki may be a pack: a pack is a folder
 * holding wiki.meta, but hidden and pseudo entries are never one, and a name
 * too long to store would be truncated into a different folder's name. */
static inline bool wiki_pack_dir_ok(const char *name)
{
    size_t length;
    if (name == NULL || name[0] == '\0' || name[0] == '.')
    {
        return false;
    }
    length = strlen(name);
    return length > 0 && length < WIKI_PACK_NAME_BYTES;
}

/* True when the record's chunk number is one the pack can name (wiki.NNN). */
static inline bool wiki_chunk_valid(uint32_t chunk, uint32_t chunk_count)
{
    return chunk < chunk_count;
}

#endif // TD_WIKI_PACK_H
