/* TinyTang — host test for the SNES cartridge loader (ports/bl616/tang_snes_load.c).
 *
 * The loader runs against a fake card holding one file of up to 9 MiB and a
 * fake link that records every frame, whether the lock was held for it, and
 * the fake clock when it went.  What is checked is what the TinySNES core
 * needs, from TinySNES's tools/load_snes_rom.py: the header first, then the
 * image from its own first byte, a pause before reset is released, and the
 * overlay hidden through the OSD module last -- and that nothing at all goes
 * out for a file the core could not use.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "tang_fpga_link.h"
#include "tang_osd.h"
#include "tang_snes_load.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
    failures++; } } while (0)

/* ---- clock and critical sections ---- */
static uint32_t now_ms;
static int critical_depth;
TickType_t xTaskGetTickCount(void) { return (TickType_t)now_ms; }
void vTaskDelay(TickType_t ticks) { now_ms += (uint32_t)ticks; }
void taskYIELD(void) { }
void host_enter_critical(void) { critical_depth++; }
void host_exit_critical(void) { critical_depth--; }
int host_critical_depth(void) { return critical_depth; }

/* ---- the card: one file ---- */
static const char FILE_PATH[] = "/sd/root/media/snes/game.sfc";
static uint8_t *file_data;
static uint32_t file_len;
static uint32_t file_at;
static bool file_open_now;
static long read_fail_at = -1;     /* fail the read that would cover this offset */

FRESULT f_open(FIL *fp, const char *path, uint8_t mode)
{
    (void)mode;
    if (strcmp(path, FILE_PATH) != 0 || file_data == NULL) {
        return FR_NO_FILE;
    }
    fp->fd = 1;
    file_at = 0;
    file_open_now = true;
    return FR_OK;
}

FRESULT f_read(FIL *fp, void *buf, UINT n, UINT *got)
{
    (void)fp;
    if (read_fail_at >= 0 && (long)file_at <= read_fail_at &&
        read_fail_at < (long)(file_at + n)) {
        *got = 0;
        return FR_DISK_ERR;
    }
    const uint32_t left = file_at < file_len ? file_len - file_at : 0;
    const uint32_t take = n < left ? n : left;
    memcpy(buf, file_data + file_at, take);
    file_at += take;
    *got = take;
    return FR_OK;
}

FRESULT f_close(FIL *fp) { (void)fp; file_open_now = false; return FR_OK; }
FSIZE_t f_size(FIL *fp) { (void)fp; return file_len; }
FRESULT f_lseek(FIL *fp, FSIZE_t ofs) { (void)fp; file_at = ofs; return FR_OK; }

/* ---- the link ---- */
#define MAX_FRAMES 12000
struct frame_rec { uint8_t type; uint32_t len; uint8_t first; bool locked; uint32_t at_ms; };
static struct frame_rec frames[MAX_FRAMES];
static int frame_count;
static uint8_t *rom_seen;          /* every 0x07 payload, concatenated */
static uint32_t rom_seen_len;
static bool locked;
static int lock_errors;
static int drains;
static bool pipe_open;
static int link_rc;
static int frame_fail_at = -1;     /* index of the frame that fails */
static int osd_calls;
static bool osd_shown_now = true;  /* the OSD module's record */
static bool osd_first;
static int osd_first_index = -1;   /* frame count at the first overlay set */
static int osd_last_index = -1;    /* frame count at the last overlay set */
static bool reenter_probe;
static int reenter_rc = -2;
static char reenter_why[96];

int tang_fpga_link_open(void) { return link_rc; }
bool tang_fpga_pipe_open(void) { return pipe_open; }
void tang_fpga_lock(void) { if (locked) lock_errors++; locked = true; }
void tang_fpga_unlock(void) { if (!locked) lock_errors++; locked = false; }
void tang_fpga_drain(void) { if (!locked) lock_errors++; drains++; }

int tang_fpga_frame(uint8_t type, const uint8_t *payload, size_t length)
{
    if (frame_count == frame_fail_at) {
        return -1;
    }
    if (frame_count >= MAX_FRAMES) {
        return -1;
    }
    struct frame_rec *f = &frames[frame_count++];
    f->type = type;
    f->len = (uint32_t)length;
    f->first = length > 0 ? payload[0] : 0;
    f->locked = locked;
    f->at_ms = now_ms;
    if (type == FPGA_CMD_ROM_DATA) {
        memcpy(rom_seen + rom_seen_len, payload, length);
        rom_seen_len += (uint32_t)length;
    }
    if (reenter_probe && frame_count == 2) {
        reenter_probe = false;
        reenter_rc = tang_snes_load(FILE_PATH, reenter_why, sizeof reenter_why);
    }
    return 0;
}

int tang_osd_set(bool on)
{
    if (locked) {
        return -1;                 /* the OSD takes the lock itself */
    }
    if (osd_calls == 0) {
        osd_first = on;
        osd_first_index = frame_count;
    }
    osd_calls++;
    osd_shown_now = on;
    osd_last_index = frame_count;
    return 0;
}

bool tang_osd_shown(void) { return osd_shown_now; }

/* ---- images ---- */

static void reset_world(void)
{
    free(file_data);
    file_data = NULL;
    file_len = 0;
    file_at = 0;
    file_open_now = false;
    read_fail_at = -1;
    frame_count = 0;
    rom_seen_len = 0;
    locked = false;
    lock_errors = 0;
    drains = 0;
    pipe_open = false;
    link_rc = 0;
    frame_fail_at = -1;
    osd_calls = 0;
    osd_shown_now = true;
    osd_first = true;
    osd_first_index = -1;
    osd_last_index = -1;
    reenter_probe = false;
    reenter_rc = -2;
    critical_depth = 0;
}

/* A file of `len` bytes of 0xFF, which scores nothing as a header. */
static void make_file(uint32_t len)
{
    file_data = malloc(len ? len : 1);
    file_len = len;
    memset(file_data, 0xFF, len);
}

/* A full-score internal header written at `at` in the file. */
static void put_header(uint32_t at, uint8_t map)
{
    uint8_t *h = file_data + at;
    memset(h, ' ', 0x15);
    memcpy(h, "TEST CART", 9);
    h[0x15] = map;
    h[0x17] = 0x0A;
    h[0x1C] = 0x34; h[0x1D] = 0x12;   /* complement 0x1234 */
    h[0x1E] = 0xCB; h[0x1F] = 0xED;   /* checksum 0xEDCB   */
}

/* Fill the image with a pattern so a byte out of place shows. */
static void pattern(uint32_t from)
{
    for (uint32_t i = from; i < file_len; i++) {
        file_data[i] = (uint8_t)((i * 131u) >> 3);
    }
}

/* The sequence every good load must show, for an image starting at `skip`
 * with its header at file offset `hdr`. */
static void check_good_load(uint32_t skip, uint32_t hdr)
{
    const uint32_t image = file_len - skip;
    CHECK(frame_count >= 4);
    CHECK(frames[0].type == FPGA_CMD_SET_LOAD && frames[0].len == 1 && frames[0].first == 1);
    CHECK(frames[1].type == FPGA_CMD_ROM_DATA && frames[1].len == TANG_SNES_HEADER_LEN);
    CHECK(frames[1].at_ms >= frames[0].at_ms + 50);
    CHECK(rom_seen_len == TANG_SNES_HEADER_LEN + image);
    CHECK(memcmp(rom_seen, file_data + hdr, TANG_SNES_HEADER_LEN) == 0);
    CHECK(memcmp(rom_seen + TANG_SNES_HEADER_LEN, file_data + skip, image) == 0);
    const int expect_data = (int)((image + 1023u) / 1024u);
    CHECK(frame_count == 2 + expect_data + 1);
    for (int i = 2; i < frame_count - 1; i++) {
        CHECK(frames[i].type == FPGA_CMD_ROM_DATA);
        CHECK(frames[i].len <= 1024u);
    }
    const struct frame_rec *last = &frames[frame_count - 1];
    const struct frame_rec *tail = &frames[frame_count - 2];
    CHECK(last->type == FPGA_CMD_SET_LOAD && last->len == 1 && last->first == 0);
    CHECK(last->at_ms >= tail->at_ms + 500);
    for (int i = 0; i < frame_count; i++) {
        CHECK(frames[i].locked);
    }
    CHECK(drains == 1);
    /* Hidden before the core is held, so the bars show through the stream,
     * and hidden again after the release. */
    CHECK(osd_calls == 2 && !osd_first && !osd_shown_now);
    CHECK(osd_first_index == 0);
    CHECK(osd_last_index == frame_count);
    CHECK(!locked && lock_errors == 0);
    CHECK(!file_open_now);
    CHECK(critical_depth == 0);
}

static void check_nothing_sent(void)
{
    CHECK(frame_count == 0);
    CHECK(osd_calls == 0 && osd_shown_now);
    CHECK(!locked && lock_errors == 0);
    CHECK(!file_open_now);
    CHECK(critical_depth == 0);
}

/* ---- tests ---- */

static void test_pure(void)
{
    CHECK(tang_snes_copier_skip(1024u * 1024u) == 0);
    CHECK(tang_snes_copier_skip(1024u * 1024u + 512u) == 512);
    CHECK(tang_snes_copier_skip(513u) == 0);
    CHECK(tang_snes_copier_skip(512u) == 512);

    uint8_t h[64];
    memset(h, 0xFF, sizeof h);
    CHECK(tang_snes_header_score(h) == 0);
    memset(h, ' ', sizeof h);
    memcpy(h, "TEST CART", 9);
    h[0x15] = 0x21;  h[0x17] = 0x0A;
    h[0x1C] = 0x34;  h[0x1D] = 0x12;  h[0x1E] = 0xCB;  h[0x1F] = 0xED;
    CHECK(tang_snes_header_score(h) == 7);
    h[0x1E] = 0xCC;                           /* checksum off by one */
    CHECK(tang_snes_header_score(h) == 4);
    h[0x1C] = 0xFF; h[0x1D] = 0xFF; h[0x1E] = 0; h[0x1F] = 0;   /* zero checksum */
    CHECK(tang_snes_header_score(h) == 4);
    h[0x15] = 0x24;                           /* map mode 4 is not one */
    CHECK(tang_snes_header_score(h) == 2);
    h[0x15] = 0x35;                           /* ExHiROM is */
    CHECK(tang_snes_header_score(h) == 4);
    h[0x17] = 0x0E;                           /* past 0x0D */
    CHECK(tang_snes_header_score(h) == 3);
    h[0x17] = 0x00;
    CHECK(tang_snes_header_score(h) == 3);
    h[0x14] = 0x7F;                           /* title byte not printable */
    CHECK(tang_snes_header_score(h) == 2);

    CHECK(tang_snes_pick_header(7, 7) == TANG_SNES_LOROM_HDR);
    CHECK(tang_snes_pick_header(3, 5) == TANG_SNES_HIROM_HDR);
    CHECK(tang_snes_pick_header(-1, 4) == TANG_SNES_HIROM_HDR);
    CHECK(tang_snes_pick_header(2, -1) == TANG_SNES_LOROM_HDR);
    CHECK(tang_snes_pick_header(0, 0) == 0);
    CHECK(tang_snes_pick_header(-1, -1) == 0);
    CHECK(tang_snes_pick_header(-1, 0) == 0);
}

static void test_lorom(void)
{
    reset_world();
    make_file(256u * 1024u);
    pattern(0);
    put_header(0x7FC0, 0x20);
    char why[96];
    now_ms = 0;
    const int rc = tang_snes_load(FILE_PATH, why, sizeof why);
    CHECK(rc == 0);
    CHECK(strstr(why, "LoROM") != NULL && strstr(why, "262144") != NULL);
    check_good_load(0, 0x7FC0);
}

/* HiROM with a copier header, and an image whose last chunk is short. */
static void test_hirom_copier(void)
{
    reset_world();
    make_file(512u + 1024u * 1024u);
    pattern(0);
    put_header(512u + 0xFFC0u, 0x21);
    char why[96];
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) == 0);
    CHECK(strstr(why, "HiROM") != NULL && strstr(why, "copier") != NULL);
    check_good_load(512, 512u + 0xFFC0u);

    reset_world();
    make_file(0x10000u + 300u);
    pattern(0);
    put_header(0xFFC0u, 0x21);
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) == 0);
    CHECK(strstr(why, "HiROM") != NULL && strstr(why, "copier") == NULL);
    check_good_load(0, 0xFFC0u);
    CHECK(frames[frame_count - 2].len == 300u);
}

/* A LoROM header that HiROM's slot also half-matches stays LoROM. */
static void test_tie_is_lorom(void)
{
    reset_world();
    make_file(128u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    put_header(0xFFC0u, 0x21);
    char why[96];
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) == 0);
    check_good_load(0, 0x7FC0u);
}

static void test_refusals(void)
{
    char why[96];

    reset_world();                       /* no such file */
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "cannot open") != NULL);
    check_nothing_sent();

    reset_world();                       /* no header anywhere */
    make_file(512u * 1024u);
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "no SNES header") != NULL);
    check_nothing_sent();

    reset_world();                       /* too short for either header */
    make_file(1000u);
    pattern(0);
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "no SNES header") != NULL);
    check_nothing_sent();

    reset_world();                       /* only the copier header */
    make_file(512u);
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "empty") != NULL);
    check_nothing_sent();

    reset_world();                       /* more than the core maps */
    make_file(TANG_SNES_MAX_IMAGE + 1024u);
    put_header(0x7FC0u, 0x20);
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "8 MiB") != NULL);
    check_nothing_sent();

    reset_world();                       /* exactly 8 MiB loads */
    make_file(TANG_SNES_MAX_IMAGE);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) == 0);
    check_good_load(0, 0x7FC0u);

    reset_world();                       /* the host's pipe has the link */
    make_file(64u * 1024u);
    put_header(0x7FC0u, 0x20);
    pipe_open = true;
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "fpgapipe") != NULL);
    check_nothing_sent();

    reset_world();                       /* no UART */
    make_file(64u * 1024u);
    put_header(0x7FC0u, 0x20);
    link_rc = -1;
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "UART1") != NULL);
    check_nothing_sent();
}

/* The overlay was hidden first and is shown again: two calls, ending where
 * it started. */
static bool restored(void)
{
    return osd_calls == 2 && !osd_first && osd_first_index == 0 && osd_shown_now;
}

/* A failure part-way leaves the core in reset: no release, the overlay put
 * back as it was, the lock given back, the file closed. */
static void test_mid_failures(void)
{
    char why[96];

    reset_world();
    make_file(64u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    read_fail_at = 20000;
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "card read failed at 19456") != NULL);
    CHECK(frames[frame_count - 1].type == FPGA_CMD_ROM_DATA);
    CHECK(restored() && !locked && lock_errors == 0 && !file_open_now);

    reset_world();
    make_file(64u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    frame_fail_at = 5;                   /* the fourth data chunk */
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "stopped taking data at 3072") != NULL);
    CHECK(restored() && !locked && lock_errors == 0 && !file_open_now);

    reset_world();
    make_file(64u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    frame_fail_at = 1;                   /* the header */
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "header") != NULL);
    CHECK(frame_count == 1 && restored() && !locked && lock_errors == 0);

    reset_world();
    make_file(64u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    frame_fail_at = 2 + 64;              /* the release */
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "did not start") != NULL);
    CHECK(restored() && !locked && lock_errors == 0);

    /* Hidden before the load stays hidden after a failed one. */
    reset_world();
    make_file(64u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    osd_shown_now = false;
    frame_fail_at = 0;                   /* the hold */
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) != 0);
    CHECK(strstr(why, "cannot start") != NULL);
    CHECK(osd_calls == 2 && !osd_shown_now && frame_count == 0);
}

/* A second load while one is streaming is refused, and the first finishes;
 * after it, a load is accepted again. */
static void test_busy(void)
{
    reset_world();
    make_file(32u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    reenter_probe = true;
    char why[96];
    /* The probe runs from inside the outer load's second frame.  The nested
     * call is refused before it opens anything, so it never touches the lock
     * or the file the outer load holds. */
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) == 0);
    CHECK(reenter_rc == 1);
    CHECK(strstr(reenter_why, "another cartridge") != NULL);
    check_good_load(0, 0x7FC0u);

    reset_world();
    make_file(32u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) == 0);
}

int main(void)
{
    rom_seen = malloc(TANG_SNES_MAX_IMAGE + 4096u);
    test_pure();
    test_lorom();
    test_hirom_copier();
    test_tie_is_lorom();
    test_refusals();
    test_mid_failures();
    test_busy();
    reset_world();
    free(rom_seen);
    if (failures != 0) {
        fprintf(stderr, "tb_snes_load: %d check(s) failed\n", failures);
        return 1;
    }
    printf("tb_snes_load: all checks passed\n");
    return 0;
}
