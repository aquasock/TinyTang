// SPDX-License-Identifier: MIT
// TinyKiwix: an offline encyclopedia on the card, read in the desktop.
//
// The card holds a pack built by tools/make_wiki_pack.py from a Kiwix/openZIM
// archive.  That script explains why the archive itself cannot be read here: a
// ZIM cluster is compressed as a whole and needs about a megabyte of window,
// against 320 KB of OCRAM and a largest free heap block under 70 KB.
//
// A pack is `wiki.meta`, a `wiki.idx` of fixed-size records sorted by folded
// title, and one or more `wiki.NNN` body chunks.  This window binary-searches
// the index by title and pages through a body a window at a time, so neither
// the article count nor an article's length is bounded by RAM: only the page
// buffer is, and it is the same 12 KB TinyHelp uses.
//
// Every FatFS call happens while a list, a page or the index is being loaded --
// never in a draw callback.  TinyHelp's first version read the card inside the
// list's draw path and hard-faulted the board when the window opened; the same
// rule holds here.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "ff.h"
#include "td_apps.h"
}

#include "td_wiki_pack.h"

namespace {

constexpr char WIKI_REAL[] = "/sd/root/wiki";
constexpr char META_NAME[] = "wiki.meta";
constexpr char INDEX_NAME[] = "wiki.idx";

constexpr int LIST_ROWS = 48;          // titles kept in RAM for the browse list
constexpr int TITLE_BYTES = 44;        // what the list keeps of each title
constexpr int PAGE_BYTES = 12288;      // one page of article text, as TinyHelp
constexpr int MAX_PAGE_LINES = 512;
// Where each page began, so Prev can undo a More.  A pack caps an article at
// 256 KB, which is 22 pages at 12 KB, so 32 entries cover every pack this
// converter writes; a longer one simply stops remembering, and Prev can only
// walk back as far as it recorded.
constexpr int MAX_PAGE_STARTS = 32;
constexpr int LINE_BYTES = 160;
constexpr int WINDOW_COLS = 76;
constexpr int META_TITLE_BYTES = 44;
constexpr int META_LICENSE_BYTES = 72;

struct meta_t {
    char title[META_TITLE_BYTES];
    char license[META_LICENSE_BYTES];
    char main_page[TITLE_BYTES];
    uint32_t articles;
    uint32_t chunks;
    uint16_t title_bytes;
    uint16_t record_bytes;
};

td_window_t *s_win;
td_widget_t *s_list;
td_widget_t *s_head;
td_widget_t *s_foot;
td_widget_t *s_primary;    // Find... / More
td_widget_t *s_second;     // Prev / Back
td_widget_t *s_third;      // Next (browse only)

meta_t s_meta;
FIL s_index;
bool s_index_open;
uint8_t s_record_raw[WIKI_MAX_TITLE_BYTES + WIKI_RECORD_OVERHEAD];

uint32_t s_first;                      // index of the first title in the list
int s_count;                           // titles loaded
char s_titles[LIST_ROWS][TITLE_BYTES];

bool s_article;                        // false: browsing titles, true: reading
char s_open_title[TITLE_BYTES];
char *s_page;                          // one page of text, malloc'd on first use
uint16_t s_line_offset[MAX_PAGE_LINES];
int s_line_count;
char s_line[LINE_BYTES];
uint32_t s_chunk;
uint32_t s_body_offset;
uint32_t s_cursor;                     // bytes of the body already paged
uint32_t s_total;                      // body length
bool s_more;                           // another page follows
uint16_t s_page_no;
uint32_t s_page_at[MAX_PAGE_STARTS];    // byte offset of each page's first line
int s_page_count;

void set_text(td_widget_t *w, const char *text)
{
    if (w != nullptr && strcmp(td_widget_text(w), text) != 0)
    {
        td_widget_set_text(w, text);
    }
}

void say(const char *text) { set_text(s_foot, text); }

/* One line from an open file; -1 at end of file, 0 for an empty line.  FatFS
 * here has no f_gets (FF_USE_STRFUNC is off), and the distinction between an
 * empty line and the end is what keeps the caller's loop from spinning. */
int read_line(FIL *file, char *out, int cap)
{
    int n = 0;
    bool any = false;
    for (;;)
    {
        UINT got = 0;
        char c = 0;
        if (f_read(file, &c, 1, &got) != FR_OK || got == 0)
        {
            break;
        }
        any = true;
        if (c == '\n' || n >= cap - 1)
        {
            break;
        }
        out[n++] = c;
    }
    out[n] = '\0';
    return any ? n : -1;
}

/* ---------------------------------------------------------------- metadata */

void meta_defaults(void)
{
    memset(&s_meta, 0, sizeof s_meta);
    s_meta.title_bytes = WIKI_TITLE_BYTES_DEFAULT;
    s_meta.record_bytes = WIKI_RECORD_BYTES(WIKI_TITLE_BYTES_DEFAULT);
    snprintf(s_meta.title, sizeof s_meta.title, "Wikipedia");
}

void copy_value(char *dst, size_t cap, const char *value)
{
    while (*value == ' ' || *value == '\t')
    {
        ++value;
    }
    snprintf(dst, cap, "%s", value);
}

void meta_load(void)
{
    meta_defaults();
    char path[64];
    snprintf(path, sizeof path, "%s/%s", WIKI_REAL, META_NAME);
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return;
    }
    char line[LINE_BYTES];
    while (read_line(&file, line, sizeof line) >= 0)
    {
        char *equals = strchr(line, '=');
        if (equals == nullptr)
        {
            continue;
        }
        *equals = '\0';
        char *key = line;
        char *value = equals + 1;
        while (*key == ' ' || *key == '\t')
        {
            ++key;
        }
        char *end = key + strlen(key);
        while (end > key && (end[-1] == ' ' || end[-1] == '\t'))
        {
            *--end = '\0';
        }
        if (strcmp(key, "title") == 0)
        {
            copy_value(s_meta.title, sizeof s_meta.title, value);
        }
        else if (strcmp(key, "license") == 0)
        {
            copy_value(s_meta.license, sizeof s_meta.license, value);
        }
        else if (strcmp(key, "main_page") == 0)
        {
            copy_value(s_meta.main_page, sizeof s_meta.main_page, value);
        }
        else if (strcmp(key, "articles") == 0)
        {
            s_meta.articles = (uint32_t)strtoul(value, nullptr, 10);
        }
        else if (strcmp(key, "chunks") == 0)
        {
            s_meta.chunks = (uint32_t)strtoul(value, nullptr, 10);
        }
        else if (strcmp(key, "title_bytes") == 0)
        {
            unsigned long v = strtoul(value, nullptr, 10);
            if (v >= 8 && v <= WIKI_MAX_TITLE_BYTES)
            {
                s_meta.title_bytes = (uint16_t)v;
                s_meta.record_bytes = WIKI_RECORD_BYTES(s_meta.title_bytes);
            }
        }
        else if (strcmp(key, "record_bytes") == 0)
        {
            unsigned long v = strtoul(value, nullptr, 10);
            if (v == (unsigned long)s_meta.title_bytes + WIKI_RECORD_OVERHEAD)
            {
                s_meta.record_bytes = (uint16_t)v;
            }
        }
    }
    (void)f_close(&file);
}

/* ------------------------------------------------------------------- index */

void index_close(void)
{
    if (s_index_open)
    {
        (void)f_close(&s_index);
        s_index_open = false;
    }
}

bool index_open(void)
{
    index_close();
    char path[64];
    snprintf(path, sizeof path, "%s/%s", WIKI_REAL, INDEX_NAME);
    if (f_open(&s_index, path, FA_READ) != FR_OK)
    {
        return false;
    }
    s_index_open = true;
    return true;
}

bool read_record(uint32_t index, wiki_record_t *record)
{
    if (!s_index_open || index >= s_meta.articles || s_meta.record_bytes > sizeof s_record_raw)
    {
        return false;
    }
    if (f_lseek(&s_index, (FSIZE_t)index * s_meta.record_bytes) != FR_OK)
    {
        return false;
    }
    UINT got = 0;
    if (f_read(&s_index, s_record_raw, s_meta.record_bytes, &got) != FR_OK ||
        got != s_meta.record_bytes)
    {
        return false;
    }
    return wiki_record_parse(s_record_raw, s_meta.title_bytes, record);
}

/* The first index whose title is not less than prefix, in the index's own
 * folded order -- which is what makes a search over millions of titles a
 * handful of card reads rather than a scan. */
uint32_t lower_bound(const char *prefix)
{
    uint32_t low = 0;
    uint32_t high = s_meta.articles;
    wiki_record_t record;
    while (low < high)
    {
        uint32_t mid = low + (high - low) / 2;
        if (!read_record(mid, &record))
        {
            break;
        }
        if (wiki_title_cmp(record.title, prefix) < 0)
        {
            low = mid + 1;
        }
        else
        {
            high = mid;
        }
    }
    return low;
}

/* ---------------------------------------------------------------------- UI */

void apply_mode(void)
{
    /* The two paging buttons keep their places in both views: Page Up and
     * Page Down mean "the previous / next page of whatever is in front of
     * you", which is the list of titles or the article.  Only the leftmost
     * button changes: Find... while browsing, Back while reading. */
    if (s_article)
    {
        set_text(s_primary, "Back");
    }
    else
    {
        set_text(s_primary, "Find...");
    }
    set_text(s_second, "Page Up");
    set_text(s_third, "Page Down");
    td_widget_set_visible(s_third, true);
}

void note_rows(void)
{
    char text[64];
    if (s_article)
    {
        snprintf(text, sizeof text, "page %u%s", (unsigned)s_page_no,
                 s_more ? "" : ", end of article");
        say(text);
        return;
    }
    uint32_t shown = s_first + (uint32_t)s_count;
    snprintf(text, sizeof text, "%u-%u of %u", (unsigned)(s_count ? s_first + 1 : 0),
             (unsigned)shown, (unsigned)s_meta.articles);
    set_text(s_foot, text);
}

void load_window(uint32_t first)
{
    if (s_meta.articles == 0)
    {
        s_count = 0;
        s_first = 0;
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
        if (first + (uint32_t)i >= s_meta.articles || !read_record(first + (uint32_t)i, &record))
        {
            break;
        }
        snprintf(s_titles[s_count], TITLE_BYTES, "%s", record.title);
        ++s_count;
    }
    td_list_set_count(s_list, s_count);
    td_list_select(s_list, 0);
}

void show_list(void)
{
    s_article = false;
    s_page_no = 0;
    s_line_count = 0;
    apply_mode();
    set_text(s_head, s_meta.title);
    load_window(s_first);
    note_rows();
    td_widget_focus(s_list);
}

/* ---------------------------------------------------------------- article */

void read_page(void);
int leading_title_rows(void);

void open_article(const wiki_record_t *record)
{
    if (record->length == 0)
    {
        say("That article is empty");
        return;
    }
    s_article = true;
    snprintf(s_open_title, sizeof s_open_title, "%s", record->title);
    s_chunk = record->chunk;
    s_body_offset = record->offset;
    s_total = record->length;
    s_cursor = 0;
    s_page_no = 1;
    s_page_count = 1;
    s_page_at[0] = 0;
    apply_mode();
    set_text(s_head, s_open_title);
    read_page();
    td_widget_focus(s_list);
}

void read_page(void)
{
    s_line_count = 0;
    s_more = false;
    if (s_cursor >= s_total)
    {
        td_list_set_count(s_list, 0);
        note_rows();
        return;
    }
    if (s_page == nullptr)
    {
        s_page = static_cast<char *>(malloc(PAGE_BYTES));
        if (s_page == nullptr)
        {
            say("Not enough memory to open an article");
            return;
        }
    }
    char name[24];
    snprintf(name, sizeof name, "wiki.%03u", (unsigned)s_chunk);
    char path[64];
    snprintf(path, sizeof path, "%s/%s", WIKI_REAL, name);
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
        say("The card refused that article");
        return;
    }
    UINT got = 0;
    const FRESULT result = f_read(&file, s_page, want, &got);
    (void)f_close(&file);
    if (result != FR_OK)
    {
        say("The card refused that article");
        return;
    }
    s_page[got] = '\0';

    /* A page ends on a line boundary: any trailing partial line stays for the
     * next page, so no sentence is cut in half by a page turn. */
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

    td_list_set_count(s_list, s_line_count);
    td_list_select(s_list, 0);
    note_rows();
}

/* An article's first heading is usually its own title, which the heading row
 * already shows; the pack keeps it because a converter cannot tell a real
 * section heading from MediaWiki's repeated page title in every archive.
 * Drop it here, where the title being displayed is known.  The bytes stay in
 * the page -- only the first one or two rows are skipped. */
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
    // Skip the heading and the blank line the converter leaves under it.
    int skip = 1;
    if (s_line_count > 1 && s_page[s_line_offset[1]] == '\0')
    {
        skip = 2;
    }
    return skip;
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
    if (s_article)
    {
        if (index < 0 || index >= s_line_count)
        {
            return "";
        }
        if (prepare_line(s_page + s_line_offset[index]))
        {
            *fg = td_theme()->accent;
        }
        return s_line;
    }
    if (index < 0 || index >= s_count)
    {
        return "";
    }
    *fg = td_theme()->accent;
    return s_titles[index];
}

/* ---------------------------------------------------------------- actions */

void more_page(void)
{
    if (!s_more)
    {
        say("End of the article");
        return;
    }
    if (s_page_count < MAX_PAGE_STARTS)
    {
        s_page_at[s_page_count++] = s_cursor;
    }
    ++s_page_no;
    read_page();
}

void prev_page(void)
{
    if (s_page_no <= 1)
    {
        say("This is the start of the article");
        return;
    }
    --s_page_no;
    s_cursor = s_page_at[s_page_no - 1];
    s_page_count = s_page_no;
    read_page();
}

void on_choose(td_widget_t *, void *)
{
    const int index = td_list_selected(s_list);
    if (s_article)
    {
        more_page();
        return;
    }
    wiki_record_t record;
    if (index < 0 || index >= s_count ||
        !read_record(s_first + (uint32_t)index, &record))
    {
        say("That title is not in the pack");
        return;
    }
    open_article(&record);
}

void on_find_done(const char *text, void *)
{
    if (text == nullptr || text[0] == '\0')
    {
        return;
    }
    char prefix[TITLE_BYTES];
    snprintf(prefix, sizeof prefix, "%s", text);
    s_article = false;
    apply_mode();
    set_text(s_head, s_meta.title);
    load_window(lower_bound(prefix));
    note_rows();
}

void on_find(td_widget_t *, void *)
{
    if (s_meta.articles == 0)
    {
        say("No pack on the card");
        return;
    }
    (void)td_inputbox("TinyKiwix", "Title starts with", "", on_find_done, nullptr);
}

void on_second(td_widget_t *, void *)
{
    if (s_article)
    {
        prev_page();
        return;
    }
    if (s_first == 0)
    {
        say("This is the start of the index");
        return;
    }
    load_window(s_first > (uint32_t)LIST_ROWS ? s_first - LIST_ROWS : 0);
    note_rows();
}

void on_third(td_widget_t *, void *)
{
    if (s_article)
    {
        more_page();
        return;
    }
    uint32_t next = s_first + (uint32_t)LIST_ROWS;
    if (next >= s_meta.articles)
    {
        say("This is the end of the index");
        return;
    }
    load_window(next);
    note_rows();
}

void on_primary(td_widget_t *w, void *user)
{
    if (s_article)
    {
        show_list();
    }
    else
    {
        on_find(w, user);
    }
}

void on_close(td_window_t *)
{
    s_win = nullptr;
    s_list = nullptr;
    s_head = nullptr;
    s_foot = nullptr;
    s_primary = nullptr;
    s_second = nullptr;
    s_third = nullptr;
    index_close();
    if (s_page != nullptr)
    {
        free(s_page);
        s_page = nullptr;
    }
    s_article = false;
    s_count = 0;
    s_first = 0;
    s_line_count = 0;
    s_open_title[0] = '\0';
}

void launch(void)
{
    if (td_win_is_open(s_win))
    {
        td_win_focus(s_win);
        return;
    }
    td_window_desc_t d = {};
    d.title = "TinyKiwix";
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
    s_primary = td_button(s_win, 0, -3, "Find...", on_primary, nullptr);
    s_second = td_button(s_win, 12, -3, "Prev", on_second, nullptr);
    s_third = td_button(s_win, 24, -3, "Next", on_third, nullptr);
    s_foot = td_label(s_win, 0, -1, 0, "");
    td_widget_set_color(s_foot, td_theme()->dim, TD_COLOR_DEFAULT);

    meta_load();
    if (!index_open() || s_meta.articles == 0)
    {
        set_text(s_head, s_meta.title);
        td_list_set_count(s_list, 0);
        say("No pack on the card: make one with tools/make_wiki_pack.py");
        return;
    }
    s_first = 0;
    if (s_meta.main_page[0] != '\0')
    {
        s_first = lower_bound(s_meta.main_page);
        if (s_first >= s_meta.articles)
        {
            s_first = 0;
        }
    }
    show_list();
}

const td_app_t s_app = {"TinyKiwix", launch, "W"};

} // namespace

extern "C" void td_kiwix_register(void)
{
    (void)td_app_register(&s_app);
}
