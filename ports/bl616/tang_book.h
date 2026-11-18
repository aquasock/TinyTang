/* TinyTang — TinyReader's books: the .trb file and which spreads to keep.
 *
 * A book is made on the PC by tools/make_book.py, whose header documents the
 * format: a 128-byte little-endian header, a spread table, then every
 * two-page spread already rendered at the plane's size in big-endian RGB565,
 * each a whole number of sectors from a 4096-byte boundary.  The board copies
 * a spread's bytes straight into one of the desktop core's SDRAM buffers
 * (desktop ABI 1.11) and turns a page by showing another buffer.
 *
 * This file is the part with no hardware in it, so it runs on a host: the
 * header check, and the cache of spreads in buffers -- which spread to load
 * next around the one being read, and which buffer to give up for it.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef TANG_BOOK_H
#define TANG_BOOK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TANG_BOOK_HEADER_BYTES 128u
#define TANG_BOOK_FORMAT_RGB565_BE 1u

typedef struct {
    uint16_t width, height;
    uint16_t count;             /* spreads */
    uint16_t pages;             /* source pages */
    uint16_t flags;
    uint32_t data_offset;       /* spread 0's first byte */
    uint32_t spread_bytes;
    char title[65];
} tang_book_t;

/* Why a header was refused. */
enum {
    TANG_BOOK_OK = 0,
    TANG_BOOK_SHORT = -1,       /* fewer than 128 bytes */
    TANG_BOOK_MAGIC = -2,       /* not "TRB1" */
    TANG_BOOK_VERSION = -3,     /* a version this firmware does not read */
    TANG_BOOK_FORMAT = -4,      /* a pixel format other than RGB565, big-endian */
    TANG_BOOK_GEOMETRY = -5,    /* sizes that do not add up */
    TANG_BOOK_TRUNCATED = -6,   /* the file ends before its last spread */
};

/* Check a header against the file's size and fill `out`.  The plane's size
 * is the caller's to compare with width and height. */
int tang_book_parse(const uint8_t *header, size_t n, uint64_t file_bytes, tang_book_t *out);

/* The file offset of spread i. */
uint64_t tang_book_offset(const tang_book_t *book, int spread);

/* A short reason for a refused header, for the user. */
const char *tang_book_error(int rc);

/* -------------------------------------------------------------- the cache
 *
 * Slots are plane buffers; each holds one spread, or none.  The spreads worth
 * holding around the one being read, cur, come in this order -- ahead twice
 * for every step back, since a reader mostly goes forward:
 *
 *     cur, cur+1, cur-1, cur+2, cur+3, cur-2, cur+4, cur+5, cur-3, ...
 *
 * skipping any outside the book, and as many as there are slots: that is the
 * window.  Loading goes in that order, so the next page is always first. */

#define TANG_BOOK_SLOTS_MAX 12

typedef struct {
    int n;                                  /* slots in use */
    int buffer[TANG_BOOK_SLOTS_MAX];        /* the plane buffer of each slot */
    int spread[TANG_BOOK_SLOTS_MAX];        /* what it holds, -1 for nothing */
    uint32_t used[TANG_BOOK_SLOTS_MAX];     /* when it was last shown */
    uint32_t clock;
} tang_book_cache_t;

/* n slots on plane buffers first .. first + n - 1, all empty. */
void tang_book_cache_init(tang_book_cache_t *c, int first_buffer, int n);

/* The slot holding spread s, or -1. */
int tang_book_cache_find(const tang_book_cache_t *c, int s);

/* Mark a slot as shown now, for least-recently-used order. */
void tang_book_cache_touch(tang_book_cache_t *c, int slot);

/* Record what a slot holds: a spread, or -1 while it is being overwritten. */
void tang_book_cache_set(tang_book_cache_t *c, int slot, int s);

/* The k-th spread of the window around cur (k from 0), or -1 past its end. */
int tang_book_window(int cur, int count, int n, int k);

/* Whether spread s is in the window around cur. */
bool tang_book_in_window(int cur, int count, int n, int s);

/* The first spread of the window not yet held, or -1 when all are. */
int tang_book_next_wanted(const tang_book_cache_t *c, int cur, int count);

/* The slot to load into: an empty one first, else the least recently used
 * whose spread is outside the window; never `shown`.  -1 when every slot
 * holds a spread of the window. */
int tang_book_cache_victim(const tang_book_cache_t *c, int cur, int count, int shown);

#ifdef __cplusplus
}
#endif

#endif /* TANG_BOOK_H */
