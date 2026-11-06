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

/* ---- the card: the cartridge, plus the save files beside it ----
 *
 * Two kinds of file, because the loader touches two: the cartridge it streams
 * and the .sav it reads and writes.  The cartridge is the one `make_file` and
 * `pattern` build; the saves are a tiny in-memory card of their own, with the
 * FatFS call that a real one would answer differently spelled out where it is
 * -- an open of a file that is not there, a rename onto an existing name, a
 * write that a full card stops. */
static const char FILE_PATH[] = "/sd/root/media/snes/game.sfc";
static uint8_t *file_data;
static uint32_t file_len;
static uint32_t file_at;
static bool file_open_now;
static long read_fail_at = -1;
static long write_fail_at = -1;
static FRESULT rename_rc = FR_OK;

#define SAV_MAX 4
struct sav_file {
    char path[192];
    uint8_t *data;
    uint32_t len;
    uint32_t cap;
    uint32_t at;
    bool open;
    bool write_mode;
};
static struct sav_file s_saves[SAV_MAX];
static int s_save_count;
static int s_open_saves;

static void saves_forget(void)
{
    for (int i = 0; i < s_save_count; i++) {
        free(s_saves[i].data);
        memset(&s_saves[i], 0, sizeof s_saves[i]);
    }
    s_save_count = 0;
    s_open_saves = 0;
}

static struct sav_file *save_find(const char *path)
{
    for (int i = 0; i < s_save_count; i++) {
        if (strcmp(s_saves[i].path, path) == 0) {
            return &s_saves[i];
        }
    }
    return NULL;
}

/* A save the test puts on the card before anything runs. */
static void save_put(const char *path, const uint8_t *bytes, uint32_t len)
{
    struct sav_file *f = save_find(path);
    if (f == NULL) {
        if (s_save_count >= SAV_MAX) {
            fprintf(stderr, "tb: too many saves\n");
            exit(2);
        }
        f = &s_saves[s_save_count++];
        memset(f, 0, sizeof *f);
        snprintf(f->path, sizeof f->path, "%s", path);
    }
    free(f->data);
    f->data = malloc(len ? len : 1);
    memcpy(f->data, bytes, len);
    f->len = len;
    f->cap = len;
}

/* What is on the card under `path`, or -1. */
static const struct sav_file *save_get(const char *path) { return save_find(path); }

FRESULT f_open(FIL *fp, const char *path, uint8_t mode)
{
    if (strcmp(path, FILE_PATH) == 0 && file_data != NULL && (mode & FA_READ) != 0) {
        fp->fd = 0;
        file_at = 0;
        file_open_now = true;
        return FR_OK;
    }
    if (strcmp(path, FILE_PATH) == 0) {
        return FR_NO_FILE;
    }
    struct sav_file *f = save_find(path);
    if ((mode & FA_CREATE_ALWAYS) != 0) {
        if (f == NULL) {
            if (s_save_count >= SAV_MAX) {
                return FR_TOO_MANY_OPEN_FILES;
            }
            f = &s_saves[s_save_count++];
            memset(f, 0, sizeof *f);
            snprintf(f->path, sizeof f->path, "%s", path);
        }
        free(f->data);
        f->data = NULL;
        f->len = 0;
        f->cap = 0;
        f->at = 0;
    } else if (f == NULL) {
        return FR_NO_FILE;
    } else {
        f->at = 0;
    }
    f->open = true;
    f->write_mode = (mode & FA_WRITE) != 0;
    s_open_saves++;
    fp->fd = 1 + (int)(f - s_saves);
    return FR_OK;
}

FRESULT f_read(FIL *fp, void *buf, UINT n, UINT *got)
{
    if (fp->fd != 0) {
        struct sav_file *f = &s_saves[fp->fd - 1];
        if (write_fail_at >= 0 && (long)f->at <= write_fail_at &&
            write_fail_at < (long)(f->at + n)) {
            *got = 0;
            return FR_DISK_ERR;
        }
        const uint32_t left = f->at < f->len ? f->len - f->at : 0;
        const uint32_t take = n < left ? n : left;
        memcpy(buf, f->data + f->at, take);
        f->at += take;
        *got = take;
        return FR_OK;
    }
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

FRESULT f_write(FIL *fp, const void *buf, UINT n, UINT *wrote)
{
    struct sav_file *f = &s_saves[fp->fd - 1];
    if (write_fail_at >= 0 && (long)f->at <= write_fail_at &&
        write_fail_at < (long)(f->at + n)) {
        *wrote = 0;
        return FR_DISK_ERR;
    }
    if (f->at + n > f->cap) {
        const uint32_t cap = f->at + n + 1024u;
        uint8_t *grown = realloc(f->data, cap);
        if (grown == NULL) {
            *wrote = 0;
            return FR_DISK_ERR;
        }
        if (f->cap < f->at) {
            memset(grown + f->cap, 0, f->at - f->cap);
        }
        f->data = grown;
        f->cap = cap;
    }
    memcpy(f->data + f->at, buf, n);
    f->at += n;
    if (f->at > f->len) {
        f->len = f->at;
    }
    *wrote = n;
    return FR_OK;
}

FRESULT f_close(FIL *fp)
{
    if (fp->fd == 0) {
        file_open_now = false;
        return FR_OK;
    }
    struct sav_file *f = &s_saves[fp->fd - 1];
    f->open = false;
    if (s_open_saves > 0) {
        s_open_saves--;
    }
    return FR_OK;
}

FSIZE_t f_size(FIL *fp)
{
    return fp->fd == 0 ? file_len : s_saves[fp->fd - 1].len;
}

FRESULT f_lseek(FIL *fp, FSIZE_t ofs)
{
    if (fp->fd == 0) {
        file_at = ofs;
    } else {
        s_saves[fp->fd - 1].at = ofs;
    }
    return FR_OK;
}

FRESULT f_unlink(const char *path)
{
    struct sav_file *f = save_find(path);
    if (f == NULL) {
        return FR_NO_FILE;
    }
    free(f->data);
    const int i = (int)(f - s_saves);
    for (int k = i; k + 1 < s_save_count; k++) {
        s_saves[k] = s_saves[k + 1];
    }
    s_save_count--;
    return FR_OK;
}

FRESULT f_rename(const char *from, const char *to)
{
    struct sav_file *f = save_find(from);
    if (f == NULL) {
        return FR_NO_FILE;
    }
    if (rename_rc != FR_OK) {
        return rename_rc;
    }
    if (save_find(to) != NULL) {
        return FR_EXIST;             /* FatFS will not rename onto a name */
    }
    snprintf(f->path, sizeof f->path, "%s", to);
    return FR_OK;
}

FRESULT f_mkdir(const char *path) { (void)path; return FR_OK; }
FRESULT f_sync(FIL *fp) { (void)fp; return FR_OK; }

/* CRC-16/CCITT-FALSE, the core's polynomial, over the reply's type and data. */
static uint16_t crc16_step(uint16_t crc, uint8_t b)
{
    uint16_t v = (uint16_t)(crc ^ (uint16_t)((uint16_t)b << 8));
    for (int i = 0; i < 8; i++) {
        v = (v & 0x8000u) ? (uint16_t)((v << 1) ^ 0x1021u) : (uint16_t)(v << 1);
    }
    return v;
}

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
static int wait_calls;
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

/* The cartridge array the core holds, and the 0x19 replies served from it.
 * `refuse_reads` is the core's own refusal -- a reply with no data bytes,
 * which is what it sends when it is not held in reset. */
static uint8_t core_sram[TANG_SNES_SAVE_MAX];
static bool refuse_reads;
static uint8_t save_reply[TANG_SNES_SAVE_BLOCK + 2];
static int save_reply_len;
static bool save_reply_pending;

/* Every 0x18 block the loader sends, decoded, so a test can check the address
 * and the bytes rather than only the frame count. */
#define FRAME_SAVE_READ  0x19u
#define SAVE_WRITES_MAX  1024
struct save_write { uint32_t addr; uint32_t len; uint8_t data[TANG_SNES_SAVE_BLOCK]; };
static struct save_write save_writes[SAVE_WRITES_MAX];
static int save_write_count;

/* The saves log: every line a save leaves for the Saves page to read. */
#define NOTES_MAX 64
static char notes[NOTES_MAX][192];
static int note_count;
void tang_ae350_note(const char *text)
{
    if (note_count < NOTES_MAX) {
        snprintf(notes[note_count], sizeof notes[note_count], "%s", text);
        note_count++;
    }
}

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
    if (type == 0x18 && length >= 3) {
        if (save_write_count < SAVE_WRITES_MAX) {
            struct save_write *w = &save_writes[save_write_count++];
            w->addr = ((uint32_t)payload[0] << 16) | ((uint32_t)payload[1] << 8) | payload[2];
            w->len = (uint32_t)(length - 3);
            if (w->len > TANG_SNES_SAVE_BLOCK) {
                w->len = TANG_SNES_SAVE_BLOCK;
            }
            memcpy(w->data, payload + 3, w->len);
        }
    }
    if (type == 0x19 && length == 4) {
        const uint32_t at = ((uint32_t)payload[0] << 16) |
                            ((uint32_t)payload[1] << 8) | payload[2];
        const uint32_t n = payload[3];
        save_reply_len = 0;
        save_reply_pending = true;
        if (!refuse_reads) {
            uint16_t crc = 0xFFFFu;
            crc = crc16_step(crc, 0x19u);
            for (uint32_t i = 0; i < n && at + i < sizeof core_sram; i++) {
                const uint8_t b = core_sram[at + i];
                save_reply[i] = b;
                crc = crc16_step(crc, b);
            }
            save_reply[n] = (uint8_t)(crc >> 8);
            save_reply[n + 1] = (uint8_t)crc;
            save_reply_len = (int)(n + 2u);
        }
    }
    if (reenter_probe && frame_count == 2) {
        reenter_probe = false;
        reenter_rc = tang_snes_load(FILE_PATH, reenter_why, sizeof reenter_why);
    }
    return 0;
}

int tang_fpga_wait(uint8_t want_type, uint8_t *out, size_t cap, uint32_t timeout_ms)
{
    wait_calls++;
    if (want_type != 0x19 || !save_reply_pending) {
        now_ms += timeout_ms;          /* nothing came back */
        return -1;
    }
    if ((size_t)save_reply_len > cap) {
        return -1;
    }
    memcpy(out, save_reply, (size_t)save_reply_len);
    const int len = save_reply_len;
    save_reply_len = 0;
    save_reply_pending = false;
    return len;
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
    saves_forget();
    refuse_reads = false;
    save_reply_len = 0;
    save_reply_pending = false;
    save_write_count = 0;
    note_count = 0;
    wait_calls = 0;
    write_fail_at = -1;
    rename_rc = FR_OK;
    memset(core_sram, 0, sizeof core_sram);
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


/* ---- the battery save ---- */

/* Sizing, from the header alone. */
static void test_save_size(void)
{
    uint8_t h[64];
    memset(h, 0, sizeof h);

    /* Chip nibble first: only 2, 5, 6, 9 and A carry a battery. */
    const uint8_t chips[] = { 0x00, 0x01, 0x03, 0x04, 0x07, 0x08, 0x0B, 0x0F };
    for (size_t i = 0; i < sizeof chips; i++) {
        h[0x16] = chips[i];
        h[0x18] = 3;
        CHECK(tang_snes_save_size(h, 0) == 0u);
    }
    const uint8_t battery[] = { 0x02, 0x05, 0x06, 0x09, 0x0A };
    for (size_t i = 0; i < sizeof battery; i++) {
        h[0x16] = battery[i];
        h[0x18] = 3;
        CHECK(tang_snes_save_size(h, 0) == 8u * 1024u);
    }

    /* 0x18 is a power of two in KiB. */
    h[0x16] = 0x02;
    h[0x18] = 0;  CHECK(tang_snes_save_size(h, 0) == 1024u);
    h[0x18] = 5;  CHECK(tang_snes_save_size(h, 0) == 32u * 1024u);
    h[0x18] = 7;  CHECK(tang_snes_save_size(h, 0) == 128u * 1024u);
    /* Past the core's whole window, and past what a header can claim. */
    h[0x18] = 8;  CHECK(tang_snes_save_size(h, 0) == 128u * 1024u);
    h[0x18] = 15; CHECK(tang_snes_save_size(h, 0) == 128u * 1024u);

    /* A SuperFX cartridge leaves 0x18 zero and puts the size in the expansion
     * byte, three bytes before the header.  Yoshi's Island claims 32 KiB. */
    h[0x16] = 0x05; h[0x18] = 0;
    CHECK(tang_snes_save_size(h, 5) == 32u * 1024u);
    CHECK(tang_snes_save_size(h, 0) == 1024u);       /* nothing to go on */

    /* The high nibble of 0x16 is not the chip and must not confuse it. */
    h[0x16] = 0xF2; h[0x18] = 4;
    CHECK(tang_snes_save_size(h, 0) == 16u * 1024u);
}

static void test_save_path(void)
{
    char out[256];
    CHECK(tang_snes_save_path("/sd/root/media/snes/S/Zelda.sfc", out, sizeof out) == 0);
    CHECK(strcmp(out, "/sd/root/saves/snes/Zelda.sav") == 0);
    /* The stem keeps the dots a title may carry. */
    CHECK(tang_snes_save_path("/sd/root/media/snes/#/S.W.A.T..sfc", out, sizeof out) == 0);
    CHECK(strcmp(out, "/sd/root/saves/snes/S.W.A.T..sav") == 0);
    /* A name with no extension still names a save. */
    CHECK(tang_snes_save_path("/x/rom", out, sizeof out) == 0);
    CHECK(strcmp(out, "/sd/root/saves/snes/rom.sav") == 0);
    /* Nothing to name it after. */
    CHECK(tang_snes_save_path("/", out, sizeof out) != 0);
    CHECK(tang_snes_save_path(NULL, out, sizeof out) != 0);
}

/* Every frame after the ROM drain and before the release is a 0x18 block. */
static int count_type(uint8_t type)
{
    int n = 0;
    for (int i = 0; i < frame_count; i++) {
        if (frames[i].type == type) {
            n++;
        }
    }
    return n;
}

static void test_save_restore(void)
{
    static const char SAV[] = "/sd/root/saves/snes/game.sav";
    char why[96];

    /* ---- a cartridge with a battery and a save on the card ---- */
    reset_world();
    make_file(256u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    file_data[0x7FC0 + 0x16] = 0x02;          /* ROM + RAM + battery */
    file_data[0x7FC0 + 0x18] = 3;             /* 8 KiB */
    uint8_t old[8 * 1024];
    for (size_t i = 0; i < sizeof old; i++) {
        old[i] = (uint8_t)(i * 7u + 3u);
    }
    save_put(SAV, old, sizeof old);

    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) == 0);
    CHECK(strstr(why, "save restored") != NULL);

    /* The blocks went out between the drain and the release, in order, with
     * the address in the first three bytes and the file's bytes after. */
    int first = -1, last = -1;
    for (int i = 0; i < frame_count; i++) {
        if (frames[i].type == 0x18) {
            if (first < 0) { first = i; }
            last = i;
        }
    }
    CHECK(first > 0 && last > 0);
    const int blocks = (int)((sizeof old + TANG_SNES_SAVE_BLOCK - 1) / TANG_SNES_SAVE_BLOCK);
    CHECK(count_type(0x18) == blocks);
    /* The release frame is the last one, after the save blocks. */
    CHECK(frames[frame_count - 1].type == FPGA_CMD_SET_LOAD &&
          frames[frame_count - 1].first == 0);
    CHECK(last == frame_count - 2);
    CHECK(first > 2);
    /* Address and data of the first and last block. */
    CHECK(save_writes[0].addr == 0u);
    CHECK(memcmp(save_writes[0].data, old, TANG_SNES_SAVE_BLOCK) == 0);
    CHECK(save_writes[blocks - 1].addr == (uint32_t)(blocks - 1) * TANG_SNES_SAVE_BLOCK);
    const uint32_t tail = (uint32_t)sizeof old - (uint32_t)(blocks - 1) * TANG_SNES_SAVE_BLOCK;
    CHECK(save_writes[blocks - 1].len == tail);
    CHECK(memcmp(save_writes[blocks - 1].data,
                 old + (blocks - 1) * TANG_SNES_SAVE_BLOCK, tail) == 0);
    /* The restore did not disturb the load's own order. */
    CHECK(frames[0].type == FPGA_CMD_SET_LOAD && frames[0].first == 1);
    CHECK(frames[1].type == FPGA_CMD_ROM_DATA && frames[1].len == TANG_SNES_HEADER_LEN);
    CHECK(frames[1].at_ms >= frames[0].at_ms + 50);
    CHECK(!locked && lock_errors == 0);
    CHECK(!file_open_now);

    /* ---- a save shorter than the cartridge's SRAM is padded with 0xFF ---- */
    reset_world();
    make_file(128u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    file_data[0x7FC0 + 0x16] = 0x02;
    file_data[0x7FC0 + 0x18] = 5;             /* 32 KiB */
    save_put(SAV, old, 512);                  /* 512 bytes only */
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) == 0);
    uint32_t seen = 0;
    bool padded = true;
    for (int i = 0; i < save_write_count; i++) {
        for (uint32_t k = 0; k < save_writes[i].len; k++) {
            const uint8_t want = seen < 512u ? old[seen] : 0xFF;
            if (save_writes[i].data[k] != want) {
                padded = false;
            }
            seen++;
        }
    }
    CHECK(seen == 32u * 1024u);
    CHECK(padded);

    /* ---- a cartridge with no battery sends nothing ---- */
    reset_world();
    make_file(128u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);                /* 0x16 stays 0xFF */
    CHECK(tang_snes_load(FILE_PATH, why, sizeof why) == 0);
    CHECK(count_type(0x18) == 0);
    CHECK(strstr(why, "save restored") == NULL);
    CHECK(frames[frame_count - 1].type == FPGA_CMD_SET_LOAD);
}

/* The readout: the core answers 0x19 blocks and the file is written, verified
 * and put in place. */
static void test_save_write(void)
{
    static const char SAV[] = "/sd/root/saves/snes/game.sav";
    char why[128];

    reset_world();
    for (size_t i = 0; i < sizeof core_sram; i++) {
        core_sram[i] = (uint8_t)(i * 13u + 5u);
    }
    char path[256];
    CHECK(tang_snes_save_path(FILE_PATH, path, sizeof path) == 0);
    CHECK(strcmp(path, SAV) == 0);

    CHECK(tang_snes_save_write(path, 8u * 1024u, why, sizeof why) == 0);
    CHECK(strstr(why, "saved") != NULL);
    /* The attempt is in the log the Saves page reads: an arm line and a
     * finish line, in that order, naming the file. */
    CHECK(note_count == 2);
    if (note_count == 2) {
        CHECK(strncmp(notes[0], "arm ", 4) == 0);
        CHECK(strstr(notes[0], SAV) != NULL);
        CHECK(strncmp(notes[1], "finish ", 7) == 0);
        CHECK(strstr(notes[1], SAV) != NULL);
        CHECK(strstr(notes[1], "verified") != NULL);
    }
    /* One hold, then one 0x19 per block, and no release: the caller loads the
     * desktop core under it. */
    CHECK(frames[0].type == FPGA_CMD_SET_LOAD && frames[0].first == 1);
    const int read_blocks = (int)((8u * 1024u + TANG_SNES_SAVE_BLOCK - 1) / TANG_SNES_SAVE_BLOCK);
    CHECK(count_type(FRAME_SAVE_READ) == read_blocks);
    CHECK(count_type(FPGA_CMD_SET_LOAD) == 1);
    CHECK(wait_calls == count_type(FRAME_SAVE_READ));
    const struct sav_file *f = save_get(SAV);
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(f->len == 8u * 1024u);
        CHECK(memcmp(f->data, core_sram, 8u * 1024u) == 0);
    }
    /* Nothing left behind. */
    char part[sizeof path + 8];
    snprintf(part, sizeof part, "%s.part", path);
    CHECK(save_get(part) == NULL);
    CHECK(!locked && lock_errors == 0);

    /* A previous save is replaced, not merged. */
    uint8_t previous[8 * 1024];
    memset(previous, 0xAA, sizeof previous);
    save_put(SAV, previous, sizeof previous);
    reset_world();
    for (size_t i = 0; i < sizeof core_sram; i++) {
        core_sram[i] = (uint8_t)(255u - i);
    }
    CHECK(tang_snes_save_write(path, 8u * 1024u, why, sizeof why) == 0);
    f = save_get(SAV);
    CHECK(f != NULL && f->len == 8u * 1024u);
    if (f != NULL) {
        CHECK(memcmp(f->data, core_sram, 8u * 1024u) == 0);
    }
}

/* Everything that must not cost the save that is already there. */
static void test_save_write_refusals(void)
{
    static const char SAV[] = "/sd/root/saves/snes/game.sav";
    char why[128];
    char path[256];
    CHECK(tang_snes_save_path(FILE_PATH, path, sizeof path) == 0);

    /* The core is not in reset, so it answers with no data bytes. */
    reset_world();
    save_put(SAV, (const uint8_t *)"keep", 4);
    refuse_reads = true;
    CHECK(tang_snes_save_write(path, 8192u, why, sizeof why) != 0);
    CHECK(strstr(why, "refused") != NULL);
    /* A refusal is logged as a finish with the failed verdict, so a save that
     * never landed still leaves a trace for the Saves page. */
    CHECK(note_count == 2 && strstr(notes[1], "failed") != NULL);
    const struct sav_file *f = save_get(SAV);
    CHECK(f != NULL && f->len == 4 && memcmp(f->data, "keep", 4) == 0);
    char part[sizeof path + 8];
    snprintf(part, sizeof part, "%s.part", path);
    CHECK(save_get(part) == NULL);

    /* The card stops taking bytes part-way. */
    reset_world();
    save_put(SAV, (const uint8_t *)"keep", 4);
    write_fail_at = 5000;
    CHECK(tang_snes_save_write(path, 8192u, why, sizeof why) != 0);
    f = save_get(SAV);
    CHECK(f != NULL && f->len == 4 && memcmp(f->data, "keep", 4) == 0);
    CHECK(save_get(part) == NULL);

    /* A size no cartridge could have. */
    reset_world();
    CHECK(tang_snes_save_write(path, 0u, why, sizeof why) != 0);
    CHECK(tang_snes_save_write(path, TANG_SNES_SAVE_MAX + 1u, why, sizeof why) != 0);
    CHECK(frame_count == 0);

    /* The host owns the link. */
    reset_world();
    pipe_open = true;
    CHECK(tang_snes_save_write(path, 8192u, why, sizeof why) != 0);
    CHECK(strstr(why, "fpgapipe") != NULL);
    CHECK(frame_count == 0);
}

/* The cartridge's own save, worked out from its file on the way out. */
static void test_save_for(void)
{
    char path[256];
    uint32_t size = 0;

    reset_world();
    make_file(256u * 1024u);
    pattern(0);
    put_header(0x7FC0u, 0x20);
    file_data[0x7FC0 + 0x16] = 0x02;
    file_data[0x7FC0 + 0x18] = 3;
    save_put("/sd/root/saves/snes/game.sav", (const uint8_t *)"x", 1);
    CHECK(tang_snes_save_for(FILE_PATH, &size, path, sizeof path) == 0);
    CHECK(size == 8u * 1024u);
    CHECK(strcmp(path, "/sd/root/saves/snes/game.sav") == 0);

    /* No battery: nothing to save, and no frame is sent looking for one. */
    reset_world();
    make_file(256u * 1024u);
    put_header(0x7FC0u, 0x20);
    CHECK(tang_snes_save_for(FILE_PATH, &size, path, sizeof path) != 0);
    CHECK(frame_count == 0);

    /* A file that is not there. */
    reset_world();
    CHECK(tang_snes_save_for(FILE_PATH, &size, path, sizeof path) != 0);
}

/* The gesture: the loader resumes the game it held. */
static void test_resume(void)
{
    reset_world();
    tang_snes_resume();
    CHECK(frame_count == 1);
    CHECK(frames[0].type == FPGA_CMD_SET_LOAD && frames[0].first == 0);
    CHECK(!locked && lock_errors == 0);
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
    test_save_size();
    test_save_path();
    test_save_restore();
    test_save_write();
    test_save_write_refusals();
    test_save_for();
    test_resume();
    reset_world();
    free(rom_seen);
    if (failures != 0) {
        fprintf(stderr, "tb_snes_load: %d check(s) failed\n", failures);
        return 1;
    }
    printf("tb_snes_load: all checks passed\n");
    return 0;
}
