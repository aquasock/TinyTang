/* TinyTang — host test for the save-log parser (ports/bl616/td_saves_log.c).
 *
 * The lines below are the exact shapes ports/bl616/tang_fpga_uart.c writes,
 * stamps and all.  What matters is that a window can answer "when did this
 * cartridge last write, and did it take" from a log that spans sessions, cards
 * and images.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "td_saves_log.h"

static int failures;

#define CHECK(x)                                                              \
    do {                                                                      \
        if (!(x)) {                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                \
            failures++;                                                       \
        }                                                                     \
    } while (0)

#define STATE_NEEDLE "/cart.state"
#define SAV_NEEDLE   "/cart.sav"

/* Feed a whole buffer through one reader, as one chunk. */
static void feed_all(td_saves_attempt_t *rec, const char *needle, const char *log)
{
    td_saves_reader_t reader;
    td_saves_reader_init(&reader);
    td_saves_reader_feed(&reader, log, strlen(log), needle, rec);
}

/* Feed the same buffer in two halves, which is what an f_read() boundary does
 * to a line. */
static void feed_split(td_saves_attempt_t *rec, const char *needle, const char *log,
                       size_t cut)
{
    td_saves_reader_t reader;
    td_saves_reader_init(&reader);
    td_saves_reader_feed(&reader, log, cut, needle, rec);
    td_saves_reader_feed(&reader, log + cut, strlen(log) - cut, needle, rec);
}

static void test_when(void)
{
    char out[32];

    td_saves_when("2026-10-24T20:41:05-07:00", out, sizeof out);
    CHECK(strcmp(out, "2026-10-24 20:41") == 0);
    td_saves_when("no-clock", out, sizeof out);
    CHECK(strcmp(out, "no-clock") == 0);
    td_saves_when("", out, sizeof out);
    CHECK(strcmp(out, "unknown") == 0);
    td_saves_when("odd", out, sizeof out);
    CHECK(strcmp(out, "odd") == 0);
}

static void test_finish(void)
{
    td_saves_attempt_t rec = {};
    feed_all(&rec, STATE_NEEDLE,
             "2026-10-24T20:41:05-07:00 finish /sd/root/states/nes/cart.state: "
             "262144 of 262144 bytes, crc 1234abcd of 1234abcd, whole\n");
    CHECK(!rec.armed);
    CHECK(strcmp(rec.verdict, "whole") == 0);
    CHECK(rec.got == 262144 && rec.want == 262144);
    CHECK(strcmp(rec.stamp, "2026-10-24T20:41:05-07:00") == 0);

    /* A save that came back short: the size is the evidence. */
    td_saves_attempt_t shortrec = {};
    feed_all(&shortrec, STATE_NEEDLE,
             "2026-10-24T20:42:00-07:00 finish /sd/root/states/nes/cart.state: "
             "8 of 262144 bytes, crc deadbeef of 1234abcd, short\n");
    CHECK(strcmp(shortrec.verdict, "short") == 0);
    CHECK(shortrec.got == 8 && shortrec.want == 262144);
}

static void test_arm_and_cancel(void)
{
    /* Armed and never finished: the page says so rather than showing a save. */
    td_saves_attempt_t rec = {};
    feed_all(&rec, STATE_NEEDLE,
             "2026-10-24T20:40:00-07:00 arm /sd/root/states/nes/cart.state: "
             "262144 bytes, crc 1234abcd, whole\n");
    CHECK(rec.armed);
    CHECK(rec.verdict[0] == '\0');
    CHECK(strcmp(rec.stamp, "2026-10-24T20:40:00-07:00") == 0);

    /* A newer attempt supersedes the old verdict. */
    td_saves_attempt_t again = {};
    feed_all(&again, STATE_NEEDLE,
             "2026-10-24T20:41:05-07:00 finish /sd/root/states/nes/cart.state: "
             "262144 of 262144 bytes, crc 1234abcd of 1234abcd, whole\n"
             "2026-10-24T20:50:00-07:00 arm /sd/root/states/nes/cart.state: "
             "262144 bytes, crc 1234abcd, whole\n");
    CHECK(again.armed);
    CHECK(again.verdict[0] == '\0');
    CHECK(strcmp(again.stamp, "2026-10-24T20:50:00-07:00") == 0);
}

static void test_two_files(void)
{
    /* Both of a cartridge's files in one log, for different cartridges too. */
    static const char log[] =
        "2026-10-24T19:00:00-07:00 arm /sd/root/saves/nes/other.sav: 8192 bytes, crc 1, whole\n"
        "2026-10-24T19:00:01-07:00 finish /sd/root/saves/nes/other.sav: 8192 of 8192 bytes, crc 1 of 1, whole\n"
        "2026-10-24T20:00:00-07:00 finish /sd/root/saves/nes/cart.sav: 8192 of 8192 bytes, crc 2 of 2, whole\n"
        "2026-10-24T20:41:05-07:00 finish /sd/root/states/nes/cart.state: 262144 of 262144 bytes, crc 3 of 3, whole\n"
        "2026-10-24T20:42:00-07:00 finish /sd/root/saves/nes/other.sav: 4096 of 8192 bytes, crc 4 of 1, short\n";
    td_saves_attempt_t state = {};
    td_saves_attempt_t batt = {};
    feed_all(&state, STATE_NEEDLE, log);
    feed_all(&batt, SAV_NEEDLE, log);

    CHECK(strcmp(state.verdict, "whole") == 0);
    CHECK(strcmp(state.stamp, "2026-10-24T20:41:05-07:00") == 0);
    CHECK(state.want == 262144);
    /* The battery save is the 20:00 line, not the newer one for another ROM. */
    CHECK(strcmp(batt.verdict, "whole") == 0);
    CHECK(strcmp(batt.stamp, "2026-10-24T20:00:00-07:00") == 0);
    CHECK(batt.want == 8192);
}

static void test_split_line(void)
{
    /* The reader is fed 256 bytes at a time; a line straddles the boundary. */
    static const char log[] =
        "2026-10-24T20:00:00-07:00 finish /sd/root/saves/nes/cart.sav: 8192 of 8192 bytes, crc 2 of 2, whole\n"
        "2026-10-24T20:41:05-07:00 finish /sd/root/states/nes/cart.state: 262144 of 262144 bytes, crc 3 of 3, whole\n";
    td_saves_attempt_t rec = {};
    feed_split(&rec, STATE_NEEDLE, log, 120);
    CHECK(strcmp(rec.verdict, "whole") == 0);
    CHECK(rec.want == 262144);
    CHECK(strcmp(rec.stamp, "2026-10-24T20:41:05-07:00") == 0);

    /* And split exactly inside the path, where a naive scan would lose it. */
    td_saves_attempt_t mid = {};
    feed_split(&mid, STATE_NEEDLE, log, 100);
    CHECK(strcmp(mid.verdict, "whole") == 0);
    CHECK(strcmp(mid.stamp, "2026-10-24T20:41:05-07:00") == 0);
}

static void test_no_clock_and_legacy(void)
{
    /* A session before the RTC had been read: the word, not a date. */
    td_saves_attempt_t rec = {};
    feed_all(&rec, STATE_NEEDLE,
             "no-clock finish /sd/root/states/nes/cart.state: 262144 of 262144 "
             "bytes, crc 3 of 3, whole\n");
    CHECK(strcmp(rec.verdict, "whole") == 0);
    CHECK(strcmp(rec.stamp, "no-clock") == 0);

    /* A line from before the stamps existed is still read, with no time. */
    td_saves_attempt_t old = {};
    feed_all(&old, STATE_NEEDLE,
             "finish /sd/root/states/nes/cart.state: 262144 of 262144 bytes, "
             "crc 3 of 3, whole\n");
    CHECK(strcmp(old.verdict, "whole") == 0);
    CHECK(old.stamp[0] == '\0');
}

static void test_ignores_other_files(void)
{
    /* Nothing for this cartridge: an empty record, so the page can say so. */
    td_saves_attempt_t rec = {};
    feed_all(&rec, STATE_NEEDLE,
             "2026-10-24T20:00:00-07:00 finish /sd/root/saves/nes/cart.sav: 8192 of 8192 bytes, crc 2 of 2, whole\n"
             "2026-10-24T20:01:00-07:00 finish /sd/root/states/nes/cartridge.state: 16 of 16 bytes, crc 9 of 9, whole\n");
    CHECK(!rec.armed);
    CHECK(rec.verdict[0] == '\0');
    CHECK(rec.stamp[0] == '\0');

    /* A CRLF log is read the same way. */
    td_saves_attempt_t crlf = {};
    feed_all(&crlf, STATE_NEEDLE,
             "2026-10-24T20:41:05-07:00 finish /sd/root/states/nes/cart.state: 262144 of 262144 bytes, crc 3 of 3, whole\r\n");
    CHECK(strcmp(crlf.verdict, "whole") == 0);
}

static void test_overlong_line(void)
{
    /* An absurd line is truncated, not split into a second one. */
    char log[512];
    int n = snprintf(log, sizeof log, "2026-10-24T20:41:05-07:00 finish /sd/root/states/nes/cart.state: ");
    for (int i = n; i < 400 && i < (int)sizeof log - 2; i++) {
        log[i] = 'x';
    }
    log[400] = '\n';
    log[401] = '\0';
    td_saves_attempt_t rec = {};
    feed_all(&rec, STATE_NEEDLE, log);
    /* Recognised as an attempt, with no size parsed and no verdict. */
    CHECK(rec.armed == false);
    CHECK(rec.want == 0);
}

int main(void)
{
    test_when();
    test_finish();
    test_arm_and_cancel();
    test_two_files();
    test_split_line();
    test_no_clock_and_legacy();
    test_ignores_other_files();
    test_overlong_line();

    if (failures) {
        printf("tb_saves_log: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_saves_log: all checks passed\n");
    return 0;
}
