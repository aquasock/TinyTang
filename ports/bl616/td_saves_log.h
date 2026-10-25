/* TinyTang — reading /sd/root/saves/last.log, the board's own record of saves.
 *
 * The firmware stamps every line it appends with the local ISO-8601 time from
 * the RTC-kept clock (ports/bl616/tang_fpga_uart.c, ae350_stamp), so a window
 * can say when a save was written, what came of it and how many bytes it
 * carried — which FatFS cannot, because the board's files carry no real dates.
 *
 * Two windows show this: Tiny89's Saves page and TinyNES's.  The parsing lives
 * here once so the two cannot drift.
 *
 * A line is "<stamp> <body>", where body is one of
 *   arm <path>: <bytes> bytes, crc <crc>, <word>
 *   finish <path>: <got> of <want> bytes, crc <a> of <b>, <verdict>
 * A line written before the stamps existed has no leading stamp; it is read
 * whole as its body and its time shows as unknown rather than being dropped.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef TD_SAVES_LOG_H
#define TD_SAVES_LOG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TD_SAVES_STAMP_MAX   32
#define TD_SAVES_VERDICT_MAX 16
#define TD_SAVES_LINE_MAX    192

/* What the log last said about one destination file.  A run arms a channel and
 * later finishes it, and the lines of different channels interleave, so each
 * destination keeps its own record. */
typedef struct {
    bool armed;                     /* an "arm" line was the last word */
    char stamp[TD_SAVES_STAMP_MAX]; /* as the firmware wrote it; "" if it wrote none */
    char verdict[TD_SAVES_VERDICT_MAX];
    unsigned long got;
    unsigned long want;
} td_saves_attempt_t;

/* Reads the log a chunk at a time, keeping a partial line between calls, so a
 * line split across two reads is still one line. */
typedef struct {
    char line[TD_SAVES_LINE_MAX];
    size_t used;
} td_saves_reader_t;

void td_saves_reader_init(td_saves_reader_t *reader);

/* Fold `len` more bytes of the log into `rec`, counting only the lines that
 * name a destination whose path ends in `needle` (e.g. "/cart.state"). */
void td_saves_reader_feed(td_saves_reader_t *reader, const char *data, size_t len,
                          const char *needle, td_saves_attempt_t *rec);

/* The stamp as a page shows it — "2026-10-23T21:40:12-07:00" becomes
 * "2026-10-23 21:40" — with "no-clock" and "unknown" left as they are, because
 * a session without a set clock must not read as a date. */
void td_saves_when(const char *stamp, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
