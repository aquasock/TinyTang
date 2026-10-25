/* TinyTang — the save log's parser (td_saves_log.h), hardware-free.
 *
 * SPDX-License-Identifier: MIT
 */

#include "td_saves_log.h"

#include <stdio.h>
#include <string.h>

/* "2026-10-23T21:40:12-07:00", or the firmware's stand-in for a clock that has
 * not been set.  Anything else is not a stamp. */
static bool stamp_like(const char *token, size_t len)
{
    if (len == 8 && memcmp(token, "no-clock", 8) == 0) {
        return true;
    }
    if (len < 20 || len >= TD_SAVES_STAMP_MAX) {
        return false;
    }
    return token[4] == '-' && token[7] == '-' && token[10] == 'T' &&
           token[13] == ':';
}

/* Split one line into its stamp and its body. */
static void split(const char *line, char *stamp, size_t stamp_cap,
                  const char **body)
{
    const char *space = strchr(line, ' ');
    stamp[0] = '\0';
    *body = line;
    if (space == NULL || space == line) {
        return;
    }
    const size_t n = (size_t)(space - line);
    if (n >= stamp_cap || !stamp_like(line, n)) {
        return;
    }
    memcpy(stamp, line, n);
    stamp[n] = '\0';
    *body = space + 1;
}

static void note(td_saves_attempt_t *rec, const char *line)
{
    char stamp[TD_SAVES_STAMP_MAX];
    const char *body = line;

    split(line, stamp, sizeof stamp, &body);

    if (strncmp(body, "arm ", 4) == 0) {
        /* A newer attempt: whatever the last one said is now history. */
        memset(rec, 0, sizeof *rec);
        rec->armed = true;
        snprintf(rec->stamp, sizeof rec->stamp, "%s", stamp);
        return;
    }
    if (strncmp(body, "finish ", 7) == 0) {
        const char *colon = strchr(body, ':');
        unsigned long crc_got = 0;
        unsigned long crc_want = 0;

        memset(rec, 0, sizeof *rec);
        snprintf(rec->stamp, sizeof rec->stamp, "%s", stamp);
        if (colon != NULL) {
            (void)sscanf(colon + 1, " %lu of %lu bytes, crc %lx of %lx, %15s",
                         &rec->got, &rec->want, &crc_got, &crc_want,
                         rec->verdict);
        }
    }
}

void td_saves_reader_init(td_saves_reader_t *reader)
{
    reader->used = 0;
    reader->line[0] = '\0';
}

void td_saves_reader_feed(td_saves_reader_t *reader, const char *data, size_t len,
                          const char *needle, td_saves_attempt_t *rec)
{
    for (size_t i = 0; i < len; i++) {
        const char c = data[i];
        if (c == '\n' || c == '\r') {
            if (reader->used > 0) {
                reader->line[reader->used] = '\0';
                if (needle == NULL || strstr(reader->line, needle) != NULL) {
                    note(rec, reader->line);
                }
                reader->used = 0;
            }
            continue;
        }
        /* An overlong line is truncated rather than split in two: its tail
         * cannot name a destination its head did not. */
        if (reader->used + 1 < sizeof reader->line) {
            reader->line[reader->used++] = c;
        }
    }
}

void td_saves_when(const char *stamp, char *out, size_t cap)
{
    const size_t n = stamp != NULL ? strlen(stamp) : 0;

    if (n == 0) {
        snprintf(out, cap, "unknown");
        return;
    }
    if (strcmp(stamp, "no-clock") == 0 || n < 19) {
        snprintf(out, cap, "%s", stamp);
        return;
    }
    snprintf(out, cap, "%.10s %.5s", stamp, stamp + 11);
}
