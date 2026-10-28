// SPDX-License-Identifier: MIT
// Pack plumbing shared by the two readers: what is on the card, what a pack
// says about itself, and how to find a title in its index.
//
// No state of its own: every call takes the caller's FatFS handle, so TinyWiki
// and TinyGutenberg can both have a pack open at once without sharing a file
// object. The format itself is in td_wiki_pack.h.
#ifndef TD_PACK_H
#define TD_PACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ff.h"
#include "td_wiki_pack.h"

#define TD_PACK_ROOT "/sd/root/wiki"          /* encyclopedias and the manual */
#define TD_PACK_BOOKS_ROOT "/sd/root/books"   /* book shelves, for TinyGutenberg */
#define TD_PACK_MAX 16
#define TD_PACK_TITLE_BYTES 44
#define TD_PACK_LICENSE_BYTES 72
#define TD_PACK_NAME_BYTES 32


typedef struct
{
    char title[TD_PACK_TITLE_BYTES];
    char license[TD_PACK_LICENSE_BYTES];
    char main_page[TD_PACK_TITLE_BYTES];
    uint32_t articles;
    uint32_t chunks;
    uint32_t parts;
    uint32_t authors;
    uint16_t title_bytes;
    uint16_t record_bytes;
} td_pack_meta_t;

typedef struct
{
    char folder[TD_PACK_NAME_BYTES];   // "" for a pack sitting at /root/wiki
    char title[TD_PACK_TITLE_BYTES];
    uint32_t articles;
} td_pack_entry_t;

/* A meta filled with the defaults a pack without a readable wiki.meta gets. */
void td_pack_defaults(td_pack_meta_t *meta);

/* Read dir/wiki.meta. False when the file is missing or unreadable. */
bool td_pack_meta_read(const char *dir, td_pack_meta_t *meta);

/* The directory a pack lives in, given its root and folder ("" = the pack at
 * the root itself). */
void td_pack_dir_of(const char *root, const char *folder, char *out, size_t cap);

/* Every pack under root -- the one at the root itself first, then each folder
 * holding a wiki.meta, sorted by folder.  Answers how many were found.  Each
 * reader has its own root: /root/wiki for encyclopedias, /root/books for book
 * shelves, so neither app lists the other's packs. */
int td_pack_scan(const char *root, td_pack_entry_t *out, int max);

/* Read one fixed-size index record. */
bool td_pack_record(FIL *index, const td_pack_meta_t *meta, uint32_t number,
                    wiki_record_t *record);

/* The first record whose title is not less than prefix, in the index's folded
 * order. */
uint32_t td_pack_lower_bound(FIL *index, const td_pack_meta_t *meta,
                             const char *prefix);

#endif // TD_PACK_H
