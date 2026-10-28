/* TinyTang -- the desktop's wall clock, kept from the I2C RTC on the USB-A port.
 * See tang_clock.h for what it is for; this file is the logic only.
 *
 * SPDX-License-Identifier: MIT
 */
#include "tang_clock.h"

#include <stddef.h>
#include <stdlib.h>

#ifdef TANG_CLOCK_HOST
#define CLOCK_LOCK()   do { } while (0)
#define CLOCK_UNLOCK() do { } while (0)
#else
#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
/* Only the state is copied inside; the bus is never touched here (TOOL-015). */
#define CLOCK_LOCK()   taskENTER_CRITICAL()
#define CLOCK_UNLOCK() taskEXIT_CRITICAL()
#endif

#define DS3231_STATUS     0x0fu
#define DS3231_STATUS_OSF 0x80u

/* ------------------------------------------------------------ the calendar */

static int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m > 2 ? m - 3 : m + 9) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static void civil_from_days(int64_t z, int64_t *y, unsigned *m, unsigned *d)
{
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const unsigned mp = (5u * doy + 2u) / 153u;
    *d = doy - (153u * mp + 2u) / 5u + 1u;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = (int64_t)yoe + era * 400 + (*m <= 2);
}

static unsigned days_in_month(int64_t y, unsigned m)
{
    static const unsigned char n[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return m == 2 && leap ? 29u : n[m - 1];
}

/* A BCD byte to its value, or -1 when a digit is not decimal. */
static int from_bcd(uint8_t v)
{
    if ((v & 0x0f) > 9 || (v >> 4) > 9) {
        return -1;
    }
    return (v >> 4) * 10 + (v & 0x0f);
}

static uint8_t to_bcd(unsigned v)
{
    return (uint8_t)(((v / 10u) << 4) | (v % 10u));
}

bool tang_clock_from_regs(const uint8_t r[7], int64_t *utc)
{
    const int sec = from_bcd(r[0] & 0x7f);
    const int min = from_bcd(r[1] & 0x7f);
    int hour;
    if (r[2] & 0x40) {                         /* 12-hour mode */
        hour = from_bcd(r[2] & 0x1f);
        if (hour < 1 || hour > 12) {
            return false;
        }
        hour = hour % 12 + ((r[2] & 0x20) ? 12 : 0);
    } else {
        hour = from_bcd(r[2] & 0x3f);
    }
    const int date = from_bcd(r[4] & 0x3f);
    const int month = from_bcd(r[5] & 0x1f);
    const int year = from_bcd(r[6]);
    if (sec < 0 || sec > 59 || min < 0 || min > 59 || hour < 0 || hour > 23 ||
        month < 1 || month > 12 || year < 0) {
        return false;
    }
    const int64_t y = 2000 + year + ((r[5] & 0x80) ? 100 : 0);
    if (date < 1 || (unsigned)date > days_in_month(y, (unsigned)month)) {
        return false;
    }
    *utc = days_from_civil(y, (unsigned)month, (unsigned)date) * 86400 +
           hour * 3600 + min * 60 + sec;
    return true;
}

bool tang_clock_to_regs(int64_t utc, uint8_t r[7])
{
    const int64_t first = days_from_civil(2000, 1, 1) * 86400;
    const int64_t end = days_from_civil(2200, 1, 1) * 86400;
    if (utc < first || utc >= end) {
        return false;
    }
    const int64_t days = utc / 86400;
    const unsigned secs = (unsigned)(utc % 86400);
    int64_t y;
    unsigned m, d;
    civil_from_days(days, &y, &m, &d);
    const unsigned yy = (unsigned)(y - 2000);
    r[0] = to_bcd(secs % 60u);
    r[1] = to_bcd(secs / 60u % 60u);
    r[2] = to_bcd(secs / 3600u);               /* bit 6 clear: 24-hour mode */
    r[3] = (uint8_t)((days % 7 + 7 + 3) % 7 + 1); /* 1970-01-01 was a Thursday, 4 */
    r[4] = to_bcd(d);
    r[5] = (uint8_t)(to_bcd(m) | (yy >= 100 ? 0x80u : 0u));
    r[6] = to_bcd(yy % 100u);
    return true;
}

/* --------------------------------------------------------------- the clock */

static bool s_set;             /* the clock holds a time */
static bool s_from_rtc;        /* ... and it came from, or went to, the RTC */
static int64_t s_base_ms;      /* UTC in ms minus tang_clock_hw_ms() */
static bool s_sync_due = true;
static uint64_t s_last_sync_ms;
static uint64_t s_next_try_ms;

static void adopt(int64_t utc, uint64_t now_ms, bool from_rtc)
{
    CLOCK_LOCK();
    s_base_ms = utc * 1000 - (int64_t)now_ms;
    s_set = true;
    s_from_rtc = from_rtc;
    if (from_rtc) {
        s_sync_due = false;
        s_last_sync_ms = now_ms;
    }
    CLOCK_UNLOCK();
}

tang_clock_sync_t tang_clock_sync(void)
{
    if (!tang_clock_hw_ready()) {
        return TANG_CLOCK_NO_BUS;
    }
    uint8_t r[7];
    uint8_t status;
    if (!tang_clock_hw_read(0x00, r, sizeof(r)) ||
        !tang_clock_hw_read(DS3231_STATUS, &status, 1)) {
        return TANG_CLOCK_NO_DEVICE;
    }
    int64_t utc;
    if ((status & DS3231_STATUS_OSF) || !tang_clock_from_regs(r, &utc)) {
        return TANG_CLOCK_RTC_INVALID;
    }
    adopt(utc, tang_clock_hw_ms(), true);
    return TANG_CLOCK_SYNCED;
}

void tang_clock_core_changed(void)
{
    CLOCK_LOCK();
    s_sync_due = true;
    s_next_try_ms = 0;
    CLOCK_UNLOCK();
}

bool tang_clock_now(int64_t *utc)
{
    const uint64_t now_ms = tang_clock_hw_ms();
    CLOCK_LOCK();
    const bool due = s_sync_due ||
                     (s_from_rtc && now_ms - s_last_sync_ms >= TANG_CLOCK_RESYNC_MS);
    const bool may_try = now_ms >= s_next_try_ms;
    CLOCK_UNLOCK();

    if (due && may_try) {
        const tang_clock_sync_t got = tang_clock_sync();
        if (got != TANG_CLOCK_SYNCED) {
            CLOCK_LOCK();
            s_sync_due = true;
            /* An absent bus costs nothing to ask about again; an RTC that
             * refused or held no date is not asked again for a minute. */
            if (got != TANG_CLOCK_NO_BUS) {
                s_next_try_ms = now_ms + TANG_CLOCK_RETRY_MS;
            }
            CLOCK_UNLOCK();
        }
    }

    CLOCK_LOCK();
    const bool set = s_set;
    const int64_t ms = s_base_ms + (int64_t)tang_clock_hw_ms();
    CLOCK_UNLOCK();
    if (!set) {
        return false;
    }
    *utc = ms >= 0 ? ms / 1000 : -((-ms + 999) / 1000);
    return true;
}

/* ----------------------------------------------------------- the zone ----
 *
 * /tinydesk_tz holds the zone as seconds east of UTC, which is what the
 * desktop's own Date & time page writes and what the shell's tangi2c and the
 * AE350 save log's time stamps already read.  It is read once: it changes only
 * when a user changes it, and every time frame wants it.  A missing or
 * unreadable file leaves 0, which is UTC -- the same answer the other two
 * readers give. */

long tang_clock_zone(void)
{
    static long zone;
    static bool known;

    if (known) {
        return zone;
    }
    known = true;
#ifndef TANG_CLOCK_HOST
    {
        FIL file;
        char buf[16];
        UINT got = 0;

        if (f_open(&file, "/sd/tinydesk_tz", FA_READ) == FR_OK) {
            if (f_read(&file, buf, sizeof buf - 1, &got) == FR_OK && got > 0) {
                buf[got] = '\0';
                const long v = strtol(buf, NULL, 10);
                /* A zone is under a day either way; anything else is a bad
                 * file. */
                if (v > -86400 && v < 86400) {
                    zone = v;
                }
            }
            (void)f_close(&file);
        }
    }
#endif
    return zone;
}

bool tang_clock_set(int64_t utc)
{
    if (tang_clock_hw_ready()) {
        uint8_t r[7];
        uint8_t status;
        if (!tang_clock_to_regs(utc, r) ||
            !tang_clock_hw_write(0x00, r, sizeof(r)) ||
            !tang_clock_hw_read(DS3231_STATUS, &status, 1)) {
            return false;
        }
        status &= (uint8_t)~DS3231_STATUS_OSF;
        if (!tang_clock_hw_write(DS3231_STATUS, &status, 1)) {
            return false;
        }
        adopt(utc, tang_clock_hw_ms(), true);
        return true;
    }
    adopt(utc, tang_clock_hw_ms(), false);
    return true;
}
