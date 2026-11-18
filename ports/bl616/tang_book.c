/* TinyTang — TinyReader's books (tang_book.h).
 *
 * SPDX-License-Identifier: MIT
 */
#include "tang_book.h"

#include <string.h>

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int tang_book_parse(const uint8_t *h, size_t n, uint64_t file_bytes, tang_book_t *out)
{
    if (h == NULL || n < TANG_BOOK_HEADER_BYTES) return TANG_BOOK_SHORT;
    if (memcmp(h, "TRB1", 4) != 0) return TANG_BOOK_MAGIC;
    if (le16(h + 4) != 1u) return TANG_BOOK_VERSION;
    if (le16(h + 12) != TANG_BOOK_FORMAT_RGB565_BE) return TANG_BOOK_FORMAT;

    tang_book_t b;
    memset(&b, 0, sizeof b);
    const uint16_t header_bytes = le16(h + 6);
    b.width = le16(h + 8);
    b.height = le16(h + 10);
    b.count = le16(h + 14);
    b.data_offset = le32(h + 16);
    b.spread_bytes = le32(h + 20);
    b.pages = le16(h + 24);
    b.flags = le16(h + 26);
    memcpy(b.title, h + 32, 64);
    b.title[64] = '\0';

    if (header_bytes != TANG_BOOK_HEADER_BYTES || b.width == 0 || b.height == 0 ||
        b.count == 0 || b.spread_bytes != (uint32_t)b.width * b.height * 2u ||
        b.data_offset < TANG_BOOK_HEADER_BYTES + 8u * b.count || b.data_offset % 512u != 0) {
        return TANG_BOOK_GEOMETRY;
    }
    if ((uint64_t)b.data_offset + (uint64_t)b.count * b.spread_bytes > file_bytes) {
        return TANG_BOOK_TRUNCATED;
    }
    *out = b;
    return TANG_BOOK_OK;
}

uint64_t tang_book_offset(const tang_book_t *book, int spread)
{
    return (uint64_t)book->data_offset + (uint64_t)spread * book->spread_bytes;
}

const char *tang_book_error(int rc)
{
    switch (rc) {
    case TANG_BOOK_OK: return "ok";
    case TANG_BOOK_SHORT: return "too short to be a book";
    case TANG_BOOK_MAGIC: return "not a TinyReader book";
    case TANG_BOOK_VERSION: return "a book version this firmware cannot read";
    case TANG_BOOK_FORMAT: return "a pixel format this firmware cannot show";
    case TANG_BOOK_GEOMETRY: return "a damaged book header";
    case TANG_BOOK_TRUNCATED: return "the book file is cut short";
    default: return "unreadable";
    }
}

void tang_book_cache_init(tang_book_cache_t *c, int first_buffer, int n)
{
    if (n > TANG_BOOK_SLOTS_MAX) n = TANG_BOOK_SLOTS_MAX;
    if (n < 0) n = 0;
    memset(c, 0, sizeof *c);
    c->n = n;
    for (int i = 0; i < n; i++) {
        c->buffer[i] = first_buffer + i;
        c->spread[i] = -1;
    }
}

int tang_book_cache_find(const tang_book_cache_t *c, int s)
{
    if (s < 0) return -1;
    for (int i = 0; i < c->n; i++) {
        if (c->spread[i] == s) return i;
    }
    return -1;
}

void tang_book_cache_touch(tang_book_cache_t *c, int slot)
{
    if (slot >= 0 && slot < c->n) c->used[slot] = ++c->clock;
}

void tang_book_cache_set(tang_book_cache_t *c, int slot, int s)
{
    if (slot >= 0 && slot < c->n) c->spread[slot] = s;
}

/* Offsets from cur in window order: 0, +1, -1, +2, +3, -2, +4, +5, -3, ... */
static int window_offset(int j)
{
    if (j == 0) return 0;
    if (j == 1) return 1;
    if (j == 2) return -1;
    const int group = (j - 3) / 3, pos = (j - 3) % 3;
    return pos == 2 ? -(group + 2) : 2 * group + 2 + pos;
}

int tang_book_window(int cur, int count, int n, int k)
{
    if (count <= 0 || n <= 0 || k < 0 || k >= n) return -1;
    int found = 0;
    /* Every offset within the book appears once; offsets outside are skipped
     * and do not use up a place.  Twice the book's length always suffices. */
    for (int j = 0; j < 3 * count + 3; j++) {
        const int s = cur + window_offset(j);
        if (s < 0 || s >= count) continue;
        if (found == k) return s;
        found++;
    }
    return -1;
}

bool tang_book_in_window(int cur, int count, int n, int s)
{
    for (int k = 0; k < n; k++) {
        const int w = tang_book_window(cur, count, n, k);
        if (w < 0) return false;
        if (w == s) return true;
    }
    return false;
}

int tang_book_next_wanted(const tang_book_cache_t *c, int cur, int count)
{
    for (int k = 0; k < c->n; k++) {
        const int s = tang_book_window(cur, count, c->n, k);
        if (s < 0) return -1;
        if (tang_book_cache_find(c, s) < 0) return s;
    }
    return -1;
}

int tang_book_cache_victim(const tang_book_cache_t *c, int cur, int count, int shown)
{
    int best = -1;
    for (int i = 0; i < c->n; i++) {
        if (i == shown) continue;
        if (c->spread[i] < 0) return i;
        if (tang_book_in_window(cur, count, c->n, c->spread[i])) continue;
        if (best < 0 || c->used[i] < c->used[best]) best = i;
    }
    return best;
}
