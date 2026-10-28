// SPDX-License-Identifier: MIT
// Host checks for the TinyWiki pack contract (ports/bl616/td_wiki_pack.h).
//
// The pack is written by tools/make_wiki_pack.py and read on the board, so the
// record layout and the title order have to agree on both sides; the converter
// test (tools/tests/check_wiki_convert.py) checks the writer, and this checks
// the reader's half against bytes it builds by hand.
#include <stdio.h>
#include <string.h>

#include "td_wiki_pack.h"

static int s_checks;
static int s_failed;

static void check(bool ok, const char *what)
{
    ++s_checks;
    if (!ok)
    {
        ++s_failed;
        fprintf(stderr, "  FAIL %s\n", what);
    }
}

static void put_le32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)(value & 0xff);
    at[1] = (uint8_t)((value >> 8) & 0xff);
    at[2] = (uint8_t)((value >> 16) & 0xff);
    at[3] = (uint8_t)((value >> 24) & 0xff);
}

int main(void)
{
    check(WIKI_RECORD_BYTES(64) == 80, "a 64-byte title makes an 80-byte record");
    check(WIKI_RECORD_BYTES(40) == 56, "a 40-byte title makes a 56-byte record");

    uint8_t raw[WIKI_RECORD_BYTES(64)];
    memset(raw, 0xa5, sizeof raw);
    memcpy(raw, "Albert Einstein", 15);
    memset(raw + 15, 0, 64 - 15);
    put_le32(raw + 64, 3);
    put_le32(raw + 68, 123456);
    put_le32(raw + 72, 4096);
    put_le32(raw + 76, 57);

    wiki_record_t record;
    check(wiki_record_parse(raw, 64, &record), "a well-formed record parses");
    check(strcmp(record.title, "Albert Einstein") == 0, "the title is copied out");
    check(record.chunk == 3, "the chunk decodes little-endian");
    check(record.offset == 123456u, "the offset decodes little-endian");
    check(record.length == 4096u, "the length decodes little-endian");
    check(record.lines == 57u, "the line count decodes little-endian");

    // A record whose title fills its width exactly must still be terminated.
    memset(raw, 'x', 64);
    put_le32(raw + 64, 0);
    put_le32(raw + 68, 0);
    put_le32(raw + 72, 1);
    put_le32(raw + 76, 1);
    check(wiki_record_parse(raw, 64, &record), "a full-width title parses");
    check(strlen(record.title) == 64 && record.title[64] == '\0',
          "a full-width title is NUL-terminated at 64");

    // A 40-byte pack reads the same record with a shorter title.
    memset(raw, 0, sizeof raw);
    memcpy(raw, "Short", 5);
    put_le32(raw + 40, 1);
    put_le32(raw + 44, 2);
    put_le32(raw + 48, 3);
    put_le32(raw + 52, 4);
    check(wiki_record_parse(raw, 40, &record), "a 40-byte record parses");
    check(strcmp(record.title, "Short") == 0, "the 40-byte title is right");
    check(record.chunk == 1 && record.offset == 2 && record.length == 3 &&
              record.lines == 4,
          "the 40-byte record's fields follow its title");

    // Refusals.
    uint8_t empty[WIKI_RECORD_BYTES(64)];
    memset(empty, 0, sizeof empty);
    check(!wiki_record_parse(empty, 64, &record), "an all-NUL record is refused");
    check(!wiki_record_parse(raw, 0, &record), "a zero title width is refused");
    check(!wiki_record_parse(raw, 256, &record), "a width past the maximum is refused");
    check(!wiki_record_parse(NULL, 64, &record), "a null record is refused");

    // Ordering: the index is sorted by this, and a binary search depends on it.
    check(wiki_title_cmp("apple", "Banana") < 0, "case does not outrank letters");
    check(wiki_title_cmp("Banana", "apple") > 0, "the compare is antisymmetric");
    check(wiki_title_cmp("same", "SAME") == 0, "case alone compares equal");
    check(wiki_title_cmp("abc", "abcd") < 0, "a prefix sorts first");
    check(wiki_title_cmp("", "a") < 0, "empty sorts before any title");
    check(wiki_title_cmp("", "") == 0, "two empty titles are equal");
    check(wiki_title_cmp("Zebra", "aardvark") > 0, "folded order crosses case");
    check(wiki_title_cmp("10.53104", "2002 Berlin") < 0, "digits compare as digits");
    check(wiki_title_cmp("A b", "a  b") > 0 || wiki_title_cmp("A b", "a  b") == 0,
          "spaces compare without surprises");

    // Chunk naming bounds: the reader only opens a chunk the pack declares.
    check(wiki_chunk_valid(0, 1), "chunk 0 of a one-chunk pack is valid");
    check(!wiki_chunk_valid(1, 1), "chunk 1 of a one-chunk pack is not");
    check(wiki_chunk_valid(2, 3), "the last chunk of three is valid");
    check(!wiki_chunk_valid(3, 3), "one past the last chunk is not");

    // Library folder names: a pack is a folder under /root/wiki, and the name
    // has to survive being stored without becoming another folder's name.
    check(wiki_pack_dir_ok("simple"), "a plain folder name is a pack candidate");
    check(wiki_pack_dir_ok("wikipedia-en-simple"), "hyphens are fine");
    check(wiki_pack_dir_ok("gutenberg books"), "spaces are fine");
    check(!wiki_pack_dir_ok(""), "an empty name is not a pack");
    check(!wiki_pack_dir_ok("."), "the current directory is not a pack");
    check(!wiki_pack_dir_ok(".."), "the parent directory is not a pack");
    check(!wiki_pack_dir_ok(".Trash-1000"), "a hidden entry is not a pack");
    check(!wiki_pack_dir_ok(NULL), "a null name is not a pack");
    char wide[WIKI_PACK_NAME_BYTES + 8];
    memset(wide, 'x', sizeof wide);
    wide[WIKI_PACK_NAME_BYTES - 1] = '\0';
    check(wiki_pack_dir_ok(wide), "a name one byte short of the limit fits");
    wide[WIKI_PACK_NAME_BYTES - 1] = 'x';
    wide[WIKI_PACK_NAME_BYTES] = '\0';
    check(!wiki_pack_dir_ok(wide), "a name at the limit is refused rather than cut");

    printf("wiki pack: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed == 0 ? 0 : 1;
}
