/* SPDX-License-Identifier: MIT
 * TinyDesk's Editor at TinyTang's limits, TD_EDITOR_MAX=8192 and
 * TD_EDITOR_UNDO=2048 (CMakeLists.txt).  Upstream's own Editor tests are
 * included and run as they are, except test_history_limits, which pastes
 * 10,001 bytes and so assumes the 16 KB default buffer; the checks below take
 * its place at the 8 KB boundary.
 */
#define main upstream_test_editor_main
#include "../../third_party/tinydesk/tests/test_editor.c"
#undef main

#if TD_EDITOR_MAX != 8192 || TD_EDITOR_UNDO != 2048
#error "built without TinyTang's Editor limits"
#endif

static char s_text[TD_EDITOR_MAX + 2];

static const char *filled(int n)
{
    memset(s_text, 'a', (size_t)n);
    s_text[n] = '\0';
    return s_text;
}

/* A file of exactly TD_EDITOR_MAX bytes is editable; one byte more is
 * read-only and stays as it was. */
static void test_size_limit(void)
{
    open_file(filled(TD_EDITOR_MAX - 1));
    CHECK(!status_contains("read-only"));
    type("b");
    ctrl('s');
    CHECK_EQ(s_file_len, TD_EDITOR_MAX);

    open_file(filled(TD_EDITOR_MAX));
    CHECK(!status_contains("read-only"));
    type("c");                         /* past the buffer: refused */
    CHECK(status_has("The file is full"));
    CHECK(!changed());

    open_file(filled(TD_EDITOR_MAX + 1));
    CHECK(status_has("read-only"));
    type("d");
    ctrl('s');
    CHECK_EQ(s_writes, 0);
    CHECK_EQ(s_file_len, TD_EDITOR_MAX + 1);
}

/* A paste that would take the file past 8 KB is refused whole. */
static void test_paste_past_limit(void)
{
    open_file(filled(TD_EDITOR_MAX - 100));
    static char big[201];
    memset(big, 'x', sizeof(big) - 1);
    td_clipboard_set(big, (int)sizeof(big) - 1);
    ctrl('v');
    CHECK(status_has("The file is full"));
    CHECK(!changed());
}

/* Undo keeps 2 KB of text: a 1 KB paste undoes, one over 2 KB stays but
 * cannot be undone. */
static void test_undo_limit(void)
{
    static char kb[1025];
    memset(kb, 'y', sizeof(kb) - 1);
    open_file("");
    td_clipboard_set(kb, (int)sizeof(kb) - 1);
    ctrl('v');
    ctrl('z');
    CHECK(!changed());

    static char over[TD_EDITOR_UNDO + 2];
    memset(over, 'z', sizeof(over) - 1);
    open_file("");
    td_clipboard_set(over, (int)sizeof(over) - 1);
    ctrl('v');
    ctrl('z');
    CHECK(status_has("Nothing to undo"));
    CHECK(changed());
    ctrl('s');
    CHECK_EQ(s_file_len, (int)sizeof(over) - 1);
}

int main(void)
{
    td_set_sysinfo(&s_info);
    td_init(&s_hal);   /* no size answer: 80x25 */
    test_undo_by_word();
    test_changed_follows_the_save_point();
    test_save_twice();
    test_new_edit_ends_redo();
    test_delete_runs();
    test_typing_over_a_selection();
    test_cut_copy_paste();
    test_paste_from_the_terminal();
    (void)test_history_limits;         /* assumes 16 KB; replaced below */
    test_close_asks_about_changes();
    test_size_limit();
    test_paste_past_limit();
    test_undo_limit();
    return TD_TEST_RESULT();
}
