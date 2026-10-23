/* TinyTang -- the desktop's wall clock, kept from the I2C RTC on the USB-A port.
 *
 * The RTC (an Adafruit DS3231 at 0x68) holds UTC; the time zone is the
 * desktop's own setting (/tinydesk_tz).  The RTC is reached through the desktop
 * core's I2C engine over the UART link, which the desk layer also uses, so the
 * clock is not read on every call: it is synced once, kept as an offset from
 * the BL616's millisecond timer, and resynced when a core is loaded and about
 * hourly.  With no RTC reachable it keeps the last good sync, and with none at
 * all it reports "not set" rather than invent a date.
 *
 * tang_clock.c is the logic and touches no hardware; the hooks at the end are
 * the hardware, supplied by tang_i2c.cpp on the board and by the host test.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef TANG_CLOCK_H
#define TANG_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TANG_CLOCK_RTC_ADDR   0x68u     /* DS3231 */
#define TANG_CLOCK_RESYNC_MS  (60u * 60u * 1000u)
#define TANG_CLOCK_RETRY_MS   (60u * 1000u)

/* The result of one sync. */
typedef enum {
    TANG_CLOCK_SYNCED = 0,
    TANG_CLOCK_NO_BUS,       /* the loaded core has no I2C bus, or a host owns the link */
    TANG_CLOCK_NO_DEVICE,    /* the read was refused (nack or timeout) */
    TANG_CLOCK_RTC_INVALID,  /* OSF set, or the registers are not a date */
} tang_clock_sync_t;

/* Seconds since 1970-01-01 UTC; false while the clock has never been set. */
bool tang_clock_now(int64_t *utc);
/* Set the clock: the RTC when one is reachable (OSF cleared), and the running
 * offset.  False, with nothing changed, when an RTC is reachable but refuses
 * the write; with no RTC the running clock alone is set. */
bool tang_clock_set(int64_t utc);
/* Read the RTC now. */
tang_clock_sync_t tang_clock_sync(void);
/* A core was loaded or replaced: sync on the next call. */
void tang_clock_core_changed(void);

/* DS3231 time registers 0x00..0x06 to UTC seconds and back.  Years 2000-2199
 * (the month register's century bit); 12-hour mode is read, 24-hour written;
 * the day of week is ISO, Monday 1 to Sunday 7. */
bool tang_clock_from_regs(const uint8_t r[7], int64_t *utc);
bool tang_clock_to_regs(int64_t utc, uint8_t r[7]);

/* ---- the hardware ---- */
/* True when the loaded core has the I2C bus and the link is free. */
bool tang_clock_hw_ready(void);
bool tang_clock_hw_read(uint8_t reg, uint8_t *buf, unsigned n);
bool tang_clock_hw_write(uint8_t reg, const uint8_t *buf, unsigned n);
uint64_t tang_clock_hw_ms(void);

#ifdef __cplusplus
}
#endif

#endif
