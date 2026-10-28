// SPDX-License-Identifier: MIT
// TinyGutenberg: Project Gutenberg books on the card, read in the desktop.
//
// A *shelf* is a pack folder under /root/wiki made by tools/make_wiki_pack.py
// from a Kiwix Gutenberg archive (one ZIM per Library of Congress class), so
// the Library here lists shelves exactly as TinyWiki lists encyclopedias.
//
// What makes this a different app rather than another view: a book is long, so
// it needs a contents list, a place to keep your page, and a way to look up an
// author.  The pack carries the first two facts ready-made:
//
//   wiki.parts   the chapters: record number, byte offset, title
//   wiki.auth    one author per book, ordered by surname
//
// Reading position lives in /root/gutenberg.pos -- the shelf, the book's record
// number and the page -- written when you leave a book and when the window
// closes, and offered back the next time the app opens.
//
// The pack plumbing is td_pack.c, shared with TinyWiki.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "ff.h"
#include "td_apps.h"
#include "td_pack.h"
}

#include "td_wiki_pack.h"

namespace {

constexpr int LIST_ROWS = 48;
constexpr int TITLE_BYTES = 44;
constexpr int AUTHOR_BYTES = 48;
constexpr int PAGE_BYTES = 12288;
constexpr int MAX_PAGE_LINES = 512;
constexpr int LINE_BYTES = 160;
constexpr int WINDOW_COLS = 76;
constexpr int MAX_PAGE_STARTS = 32;
constexpr int MAX_CHAPTERS = 64;       // a contents list the window can show
constexpr int PART_BYTES = 56;         // matches tools/make_wiki_pack.py
constexpr int AUTHOR_ROW_BYTES = 52;
constexpr char POS_REAL[] = "/sd/root/gutenberg.pos";

enum mode_t {
    MODE_LIBRARY,   // shelves
    MODE_TITLES,    // a shelf's books, by title
    MODE_AUTHORS,   // a shelf's authors, by surname
    MODE_BOOKS_OF,  // the books of one author
    MODE_READING,   // a page of a book
    MODE_CONTENTS,  // that book's chapters
};

td_window_t *s_win;
td_widget_t *s_list;
td_widget_t *s_head;
td_widget_t *s_foot;
td_widget_t *s_primary;   // Refresh / Back
td_widget_t *s_swap;      // Authors <-> Titles
td_widget_t *s_second;    // Page Up
td_widget_t *s_third;     // Page Down
td_widget_t *s_find;      // Find... / Contents

mode_t s_mode;
td_pack_entry_t s_packs[TD_PACK_MAX];
int s_pack_count;
int s_pack_open;

td_pack_meta_t s_meta;
FIL s_index;
bool s_index_open;

/* the title window */
uint32_t s_first;
int s_count;
char s_titles[LIST_ROWS][TITLE_BYTES];

/* the author window, and the books of one author */
char s_authors[LIST_ROWS][AUTHOR_BYTES];
uint32_t s_author_book[LIST_ROWS];
uint32_t s_author_first;
int s_author_count;
uint32_t s_books_of[MAX_CHAPTERS];
char s_books_of_title[MAX_CHAPTERS][TITLE_BYTES];
int s_books_of_count;
char s_author_title[AUTHOR_BYTES];

/* one page of a book */
char s_open_title[TITLE_BYTES];
char *s_page;
uint16_t s_line_offset[MAX_PAGE_LINES];
int s_line_count;
char s_line[LINE_BYTES];
uint32_t s_chunk;
uint32_t s_body_offset;
uint32_t s_cursor;
uint32_t s_total;
bool s_more;
uint16_t s_page_no;
uint32_t s_page_at[MAX_PAGE_STARTS];
int s_page_count;
uint32_t s_book_record;

/* chapters of the open book */
char s_chapter_title[MAX_CHAPTERS][AUTHOR_BYTES];
uint32_t s_chapter_offset[MAX_CHAPTERS];
int s_chapter_count;

void set_text(td_widget_t *w, const char *text)
{
    if (w != nullptr && strcmp(td_widget_text(w), text) != 0)
    {
        td_widget_set_text(w, text);
    }
}

void say(const char *text) { set_text(s_foot, text); }

void sync_buttons(void);

/* ------------------------------------------------------------------ index */

void index_close(void)
{
    if (s_index_open)
    {
        (void)f_close(&s_index);
        s_index_open = false;
    }
}

bool index_open(const char *dir)
{
    index_close();
    char path[96];
    snprintf(path, sizeof path, "%s/wiki.idx", dir);
    if (f_open(&s_index, path, FA_READ) != FR_OK)
    {
        return false;
    }
    s_index_open = true;
    return true;
}

bool pack_dir(char *out, size_t cap)
{
    if (s_pack_open < 0 || s_pack_open >= s_pack_count)
    {
        return false;
    }
    td_pack_dir_of(TD_PACK_BOOKS_ROOT, s_packs[s_pack_open].folder, out, cap);
    return true;
}

/* --------------------------------------------------------------- chapters */

/* The first chapter row of a book, by binary search over wiki.parts, or -1. */
long parts_first(uint32_t book)
{
    char dir[96];
    if (!pack_dir(dir, sizeof dir))
    {
        return -1;
    }
    char path[96];
    snprintf(path, sizeof path, "%s/wiki.parts", dir);
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return -1;
    }
    FSIZE_t size = f_size(&file);
    uint32_t rows = (uint32_t)(size / PART_BYTES);
    uint32_t low = 0;
    uint32_t high = rows;
    uint8_t row[PART_BYTES];
    while (low < high)
    {
        uint32_t mid = low + (high - low) / 2;
        UINT got = 0;
        if (f_lseek(&file, (FSIZE_t)mid * PART_BYTES) != FR_OK ||
            f_read(&file, row, PART_BYTES, &got) != FR_OK || got != PART_BYTES)
        {
            (void)f_close(&file);
            return -1;
        }
        uint32_t row_book = wiki_le32(row);
        if (row_book < book)
        {
            low = mid + 1;
        }
        else
        {
            high = mid;
        }
    }
    (void)f_close(&file);
    return low < rows ? (long)low : -1;
}

void load_chapters(void)
{
    s_chapter_count = 0;
    long first = parts_first(s_book_record);
    if (first < 0)
    {
        return;
    }
    char dir[96];
    if (!pack_dir(dir, sizeof dir))
    {
        return;
    }
    char path[96];
    snprintf(path, sizeof path, "%s/wiki.parts", dir);
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return;
    }
    if (f_lseek(&file, (FSIZE_t)first * PART_BYTES) != FR_OK)
    {
        (void)f_close(&file);
        return;
    }
    uint8_t row[PART_BYTES];
    while (s_chapter_count < MAX_CHAPTERS)
    {
        UINT got = 0;
        if (f_read(&file, row, PART_BYTES, &got) != FR_OK || got != PART_BYTES)
        {
            break;
        }
        if (wiki_le32(row) != s_book_record)
        {
            break;
        }
        s_chapter_offset[s_chapter_count] = wiki_le32(row + 4);
        snprintf(s_chapter_title[s_chapter_count], sizeof s_chapter_title[0], "%.*s",
                 AUTHOR_BYTES - 1, (const char *)(row + 8));
        ++s_chapter_count;
    }
    (void)f_close(&file);
}

/* ----------------------------------------------------------------- authors */

/* Fill the author window from wiki.auth starting at row `first`. */
void load_authors(uint32_t first)
{
    s_author_count = 0;
    s_author_first = first;
    char dir[96];
    if (!pack_dir(dir, sizeof dir))
    {
        return;
    }
    char path[96];
    snprintf(path, sizeof path, "%s/wiki.auth", dir);
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return;
    }
    if (f_lseek(&file, (FSIZE_t)first * AUTHOR_ROW_BYTES) != FR_OK)
    {
        (void)f_close(&file);
        return;
    }
    uint8_t row[AUTHOR_ROW_BYTES];
    while (s_author_count < LIST_ROWS)
    {
        UINT got = 0;
        if (f_read(&file, row, AUTHOR_ROW_BYTES, &got) != FR_OK ||
            got != AUTHOR_ROW_BYTES)
        {
            break;
        }
        s_author_book[s_author_count] = wiki_le32(row);
        snprintf(s_authors[s_author_count], AUTHOR_BYTES, "%.*s", AUTHOR_BYTES - 1,
                 (const char *)(row + 4));
        ++s_author_count;
    }
    (void)f_close(&file);
    td_list_set_count(s_list, s_author_count);
    td_list_select(s_list, 0);
}

/* Every book by the author at `row` of wiki.auth. */
void load_books_of(uint32_t row)
{
    s_books_of_count = 0;
    char dir[96];
    if (!pack_dir(dir, sizeof dir))
    {
        return;
    }
    char path[96];
    snprintf(path, sizeof path, "%s/wiki.auth", dir);
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return;
    }
    if (f_lseek(&file, (FSIZE_t)row * AUTHOR_ROW_BYTES) != FR_OK)
    {
        (void)f_close(&file);
        return;
    }
    uint8_t first[AUTHOR_ROW_BYTES];
    UINT got = 0;
    if (f_read(&file, first, AUTHOR_ROW_BYTES, &got) != FR_OK || got != AUTHOR_ROW_BYTES)
    {
        (void)f_close(&file);
        return;
    }
    snprintf(s_author_title, sizeof s_author_title, "%.*s", AUTHOR_BYTES - 1,
             (const char *)(first + 4));
    (void)f_close(&file);

    // The table is ordered by surname, so one walk over it collects the rest.
    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return;
    }
    uint8_t rowbuf[AUTHOR_ROW_BYTES];
    while (s_books_of_count < MAX_CHAPTERS &&
           f_read(&file, rowbuf, AUTHOR_ROW_BYTES, &got) == FR_OK &&
           got == AUTHOR_ROW_BYTES)
    {
        if (strncmp((const char *)(rowbuf + 4), s_author_title, AUTHOR_BYTES) == 0 &&
            s_books_of_count < MAX_CHAPTERS)
        {
            uint32_t number = wiki_le32(rowbuf);
            wiki_record_t record;
            if (td_pack_record(&s_index, &s_meta, number, &record))
            {
                snprintf(s_books_of_title[s_books_of_count], TITLE_BYTES, "%s",
                         record.title);
                s_books_of[s_books_of_count++] = number;
            }
        }
    }
    (void)f_close(&file);
}

/* --------------------------------------------------------------- position */

void position_save(void)
{
    if (s_pack_open < 0 || s_book_record == 0)
    {
        return;
    }
    FIL file;
    if (f_open(&file, POS_REAL, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        return;
    }
    char line[128];
    snprintf(line, sizeof line, "%s\n%lu\n%u\n", s_packs[s_pack_open].folder,
             (unsigned long)s_book_record, (unsigned)s_page_no);
    UINT wrote = 0;
    (void)f_write(&file, line, strlen(line), &wrote);
    (void)f_close(&file);
}

/* --------------------------------------------------------------- reading */

void read_page(void);
int leading_title_rows(void);

void open_book(uint32_t record_number);

void show_reading(void)
{
    s_mode = MODE_READING;
    set_text(s_head, s_open_title);
    td_list_set_count(s_list, s_line_count);
    td_list_select(s_list, 0);
    char text[48];
    snprintf(text, sizeof text, "page %u%s", (unsigned)s_page_no,
             s_more ? "" : ", end of book");
    say(text);
}

void read_page(void)
{
    s_line_count = 0;
    s_more = false;
    if (s_cursor >= s_total)
    {
        td_list_set_count(s_list, 0);
        return;
    }
    if (s_page == nullptr)
    {
        s_page = static_cast<char *>(malloc(PAGE_BYTES));
        if (s_page == nullptr)
        {
            say("Not enough memory to open a book");
            return;
        }
    }
    char dir[96];
    if (!pack_dir(dir, sizeof dir))
    {
        return;
    }
    char name[24];
    snprintf(name, sizeof name, "wiki.%03u", (unsigned)s_chunk);
    char path[128];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        say("That chunk is not on the card");
        return;
    }
    uint32_t room = s_total - s_cursor;
    UINT want = room > PAGE_BYTES - 1 ? PAGE_BYTES - 1 : (UINT)room;
    if (f_lseek(&file, (FSIZE_t)(s_body_offset + s_cursor)) != FR_OK)
    {
        (void)f_close(&file);
        say("The card refused that page");
        return;
    }
    UINT got = 0;
    const FRESULT result = f_read(&file, s_page, want, &got);
    (void)f_close(&file);
    if (result != FR_OK)
    {
        say("The card refused that page");
        return;
    }
    s_page[got] = '\0';

    UINT used = got;
    if (s_cursor + got < s_total)
    {
        UINT cut = got;
        while (cut > 0 && s_page[cut - 1] != '\n')
        {
            --cut;
        }
        if (cut > 0)
        {
            used = cut;
        }
    }

    int line = 0;
    UINT at = 0;
    while (at < used && line < MAX_PAGE_LINES)
    {
        s_line_offset[line++] = (uint16_t)at;
        while (at < used && s_page[at] != '\n')
        {
            ++at;
        }
        if (at < used)
        {
            s_page[at++] = '\0';
        }
    }
    s_line_count = line;
    s_cursor += used;
    s_more = s_cursor < s_total;

    const int skip = leading_title_rows();
    if (skip > 0 && skip < s_line_count)
    {
        memmove(s_line_offset, s_line_offset + skip,
                (size_t)(s_line_count - skip) * sizeof s_line_offset[0]);
        s_line_count -= skip;
    }
}

int leading_title_rows(void)
{
    if (s_page_no != 1 || s_line_count == 0)
    {
        return 0;
    }
    const char *line = s_page + s_line_offset[0];
    if (line[0] != '#' || line[1] != '#' || line[2] != ' ')
    {
        return 0;
    }
    const char *heading = line + 3;
    const char *title = s_open_title;
    while (*heading != '\0' && *title != '\0')
    {
        char a = (*heading >= 'A' && *heading <= 'Z') ? (char)(*heading - 'A' + 'a') : *heading;
        char b = (*title >= 'A' && *title <= 'Z') ? (char)(*title - 'A' + 'a') : *title;
        if (a != b)
        {
            return 0;
        }
        ++heading;
        ++title;
    }
    if (*heading != '\0' || *title != '\0')
    {
        return 0;
    }
    int skip = 1;
    if (s_line_count > 1 && s_page[s_line_offset[1]] == '\0')
    {
        skip = 2;
    }
    return skip;
}

void open_book(uint32_t record_number)
{
    wiki_record_t record;
    if (!td_pack_record(&s_index, &s_meta, record_number, &record) || record.length == 0)
    {
        say("That book is not readable");
        return;
    }
    s_book_record = record_number;
    snprintf(s_open_title, sizeof s_open_title, "%s", record.title);
    s_chunk = record.chunk;
    s_body_offset = record.offset;
    s_total = record.length;
    s_cursor = 0;
    s_page_no = 1;
    s_page_count = 1;
    s_page_at[0] = 0;
    read_page();
    load_chapters();
    s_mode = MODE_READING;
    sync_buttons();
    show_reading();
    td_widget_focus(s_list);
}

/* Page Down / Page Up inside a book. */
void more_page(void)
{
    if (!s_more)
    {
        say("End of the book");
        return;
    }
    if (s_page_count < MAX_PAGE_STARTS)
    {
        s_page_at[s_page_count++] = s_cursor;
    }
    ++s_page_no;
    read_page();
    show_reading();
}

void prev_page(void)
{
    if (s_page_no <= 1)
    {
        say("This is the start of the book");
        return;
    }
    if (s_page_no > s_page_count)
    {
        /* Past what the table remembers: walk from the top rather than jump to
         * an offset that was never recorded. */
        uint32_t target = s_page_no - 1;
        s_cursor = 0;
        s_page_no = 1;
        s_page_count = 1;
        s_page_at[0] = 0;
        read_page();
        while (s_page_no < target && s_more)
        {
            if (s_page_count < MAX_PAGE_STARTS)
            {
                s_page_at[s_page_count++] = s_cursor;
            }
            ++s_page_no;
            read_page();
        }
        show_reading();
        return;
    }
    --s_page_no;
    s_cursor = s_page_at[s_page_no - 1];
    s_page_count = s_page_no;
    read_page();
    show_reading();
}

void jump_to_chapter(int index)
{
    if (index < 0 || index >= s_chapter_count)
    {
        return;
    }
    s_cursor = s_chapter_offset[index];
    s_page_no = 1;
    s_page_count = 1;
    s_page_at[0] = s_cursor;
    read_page();
    s_mode = MODE_READING;
    sync_buttons();
    set_text(s_head, s_open_title);
    show_reading();
    td_widget_focus(s_list);
}

/* -------------------------------------------------------------------- UI */

void apply_mode(void)
{
    bool in_book = s_mode == MODE_READING || s_mode == MODE_CONTENTS;
    set_text(s_primary, s_mode == MODE_LIBRARY ? "Refresh" : "Back");
    set_text(s_second, "Page Up");
    set_text(s_third, "Page Down");
    td_widget_set_visible(s_third, true);

    // The swap button offers the other way of browsing, and disappears nowhere
    // it would be meaningless.
    if (s_mode == MODE_AUTHORS)
    {
        set_text(s_swap, "Titles");
        td_widget_set_visible(s_swap, true);
    }
    else if (s_mode == MODE_TITLES || s_mode == MODE_BOOKS_OF)
    {
        set_text(s_swap, "Authors");
        td_widget_set_visible(s_swap, s_meta.authors > 0);
    }
    else
    {
        td_widget_set_visible(s_swap, false);
    }

    if (s_find != nullptr)
    {
        if (in_book)
        {
            set_text(s_find, "Contents");
            td_widget_set_visible(s_find, s_chapter_count > 0 && s_mode == MODE_READING);
        }
        else
        {
            set_text(s_find, "Find...");
            td_widget_set_visible(s_find, s_mode != MODE_LIBRARY);
        }
    }
}

void sync_buttons(void) { apply_mode(); }

void show_library(void)
{
    s_mode = MODE_LIBRARY;
    s_pack_open = -1;
    index_close();
    sync_buttons();
    set_text(s_head, "Book shelves");
    td_list_set_count(s_list, s_pack_count);
    td_list_select(s_list, 0);
    char text[48];
    snprintf(text, sizeof text, "%d shelf%s", s_pack_count,
             s_pack_count == 1 ? "" : "s");
    say(text);
    td_widget_focus(s_list);
    if (s_pack_count == 0)
    {
        say("No book shelf: make one into /root/books");
    }
}

void load_titles(uint32_t first)
{
    if (s_meta.articles == 0)
    {
        s_count = 0;
        td_list_set_count(s_list, 0);
        return;
    }
    if (first >= s_meta.articles)
    {
        first = s_meta.articles - 1;
    }
    s_first = first;
    s_count = 0;
    wiki_record_t record;
    for (int i = 0; i < LIST_ROWS; ++i)
    {
        if (first + (uint32_t)i >= s_meta.articles ||
            !td_pack_record(&s_index, &s_meta, first + (uint32_t)i, &record))
        {
            break;
        }
        snprintf(s_titles[s_count], TITLE_BYTES, "%s", record.title);
        ++s_count;
    }
    td_list_set_count(s_list, s_count);
    td_list_select(s_list, 0);
}

void note_titles(void)
{
    uint32_t shown = s_first + (uint32_t)s_count;
    char text[64];
    snprintf(text, sizeof text, "%u-%u of %u", (unsigned)(s_count ? s_first + 1 : 0),
             (unsigned)shown, (unsigned)s_meta.articles);
    say(text);
}

void show_titles(void)
{
    s_mode = MODE_TITLES;
    sync_buttons();
    set_text(s_head, s_meta.title);
    load_titles(s_first);
    note_titles();
    td_widget_focus(s_list);
}

void show_authors(void)
{
    if (s_meta.authors == 0)
    {
        say("This shelf has no author table");
        return;
    }
    s_mode = MODE_AUTHORS;
    sync_buttons();
    set_text(s_head, "Authors");
    load_authors(0);
    char text[48];
    snprintf(text, sizeof text, "%u author%s", (unsigned)s_meta.authors,
             s_meta.authors == 1 ? "" : "s");
    say(text);
    td_widget_focus(s_list);
}

void show_books_of(void)
{
    s_mode = MODE_BOOKS_OF;
    sync_buttons();
    set_text(s_head, s_author_title);
    s_count = s_books_of_count;
    td_list_set_count(s_list, s_count);
    td_list_select(s_list, 0);
    char text[48];
    snprintf(text, sizeof text, "%d book%s", s_count, s_count == 1 ? "" : "s");
    say(text);
    td_widget_focus(s_list);
}

void show_contents(void)
{
    s_mode = MODE_CONTENTS;
    sync_buttons();
    set_text(s_head, s_open_title);
    td_list_set_count(s_list, s_chapter_count);
    td_list_select(s_list, 0);
    char text[48];
    snprintf(text, sizeof text, "%d chapter%s", s_chapter_count,
             s_chapter_count == 1 ? "" : "s");
    say(text);
    td_widget_focus(s_list);
}

bool open_shelf(int index)
{
    if (index < 0 || index >= s_pack_count)
    {
        return false;
    }
    char dir[96];
    td_pack_dir_of(TD_PACK_BOOKS_ROOT, s_packs[index].folder, dir, sizeof dir);
    if (!td_pack_meta_read(dir, &s_meta))
    {
        say("That shelf's metadata is unreadable");
        return false;
    }
    if (!index_open(dir) || s_meta.articles == 0)
    {
        say("That shelf's index is unreadable");
        index_close();
        return false;
    }
    s_pack_open = index;
    s_first = 0;
    show_titles();
    return true;
}

/* --------------------------------------------------------------- drawing */

bool prepare_line(const char *in)
{
    while (*in == ' ' || *in == '\t')
    {
        ++in;
    }
    bool heading = false;
    if (in[0] == '#' && in[1] == '#')
    {
        in += 2;
        while (*in == ' ')
        {
            ++in;
        }
        heading = true;
    }
    int out = 0;
    while (*in != '\0' && out < LINE_BYTES - 1)
    {
        if (in[0] == '*' && in[1] == '*')
        {
            in += 2;
            continue;
        }
        if (in[0] == '`')
        {
            ++in;
            continue;
        }
        s_line[out++] = *in++;
    }
    s_line[out] = '\0';
    if (static_cast<int>(strlen(s_line)) > WINDOW_COLS)
    {
        s_line[WINDOW_COLS] = '\0';
    }
    return heading;
}

const char *list_item(td_widget_t *, int index, int *fg, void *)
{
    switch (s_mode)
    {
    case MODE_LIBRARY:
        if (index < 0 || index >= s_pack_count)
        {
            return "";
        }
        *fg = td_theme()->accent;
        if (s_packs[index].folder[0] != '\0')
        {
            snprintf(s_line, sizeof s_line, "%s  [%s]", s_packs[index].title,
                     s_packs[index].folder);
        }
        else
        {
            snprintf(s_line, sizeof s_line, "%s", s_packs[index].title);
        }
        return s_line;
    case MODE_AUTHORS:
        if (index < 0 || index >= s_author_count)
        {
            return "";
        }
        *fg = td_theme()->accent;
        return s_authors[index];
    case MODE_BOOKS_OF:
        if (index < 0 || index >= s_books_of_count)
        {
            return "";
        }
        *fg = td_theme()->accent;
        return s_books_of_title[index];
    case MODE_CONTENTS:
        if (index < 0 || index >= s_chapter_count)
        {
            return "";
        }
        *fg = td_theme()->accent;
        snprintf(s_line, sizeof s_line, "%s", s_chapter_title[index]);
        return s_line;
    case MODE_READING:
        if (index < 0 || index >= s_line_count)
        {
            return "";
        }
        if (prepare_line(s_page + s_line_offset[index]))
        {
            *fg = td_theme()->accent;
        }
        return s_line;
    case MODE_TITLES:
    default:
        if (index < 0 || index >= s_count)
        {
            return "";
        }
        *fg = td_theme()->accent;
        return s_titles[index];
    }
}

/* --------------------------------------------------------------- actions */

void on_choose(td_widget_t *, void *)
{
    const int index = td_list_selected(s_list);
    switch (s_mode)
    {
    case MODE_LIBRARY:
        (void)open_shelf(index);
        return;
    case MODE_TITLES:
        if (index >= 0 && index < s_count)
        {
            open_book(s_first + (uint32_t)index);
        }
        return;
    case MODE_AUTHORS:
        if (index >= 0 && index < s_author_count)
        {
            load_books_of(s_author_first + (uint32_t)index);
            show_books_of();
        }
        return;
    case MODE_BOOKS_OF:
        if (index >= 0 && index < s_books_of_count)
        {
            open_book(s_books_of[index]);
        }
        return;
    case MODE_CONTENTS:
        jump_to_chapter(index);
        return;
    case MODE_READING:
    default:
        more_page();
        return;
    }
}

void on_find_done(const char *text, void *)
{
    if (text == nullptr || text[0] == '\0')
    {
        return;
    }
    if (s_mode == MODE_AUTHORS)
    {
        // Authors are ordered by surname, so a prefix search is a scan of a
        // small table rather than a binary search over a big one.
        char prefix[AUTHOR_BYTES];
        snprintf(prefix, sizeof prefix, "%s", text);
        char dir[96];
        if (!pack_dir(dir, sizeof dir))
        {
            return;
        }
        char path[96];
        snprintf(path, sizeof path, "%s/wiki.auth", dir);
        FIL file;
        if (f_open(&file, path, FA_READ) != FR_OK)
        {
            return;
        }
        uint8_t row[AUTHOR_ROW_BYTES];
        uint32_t number = 0;
        uint32_t start = 0;
        UINT got = 0;
        while (f_read(&file, row, AUTHOR_ROW_BYTES, &got) == FR_OK &&
               got == AUTHOR_ROW_BYTES)
        {
            if (wiki_title_cmp((const char *)(row + 4), prefix) >= 0)
            {
                start = number;
                break;
            }
            ++number;
        }
        (void)f_close(&file);
        load_authors(start);
        return;
    }
    char prefix[TITLE_BYTES];
    snprintf(prefix, sizeof prefix, "%s", text);
    s_mode = MODE_TITLES;
    sync_buttons();
    set_text(s_head, s_meta.title);
    load_titles(td_pack_lower_bound(&s_index, &s_meta, prefix));
    note_titles();
}

void on_find(td_widget_t *, void *)
{
    switch (s_mode)
    {
    case MODE_READING:
        if (s_chapter_count > 0)
        {
            show_contents();
        }
        return;
    case MODE_AUTHORS:
        (void)td_inputbox("TinyGutenberg", "Surname starts with", "", on_find_done,
                          nullptr);
        return;
    case MODE_TITLES:
    case MODE_BOOKS_OF:
        if (s_meta.articles == 0)
        {
            return;
        }
        (void)td_inputbox("TinyGutenberg", "Title starts with", "", on_find_done,
                          nullptr);
        return;
    default:
        return;
    }
}

void page_up(void)
{
    switch (s_mode)
    {
    case MODE_READING:
        prev_page();
        return;
    case MODE_CONTENTS:
        if (s_chapter_count > 0)
        {
            int selected = td_list_selected(s_list);
            td_list_select(s_list, selected > LIST_ROWS ? selected - LIST_ROWS : 0);
        }
        return;
    case MODE_LIBRARY:
    {
        int selected = td_list_selected(s_list);
        td_list_select(s_list, selected > LIST_ROWS ? selected - LIST_ROWS : 0);
        return;
    }
    case MODE_AUTHORS:
        if (s_author_first >= LIST_ROWS)
        {
            load_authors(s_author_first - LIST_ROWS);
        }
        else
        {
            load_authors(0);
        }
        return;
    case MODE_TITLES:
    default:
        if (s_first == 0)
        {
            say("This is the start of the shelf");
            return;
        }
        load_titles(s_first > (uint32_t)LIST_ROWS ? s_first - LIST_ROWS : 0);
        note_titles();
        return;
    }
}

void page_down(void)
{
    switch (s_mode)
    {
    case MODE_READING:
        more_page();
        return;
    case MODE_CONTENTS:
    {
        int selected = td_list_selected(s_list);
        int last = s_chapter_count > 0 ? s_chapter_count - 1 : 0;
        td_list_select(s_list, selected + LIST_ROWS < last ? selected + LIST_ROWS : last);
        return;
    }
    case MODE_LIBRARY:
    {
        int selected = td_list_selected(s_list);
        int last = s_pack_count > 0 ? s_pack_count - 1 : 0;
        td_list_select(s_list, selected + LIST_ROWS < last ? selected + LIST_ROWS : last);
        return;
    }
    case MODE_AUTHORS:
        if (s_author_first + (uint32_t)LIST_ROWS < s_meta.authors)
        {
            load_authors(s_author_first + LIST_ROWS);
        }
        return;
    case MODE_TITLES:
    default:
    {
        uint32_t next = s_first + (uint32_t)LIST_ROWS;
        if (next >= s_meta.articles)
        {
            say("This is the end of the shelf");
            return;
        }
        load_titles(next);
        note_titles();
        return;
    }
    }
}

void on_primary(td_widget_t *, void *)
{
    switch (s_mode)
    {
    case MODE_LIBRARY:
        s_pack_count = td_pack_scan(TD_PACK_BOOKS_ROOT, s_packs, TD_PACK_MAX);
        td_list_set_count(s_list, s_pack_count);
        td_list_select(s_list, 0);
        return;
    case MODE_READING:
        position_save();
        show_titles();
        return;
    case MODE_CONTENTS:
        show_reading();
        return;
    case MODE_AUTHORS:
    case MODE_BOOKS_OF:
        show_titles();
        return;
    case MODE_TITLES:
    default:
        show_library();
        return;
    }
}

void on_swap(td_widget_t *, void *)
{
    if (s_mode == MODE_AUTHORS)
    {
        show_titles();
    }
    else
    {
        show_authors();
    }
}

void on_second(td_widget_t *, void *) { page_up(); }
void on_third(td_widget_t *, void *) { page_down(); }

void on_close(td_window_t *)
{
    position_save();
    s_win = nullptr;
    s_list = nullptr;
    s_head = nullptr;
    s_foot = nullptr;
    s_primary = nullptr;
    s_swap = nullptr;
    s_second = nullptr;
    s_third = nullptr;
    s_find = nullptr;
    index_close();
    if (s_page != nullptr)
    {
        free(s_page);
        s_page = nullptr;
    }
    s_mode = MODE_LIBRARY;
    s_pack_open = -1;
    s_pack_count = 0;
    s_count = 0;
    s_line_count = 0;
    s_chapter_count = 0;
    s_book_record = 0;
}

void launch(void)
{
    if (td_win_is_open(s_win))
    {
        td_win_focus(s_win);
        return;
    }
    td_window_desc_t d = {};
    d.title = "TinyGutenberg";
    d.rect = td_rect(-1, -1, 80, 40);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE | TD_WIN_RESIZABLE;
    d.min_w = 46;
    d.min_h = 14;
    d.on_close = on_close;
    s_win = td_win_create(&d);
    if (s_win == nullptr)
    {
        return;
    }

    s_head = td_label(s_win, 0, 0, 0, "");
    s_list = td_list(s_win, td_rect(0, 1, -1, -4), list_item, on_choose, nullptr);
    td_scrollbar(s_win, -1, 1, -4, s_list);
    s_primary = td_button(s_win, 0, -3, "Refresh", on_primary, nullptr);
    s_swap = td_button(s_win, 12, -3, "Authors", on_swap, nullptr);
    s_second = td_button(s_win, 24, -3, "Page Up", on_second, nullptr);
    s_third = td_button(s_win, 38, -3, "Page Down", on_third, nullptr);
    s_find = td_button(s_win, 52, -3, "Find...", on_find, nullptr);
    s_foot = td_label(s_win, 0, -1, 0, "");
    td_widget_set_color(s_foot, td_theme()->dim, TD_COLOR_DEFAULT);

    s_pack_count = td_pack_scan(TD_PACK_BOOKS_ROOT, s_packs, TD_PACK_MAX);
    show_library();
}

const td_app_t s_app = {"TinyGutenberg", launch, "G"};

} // namespace

extern "C" void td_gutenberg_register(void)
{
    (void)td_app_register(&s_app);
}
