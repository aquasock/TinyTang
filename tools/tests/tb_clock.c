/* SPDX-License-Identifier: MIT
 *
 * The desktop's wall clock (ports/bl616/tang_clock.c) on the host.  The four
 * hardware hooks are a model of a DS3231 behind the core's I2C engine and a
 * millisecond counter, so the real conversion and sync logic runs: the
 * register encoding against dates Python's datetime computed independently,
 * the "never set" case, the sync and its hourly resync, the retry backoff, a
 * core change, the OSF flag, and both halves of a manual set.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tang_clock.h"

static int s_failed, s_checks;
#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        s_checks++;                                                            \
        if (!(cond)) {                                                         \
            s_failed++;                                                        \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                        \
            printf(__VA_ARGS__);                                               \
            printf("\n");                                                      \
        }                                                                      \
    } while (0)

/* ---- the model ---- */
static uint8_t s_regs[0x13];
static bool s_ready;
static bool s_read_fails;
static bool s_write_fails;
static uint64_t s_ms;
static int s_reads;

bool tang_clock_hw_ready(void) { return s_ready; }

bool tang_clock_hw_read(uint8_t reg, uint8_t *buf, unsigned n)
{
    s_reads++;
    if (s_read_fails || reg + n > sizeof(s_regs)) {
        return false;
    }
    memcpy(buf, &s_regs[reg], n);
    return true;
}

bool tang_clock_hw_write(uint8_t reg, const uint8_t *buf, unsigned n)
{
    if (s_write_fails || reg + n > sizeof(s_regs)) {
        return false;
    }
    memcpy(&s_regs[reg], buf, n);
    return true;
}

uint64_t tang_clock_hw_ms(void) { return s_ms; }

static void rtc_holds(const uint8_t r[7], uint8_t status)
{
    memcpy(s_regs, r, 7);
    s_regs[0x0f] = status;
}

/* ---- the cases ---- */

struct known {
    const char *what;
    int64_t utc;
    uint8_t regs[7];   /* sec min hour day date month|century year */
};

/* UTC seconds and ISO weekdays from Python's datetime. */
static const struct known s_known[] = {
    {"2000-01-01 00:00:00", 946684800, {0x00, 0x00, 0x00, 6, 0x01, 0x01, 0x00}},
    {"2000-02-29 23:59:59", 951868799, {0x59, 0x59, 0x23, 2, 0x29, 0x02, 0x00}},
    {"2026-10-18 19:01:09", 1792350069, {0x09, 0x01, 0x19, 7, 0x18, 0x10, 0x26}},
    {"2099-12-31 23:59:59", 4102444799, {0x59, 0x59, 0x23, 4, 0x31, 0x12, 0x99}},
    {"2100-03-01 12:00:00", 4107585600, {0x00, 0x00, 0x12, 1, 0x01, 0x83, 0x00}},
    {"2199-12-31 23:59:59", 7258118399, {0x59, 0x59, 0x23, 2, 0x31, 0x92, 0x99}},
};

static void test_encoding(void)
{
    for (size_t i = 0; i < sizeof(s_known) / sizeof(s_known[0]); i++) {
        const struct known *k = &s_known[i];
        int64_t utc = 0;
        CHECK(tang_clock_from_regs(k->regs, &utc) && utc == k->utc,
              "%s read as %lld", k->what, (long long)utc);
        uint8_t r[7];
        CHECK(tang_clock_to_regs(k->utc, r) && memcmp(r, k->regs, 7) == 0,
              "%s written as %02x %02x %02x %02x %02x %02x %02x", k->what,
              r[0], r[1], r[2], r[3], r[4], r[5], r[6]);
    }
    /* Every day across a leap year and a century: written, read back. */
    for (int64_t t = 946684800 - 86400LL * 400; t < 4107585600 + 86400LL * 400; t += 86400 + 3601) {
        uint8_t r[7];
        int64_t back = 0;
        if (t < 946684800) {
            CHECK(!tang_clock_to_regs(t, r), "%lld before 2000 accepted", (long long)t);
            continue;
        }
        CHECK(tang_clock_to_regs(t, r) && tang_clock_from_regs(r, &back) && back == t,
              "%lld round trip gave %lld", (long long)t, (long long)back);
    }
    /* 12-hour mode: 12 AM is 0, 12 PM is 12, 7 PM is 19. */
    const uint8_t am12[7] = {0x00, 0x00, 0x40 | 0x12, 1, 0x01, 0x01, 0x00};
    const uint8_t pm12[7] = {0x00, 0x00, 0x60 | 0x12, 1, 0x01, 0x01, 0x00};
    const uint8_t pm7[7]  = {0x00, 0x00, 0x60 | 0x07, 1, 0x01, 0x01, 0x00};
    int64_t a = 0, b = 0, c = 0;
    CHECK(tang_clock_from_regs(am12, &a) && a == 946684800, "12 AM read as %lld", (long long)a);
    CHECK(tang_clock_from_regs(pm12, &b) && b == 946684800 + 12 * 3600, "12 PM read as %lld", (long long)b);
    CHECK(tang_clock_from_regs(pm7, &c) && c == 946684800 + 19 * 3600, "7 PM read as %lld", (long long)c);
    /* Registers that are not a date. */
    const uint8_t bad_digit[7] = {0x5a, 0x00, 0x00, 1, 0x01, 0x01, 0x00};
    const uint8_t feb30[7]     = {0x00, 0x00, 0x00, 1, 0x30, 0x02, 0x26};
    const uint8_t feb29[7]     = {0x00, 0x00, 0x00, 1, 0x29, 0x02, 0x26};
    const uint8_t month13[7]   = {0x00, 0x00, 0x00, 1, 0x01, 0x13, 0x26};
    const uint8_t hour24[7]    = {0x00, 0x00, 0x24, 1, 0x01, 0x01, 0x26};
    CHECK(!tang_clock_from_regs(bad_digit, &a), "a non-decimal digit accepted");
    CHECK(!tang_clock_from_regs(feb30, &a), "30 February accepted");
    CHECK(!tang_clock_from_regs(feb29, &a), "29 February 2026 accepted");
    CHECK(!tang_clock_from_regs(month13, &a), "month 13 accepted");
    CHECK(!tang_clock_from_regs(hour24, &a), "hour 24 accepted");
}

static void test_clock(void)
{
    int64_t utc = 0;

    /* Boot with no I2C core: never set. */
    s_reads = 0;
    s_ready = false;
    s_ms = 5000;
    CHECK(!tang_clock_now(&utc), "a time with nothing behind it");
    CHECK(s_reads == 0, "the bus was read with no I2C core (%d)", s_reads);

    /* An I2C core arrives: the next call syncs, then counts on its own. */
    rtc_holds(s_known[2].regs, 0x08);
    s_ready = true;
    tang_clock_core_changed();
    CHECK(tang_clock_now(&utc) && utc == 1792350069, "synced to %lld", (long long)utc);
    const int after_sync = s_reads;
    s_ms += 61500;
    CHECK(tang_clock_now(&utc) && utc == 1792350069 + 61, "61.5 s later %lld", (long long)utc);
    CHECK(s_reads == after_sync, "the bus was read between syncs");

    /* An hour on, it resyncs and takes the RTC's word for the time. */
    s_regs[0] = 0x00; s_regs[1] = 0x10; s_regs[2] = 0x20;   /* RTC at 20:10:00 */
    s_ms += TANG_CLOCK_RESYNC_MS;
    CHECK(tang_clock_now(&utc) && utc == 1792350069 - 19 * 3600 - 69 + 20 * 3600 + 600,
          "after the hourly resync %lld", (long long)utc);
    CHECK(s_reads > after_sync, "no hourly resync");

    /* The core goes away: the running clock carries on from the last sync. */
    s_ready = false;
    tang_clock_core_changed();
    const int64_t before = utc;
    s_ms += 10000;
    CHECK(tang_clock_now(&utc) && utc == before + 10, "kept running without the RTC: %lld",
          (long long)utc);

    /* An RTC that does not answer is asked again only after the backoff. */
    s_ready = true;
    s_read_fails = true;
    tang_clock_core_changed();
    int reads = s_reads;
    CHECK(tang_clock_now(&utc) && utc == before + 10, "a refused read changed the time");
    CHECK(s_reads == reads + 1, "the failed sync read %d times", s_reads - reads);
    reads = s_reads;
    s_ms += TANG_CLOCK_RETRY_MS - 1;
    (void)tang_clock_now(&utc);
    CHECK(s_reads == reads, "asked again inside the backoff");
    s_ms += 1;
    s_read_fails = false;
    (void)tang_clock_now(&utc);
    CHECK(s_reads > reads, "not asked again after the backoff");

    /* Manual set with the RTC there: the registers, OSF cleared, the clock. */
    s_regs[0x0f] = 0x88;
    CHECK(tang_clock_set(s_known[4].utc), "the set was refused");
    CHECK(memcmp(s_regs, s_known[4].regs, 7) == 0, "the RTC holds %02x %02x %02x %02x %02x %02x %02x",
          s_regs[0], s_regs[1], s_regs[2], s_regs[3], s_regs[4], s_regs[5], s_regs[6]);
    CHECK(s_regs[0x0f] == 0x08, "status after the set %02x", s_regs[0x0f]);
    CHECK(tang_clock_now(&utc) && utc == s_known[4].utc, "the clock after the set %lld",
          (long long)utc);

    /* An RTC that refuses the write: false, and nothing changes. */
    s_write_fails = true;
    CHECK(!tang_clock_set(s_known[0].utc), "a refused write reported as set");
    CHECK(tang_clock_now(&utc) && utc == s_known[4].utc, "a refused write moved the clock");
    s_write_fails = false;

    /* No RTC: the running clock alone is set. */
    s_ready = false;
    CHECK(tang_clock_set(s_known[1].utc), "a set with no RTC refused");
    CHECK(tang_clock_now(&utc) && utc == s_known[1].utc, "the software set gave %lld",
          (long long)utc);
}

static void test_osf_never_set(void)
{
    /* A fresh boot whose RTC lost its time: OSF set, so never set. */
    int64_t utc = 0;
    rtc_holds(s_known[2].regs, 0x88);
    s_ready = true;
    CHECK(!tang_clock_now(&utc), "a time taken from an RTC with OSF set");
}

int main(void)
{
    test_encoding();
    test_osf_never_set();     /* first: needs the clock still unset */
    test_clock();
    printf("tang_clock: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}
