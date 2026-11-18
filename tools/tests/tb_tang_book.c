/* Host test: the firmware's side of a TinyReader book (ports/bl616/tang_book.c)
 * against a file tools/make_book.py made, and the spread cache's planning.
 *
 *   tb_tang_book <book.trb> <spreads> <width> <height> <zoom views>
 *
 * SPDX-License-Identifier: MIT
 */
#include "tang_book.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(int argc, char **argv)
{
    if (argc != 6) {
        fprintf(stderr, "usage: tb_tang_book <book.trb> <spreads> <width> <height> <zoom views>\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t h[128];
    if (fread(h, 1, sizeof h, f) != sizeof h) { fprintf(stderr, "short\n"); return 2; }
    static uint8_t ztable[8 * 1024];
    const int zwant = atoi(argv[5]);
    if (zwant > 1024) return 2;
    fseek(f, 128 + 8L * atoi(argv[2]), SEEK_SET);
    if (zwant && fread(ztable, 8, (size_t)zwant, f) != (size_t)zwant) return 2;
    fclose(f);

    /* ------------------------------------------------ the converter's header */
    tang_book_t b;
    CHECK(tang_book_parse(h, sizeof h, (uint64_t)size, &b) == TANG_BOOK_OK);
    CHECK(b.count == atoi(argv[2]));
    CHECK(b.width == atoi(argv[3]) && b.height == atoi(argv[4]));
    CHECK(b.spread_bytes == (uint32_t)b.width * b.height * 2u);
    CHECK(b.data_offset % 4096u == 0);
    CHECK(b.zoom_count == zwant);
    if (zwant) {
        CHECK(b.zoom_offset % 4096u == 0);
        CHECK(b.zoom_offset >= tang_book_offset(&b, b.count - 1) + b.spread_bytes);
        CHECK(tang_book_zoom_offset(&b, b.zoom_count - 1) + b.spread_bytes == (uint64_t)size);
        CHECK(tang_book_zoom_table(&b) == 128u + 8u * b.count);
    } else {
        CHECK(b.zoom_offset == 0);
        CHECK(tang_book_offset(&b, b.count - 1) + b.spread_bytes == (uint64_t)size);
    }
    CHECK(strcmp(b.title, "Test Book") == 0);

    /* The zoom table, for the five-page test book: page 1 in spread 0, pages
     * 2 and 3 in spread 1, 4 and 5 in spread 2, two views a page. */
    if (zwant == 10) {
        const int spread_of[10] = { 0, 0, 1, 1, 1, 1, 2, 2, 2, 2 };
        for (int i = 0; i < 10; i++) CHECK(tang_book_zoom_spread(ztable, 10, i) == spread_of[i]);
        CHECK(tang_book_zoom_first(ztable, 10, 0) == 0);
        CHECK(tang_book_zoom_first(ztable, 10, 1) == 2);
        CHECK(tang_book_zoom_first(ztable, 10, 2) == 6);
        CHECK(tang_book_zoom_first(ztable, 10, 9) == 6);     /* past the end: the last spread's */
        CHECK(tang_book_zoom_spread(ztable, 10, 99) == 0);
    }

    /* Refusals. */
    CHECK(tang_book_parse(h, 127, (uint64_t)size, &b) == TANG_BOOK_SHORT);
    CHECK(tang_book_parse(h, sizeof h, (uint64_t)size - 1, &b) == TANG_BOOK_TRUNCATED);
    {
        uint8_t bad[128];
        memcpy(bad, h, sizeof bad); bad[0] = 'X';
        CHECK(tang_book_parse(bad, sizeof bad, (uint64_t)size, &b) == TANG_BOOK_MAGIC);
        memcpy(bad, h, sizeof bad); bad[4] = 2;
        CHECK(tang_book_parse(bad, sizeof bad, (uint64_t)size, &b) == TANG_BOOK_VERSION);
        memcpy(bad, h, sizeof bad); bad[12] = 2;
        CHECK(tang_book_parse(bad, sizeof bad, (uint64_t)size, &b) == TANG_BOOK_FORMAT);
        memcpy(bad, h, sizeof bad); bad[20] ^= 2;            /* spread bytes off */
        CHECK(tang_book_parse(bad, sizeof bad, (uint64_t)size, &b) == TANG_BOOK_GEOMETRY);
        memcpy(bad, h, sizeof bad); bad[16] = 1;              /* data offset not sector-aligned */
        CHECK(tang_book_parse(bad, sizeof bad, (uint64_t)size, &b) == TANG_BOOK_GEOMETRY);
        if (zwant) {
            memcpy(bad, h, sizeof bad); bad[96] = 1;          /* zoom offset not aligned */
            CHECK(tang_book_parse(bad, sizeof bad, (uint64_t)size, &b) == TANG_BOOK_GEOMETRY);
            memcpy(bad, h, sizeof bad); bad[100] += 1;        /* one view more than the file holds */
            CHECK(tang_book_parse(bad, sizeof bad, (uint64_t)size, &b) != TANG_BOOK_OK);
            /* Without its zoom fields the book still reads, as spreads alone. */
            memcpy(bad, h, sizeof bad); memset(bad + 96, 0, 8);
            CHECK(tang_book_parse(bad, sizeof bad, (uint64_t)size, &b) == TANG_BOOK_OK &&
                  b.zoom_count == 0 && b.zoom_offset == 0);
        }
    }

    /* ------------------------------------------------------------ the window */
    {
        const int want[11] = { 50, 51, 49, 52, 53, 48, 54, 55, 47, 56, 57 };
        for (int k = 0; k < 11; k++) CHECK(tang_book_window(50, 100, 11, k) == want[k]);
        CHECK(tang_book_window(50, 100, 11, 11) == -1);
        /* At the cover the window is all ahead; at the end, all behind. */
        for (int k = 0; k < 11; k++) CHECK(tang_book_window(0, 100, 11, k) == k);
        const int tail[5] = { 99, 98, 97, 96, 95 };
        for (int k = 0; k < 5; k++) CHECK(tang_book_window(99, 100, 5, k) == tail[k]);
        /* A book shorter than the cache: every spread, once. */
        int seen = 0;
        for (int k = 0; k < 11; k++) {
            const int s = tang_book_window(1, 3, 11, k);
            if (s >= 0) { CHECK(s < 3); seen |= 1 << s; }
        }
        CHECK(seen == 7);
        CHECK(tang_book_window(1, 3, 11, 3) == -1);
        CHECK(tang_book_in_window(50, 100, 11, 47) && !tang_book_in_window(50, 100, 11, 46));
        CHECK(!tang_book_in_window(50, 100, 11, 58));
    }

    /* ------------------------------------------------------------- the cache */
    {
        tang_book_cache_t c;
        tang_book_cache_init(&c, 1, 11);
        CHECK(c.n == 11 && c.buffer[0] == 1 && c.buffer[10] == 11);
        CHECK(tang_book_cache_find(&c, 0) < 0);

        /* Fill the window around 10 in its order, never touching the shown slot. */
        int shown = -1, order[11], loads = 0, s;
        while ((s = tang_book_next_wanted(&c, 10, 100)) >= 0 && loads < 20) {
            const int v = tang_book_cache_victim(&c, 10, 100, shown);
            CHECK(v >= 0 && v != shown);
            tang_book_cache_set(&c, v, s);
            if (s == 10) { shown = v; tang_book_cache_touch(&c, v); }
            order[loads++] = s;
        }
        CHECK(loads == 11);
        CHECK(order[0] == 10 && order[1] == 11 && order[2] == 9 && order[3] == 12);
        for (int k = 0; k < 11; k++) CHECK(tang_book_cache_find(&c, tang_book_window(10, 100, 11, k)) >= 0);
        /* Full: nothing wanted, nothing to give up. */
        CHECK(tang_book_next_wanted(&c, 10, 100) == -1);
        CHECK(tang_book_cache_victim(&c, 10, 100, shown) == -1);

        /* Turn forward: 11 is already held, and the one spread the new window
         * lacks takes the slot of the one that fell out of it (7). */
        int slot = tang_book_cache_find(&c, 11);
        CHECK(slot >= 0);
        shown = slot; tang_book_cache_touch(&c, slot);
        s = tang_book_next_wanted(&c, 11, 100);
        CHECK(s == 18);
        const int v = tang_book_cache_victim(&c, 11, 100, shown);
        CHECK(v >= 0 && c.spread[v] == 7);

        /* Jump far: every slot is outside the new window; the shown one is
         * kept, and the least recently shown goes first. */
        tang_book_cache_t d;
        tang_book_cache_init(&d, 1, 3);
        for (int i = 0; i < 3; i++) { tang_book_cache_set(&d, i, i); tang_book_cache_touch(&d, i); }
        tang_book_cache_touch(&d, 0);         /* 0 recent, 1 oldest, 2 next */
        CHECK(tang_book_cache_victim(&d, 80, 100, 2) == 1);
        CHECK(tang_book_cache_victim(&d, 80, 100, 1) == 2);
        /* A slot being overwritten (-1) is empty, and goes first. */
        tang_book_cache_set(&d, 0, -1);
        CHECK(tang_book_cache_victim(&d, 80, 100, 2) == 0);
    }

    /* ------------------------------------------- two sequences, one cache */
    {
        tang_book_cache_t c;
        tang_book_cache_init(&c, 1, 3);
        for (int i = 0; i < 3; i++) { tang_book_cache_set(&c, i, i); tang_book_cache_touch(&c, i); }
        CHECK(tang_book_cache_find(&c, 1) == 1);
        /* Zoom views are numbered from 1000: spread 1 is not view 1. */
        tang_book_cache_rebase(&c, 1000);
        CHECK(tang_book_cache_find(&c, 1) == -1);
        CHECK(tang_book_next_wanted(&c, 0, 50) == 0);
        /* The spreads are given up for the views, though spreads 0-2 would
         * be inside the views' window if their numbers were taken as views. */
        CHECK(tang_book_cache_victim(&c, 0, 50, 2) == 0);
        tang_book_cache_set(&c, 0, 0);                         /* view 0 into slot 0 */
        CHECK(tang_book_cache_find(&c, 0) == 0 && c.spread[0] == 1000);
        /* Back to spreads: 1 and 2 are still held, 0 is gone. */
        tang_book_cache_rebase(&c, 0);
        CHECK(tang_book_cache_find(&c, 0) == -1);
        CHECK(tang_book_cache_find(&c, 1) == 1 && tang_book_cache_find(&c, 2) == 2);
        tang_book_cache_set(&c, 0, -1);
        CHECK(c.spread[0] == -1);
    }

    if (failures) {
        printf("tb_tang_book: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_tang_book: all checks passed\n");
    return 0;
}
