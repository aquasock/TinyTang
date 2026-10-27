// SPDX-License-Identifier: MIT
// TinyHelp: a reader for the board's manual, in the desktop.
//
// The manual is a folder of Markdown chapters on the card; `index.md` lists
// them one per line as `- [Title](file.md)`.  The window shows either that
// contents list or one chapter, and the contents list is how a chapter is
// opened.
//
// Why not the editor: its buffer is 16 KB (TD_EDITOR_MAX), which a manual
// outgrows, and it has no contents, no links and no paging.
//
// A chapter is read once, whole, into a bounded buffer, and split into lines
// there; drawing only copies from that buffer.  The first version read each
// line back from the card as the list drew it, which put f_open/f_lseek/f_read
// inside the widget draw path and hard-faulted the board when the window
// opened: FatFS wants several hundred bytes of stack per call and the draw
// path is already the deepest one in the desktop.  Nothing here touches the
// card except when a chapter is opened or the index is reloaded.
//
// The Markdown is a subset, because the desktop draws characters, not pages:
// `#` headings are shown in the accent colour, `-` keeps its bullet, list
// numbering is left alone, `**bold**`, backticks and `[text](link)` lose their
// markers, and fenced code is shown as written.  Links are only live in the
// contents list.  A line longer than the window is truncated, so chapters keep
// their paragraphs under about 78 characters.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "ff.h"
#include "td_apps.h"
}

namespace {

constexpr char MANUAL_REAL[] = "/sd/root/manual";
constexpr char INDEX_NAME[] = "index.md";
constexpr int MAX_CHAPTERS = 32;
constexpr int TITLE_BYTES = 48;
constexpr int FILE_BYTES = 40;
constexpr int CHAPTER_BYTES = 12288;   // one chapter, whole
constexpr int MAX_LINES = 512;
constexpr int LINE_BYTES = 160;
constexpr int WINDOW_COLS = 78;

struct chapter {
    char title[TITLE_BYTES];
    char file[FILE_BYTES];
};

td_window_t *s_win;
td_widget_t *s_list;
td_widget_t *s_heading;
td_widget_t *s_foot;

chapter s_chapters[MAX_CHAPTERS];
int s_chapter_count;

char s_open_file[FILE_BYTES];      // "" when the contents list is shown
char s_open_title[TITLE_BYTES];

/* The open chapter, and where each of its lines starts in it.  Allocated when
 * a chapter is opened and freed when the window closes: 12 KB is a fifth of the
 * free heap to keep reserved for a window that is nearly always shut. */
char *s_text;
uint16_t s_line_offset[MAX_LINES];
int s_line_count;
bool s_truncated;

/* The line being drawn, with its markers removed.  Static rather than on the
 * stack, and reused by every row, because the list asks for one row at a time. */
char s_line[LINE_BYTES];

void set_text(td_widget_t *w, const char *text)
{
    if (w != nullptr && strcmp(td_widget_text(w), text) != 0) {
        td_widget_set_text(w, text);
    }
}

void say(const char *text) { set_text(s_foot, text); }

/* One line from an open file.  FatFS here is built without FF_USE_STRFUNC, so
 * f_gets does not exist; a byte at a time is fine, because the sector cache
 * makes it a memory copy and this runs once per chapter, not per row.
 * Answers the length, or -1 at end of file. */
int read_line(FIL *file, char *out, int cap)
{
    int n = 0;
    bool any = false;
    for (;;) {
        UINT got = 0;
        char c = 0;
        if (f_read(file, &c, 1, &got) != FR_OK || got == 0) {
            break;
        }
        any = true;
        if (c == '\n' || n >= cap - 1) {
            break;
        }
        out[n++] = c;
    }
    out[n] = '\0';
    // An empty line is 0 bytes and end of file is -1: the caller's loop needs
    // to tell them apart, or it spins on the last line for ever and the
    // watchdog resets the board (the first version of this did exactly that).
    return any ? n : -1;
}

/* ------------------------------------------------------------- markdown */

/* Copy one chapter line into s_line with the markers the desktop cannot draw
 * removed, and answer whether it is a heading so the caller can colour it. */
bool prepare_line(const char *in)
{
    while (*in == ' ' || *in == '\t') {
        ++in;
    }
    bool heading = false;
    if (*in == '#') {
        while (*in == '#') {
            ++in;
        }
        if (*in == ' ') {
            ++in;
        }
        heading = true;
    }

    int out = 0;
    while (*in != '\0' && out < LINE_BYTES - 1) {
        if (in[0] == '*' && in[1] == '*') {
            in += 2;
            continue;
        }
        if (in[0] == '`') {
            ++in;
            continue;
        }
        if (in[0] == '[') {
            ++in;
            continue;
        }
        if (in[0] == ']' && in[1] == '(') {
            const char *close = strchr(in, ')');
            if (close != nullptr) {
                in = close + 1;
                continue;
            }
        }
        s_line[out++] = *in++;
    }
    s_line[out] = '\0';
    return heading;
}

/* ------------------------------------------------------------- chapters */

void split_lines(void)
{
    s_line_count = 0;
    s_truncated = false;
    uint32_t at = 0;
    while (at < CHAPTER_BYTES && s_text[at] != '\0' && s_line_count < MAX_LINES) {
        s_line_offset[s_line_count++] = static_cast<uint16_t>(at);
        while (at < CHAPTER_BYTES && s_text[at] != '\0' && s_text[at] != '\n') {
            ++at;
        }
        if (at < CHAPTER_BYTES && s_text[at] == '\n') {
            s_text[at++] = '\0';
        }
    }
    // Anything left over means the chapter is longer than the buffer.
    while (at < CHAPTER_BYTES && s_text[at] != '\0') {
        if (s_text[at] != '\n') {
            s_truncated = true;
            break;
        }
        ++at;
    }
}

void open_chapter(int index)
{
    if (index < 0 || index >= s_chapter_count) {
        return;
    }
    if (s_text == nullptr) {
        s_text = static_cast<char *>(malloc(CHAPTER_BYTES));
        if (s_text == nullptr) {
            say("Not enough memory to open a chapter");
            return;
        }
    }
    char path[FILE_BYTES + 16];
    snprintf(path, sizeof path, "%s/%s", MANUAL_REAL, s_chapters[index].file);
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK) {
        say("That chapter is not on the card");
        return;
    }
    UINT got = 0;
    const FRESULT result = f_read(&file, s_text, CHAPTER_BYTES - 1, &got);
    (void)f_close(&file);
    if (result != FR_OK) {
        say("The card refused that chapter");
        return;
    }
    s_text[got] = '\0';
    snprintf(s_open_file, sizeof s_open_file, "%s", s_chapters[index].file);
    snprintf(s_open_title, sizeof s_open_title, "%s", s_chapters[index].title);
    split_lines();
    td_list_set_count(s_list, s_line_count);
    td_list_select(s_list, 0);
    td_widget_focus(s_list);
    set_text(s_heading, s_open_title);
    say(s_truncated ? "Chapter longer than the reader's buffer" : "");
}

void load_index(void);

// The contents page, read from the card each time: index.md is a few hundred
// bytes, so coming back from a chapter is also what picks up a chapter added
// or edited since the window opened.  That is why there is no separate Reload
// button -- it did the same thing and looked the same doing it.
void show_contents(void)
{
    load_index();
    s_open_file[0] = '\0';
    td_list_set_count(s_list, s_chapter_count);
    td_list_select(s_list, 0);
    td_widget_focus(s_list);
    set_text(s_heading, "Contents");
    char text[64];
    snprintf(text, sizeof text, "%d chapter%s", s_chapter_count,
             s_chapter_count == 1 ? "" : "s");
    say(text);
}

/* --------------------------------------------------------------- index */

void load_index(void)
{
    s_chapter_count = 0;
    char path[FILE_BYTES + 16];
    snprintf(path, sizeof path, "%s/%s", MANUAL_REAL, INDEX_NAME);
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK) {
        return;
    }
    char line[LINE_BYTES];
    while (s_chapter_count < MAX_CHAPTERS && read_line(&file, line, sizeof line) >= 0) {
        const char *open = strchr(line, '[');
        const char *close = open != nullptr ? strchr(open, ']') : nullptr;
        const char *target = close != nullptr && close[1] == '(' ? close + 2 : nullptr;
        if (open == nullptr || close == nullptr || target == nullptr) {
            continue;
        }
        const char *end = strchr(target, ')');
        if (end == nullptr || end == target) {
            continue;
        }
        chapter &c = s_chapters[s_chapter_count];
        size_t title_len = static_cast<size_t>(close - open - 1);
        if (title_len >= sizeof c.title) {
            title_len = sizeof c.title - 1;
        }
        memcpy(c.title, open + 1, title_len);
        c.title[title_len] = '\0';
        size_t file_len = static_cast<size_t>(end - target);
        if (file_len >= sizeof c.file) {
            continue;                       // a name the reader cannot open
        }
        memcpy(c.file, target, file_len);
        c.file[file_len] = '\0';
        ++s_chapter_count;
    }
    (void)f_close(&file);
}

/* -------------------------------------------------------------- drawing */

const char *list_item(td_widget_t *, int index, int *fg, void *)
{
    if (s_open_file[0] == '\0') {
        if (index < 0 || index >= s_chapter_count) {
            return "";
        }
        *fg = td_theme()->accent;
        return s_chapters[index].title;
    }

    if (index < 0 || index >= s_line_count) {
        return "";
    }
    if (prepare_line(s_text + s_line_offset[index])) {
        *fg = td_theme()->accent;
    }
    if (static_cast<int>(strlen(s_line)) > WINDOW_COLS) {
        s_line[WINDOW_COLS] = '\0';
    }
    return s_line;
}

void on_choose(td_widget_t *, void *)
{
    const int index = td_list_selected(s_list);
    if (s_open_file[0] == '\0') {
        open_chapter(index);
    }
}

void on_contents(td_widget_t *, void *) { show_contents(); }

void on_close(td_window_t *)
{
    s_win = nullptr;
    s_list = nullptr;
    s_heading = nullptr;
    s_foot = nullptr;
    /* Give the chapter buffer back, and clear the view state with it so
     * list_item() cannot read a freed buffer after a close. */
    if (s_text != nullptr) {
        free(s_text);
        s_text = nullptr;
    }
    s_open_file[0] = '\0';
    s_open_title[0] = '\0';
    s_line_count = 0;
    s_line_offset[0] = 0;
    s_truncated = false;
}

void launch(void)
{
    if (td_win_is_open(s_win)) {
        td_win_focus(s_win);
        return;
    }
    td_window_desc_t d = {};
    d.title = "TinyHelp";
    d.rect = td_rect(-1, -1, 80, 32);
    d.flags = TD_WIN_MOVABLE | TD_WIN_CLOSABLE | TD_WIN_RESIZABLE;
    d.min_w = 46;
    d.min_h = 12;
    d.on_close = on_close;
    s_win = td_win_create(&d);
    if (s_win == nullptr) {
        return;
    }

    s_heading = td_label(s_win, 0, 0, 0, "");
    s_list = td_list(s_win, td_rect(0, 1, -1, -4), list_item, on_choose, nullptr);
    td_scrollbar(s_win, -1, 1, -4, s_list);
    (void)td_button(s_win, 0, -3, "Contents", on_contents, nullptr);
    s_foot = td_label(s_win, 0, -1, 0, "");
    td_widget_set_color(s_foot, td_theme()->dim, TD_COLOR_DEFAULT);

    load_index();
    show_contents();
    if (s_chapter_count == 0) {
        say("No manual on the card: put chapters in /root/manual");
    }
}

const td_app_t s_app = {"TinyHelp", launch, "?"};

} // namespace

extern "C" void td_help_register(void)
{
    (void)td_app_register(&s_app);
}
