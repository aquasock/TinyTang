// TinyTang — stream a SNES cartridge from the card into the TinySNES core.
//
// The sequence is TinySNES's tools/load_snes_rom.py, which is what the core
// was proven against on the board; see tang_snes_load.h.  The frames go out
// under the link lock a chunk at a time, with the lock dropped between
// chunks as nesload does, so the desk layer and the console stay alive for
// the few seconds a cartridge takes.  The desk's own frames may land between
// two chunks: the core's frame decoder is typed, so only 0x07 feeds the ROM.
//
// SPDX-License-Identifier: MIT

#include "tang_snes_load.h"

#include <stdbool.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "tang_fpga_link.h"
#include "tang_osd.h"

#define SNES_CHUNK 1024u

/* Before the header goes, the core gets a moment to open its ROM window, as
 * the host loader gives it. */
#define SNES_OPEN_MS    50u
/* The last frames are still moving into SDRAM when the BL616 stops writing;
 * releasing reset in the same breath truncates the image (TinySNES's
 * load_snes_rom.py found this on the board). */
#define SNES_DRAIN_MS   500u
/* After release, before the overlay is hidden again: the host loader's order. */
#define SNES_START_MS   100u

static uint8_t s_chunk[SNES_CHUNK];
static bool s_busy;

uint32_t tang_snes_copier_skip(uint32_t file_size)
{
    return (file_size % 1024u) == 512u ? 512u : 0u;
}

int tang_snes_header_score(const uint8_t h[TANG_SNES_HEADER_LEN])
{
    int score = 0;
    const unsigned checksum = (unsigned)h[0x1E] | ((unsigned)h[0x1F] << 8);
    const unsigned complement = (unsigned)h[0x1C] | ((unsigned)h[0x1D] << 8);
    if ((checksum ^ complement) == 0xFFFFu && checksum != 0u) {
        score += 3;
    }
    const unsigned map = h[0x15] & 0x0Fu;
    if (map == 0u || map == 1u || map == 2u || map == 3u || map == 5u) {
        score += 2;
    }
    const unsigned rom_size = h[0x17] & 0x0Fu;
    if (rom_size >= 1u && rom_size <= 0x0Du) {
        score += 1;
    }
    bool printable = true;
    for (unsigned i = 0x10; i < 0x15; i++) {
        if (h[i] < 0x20u || h[i] > 0x7Eu) {
            printable = false;
        }
    }
    if (printable) {
        score += 1;
    }
    return score;
}

uint32_t tang_snes_pick_header(int lorom_score, int hirom_score)
{
    if (hirom_score > 0 && hirom_score > lorom_score) {
        return TANG_SNES_HIROM_HDR;
    }
    if (lorom_score > 0) {
        return TANG_SNES_LOROM_HDR;
    }
    return 0u;
}

/* The candidate at `at` in the image, scored, or -1 when the image is too
 * short to hold it or the card will not give it. */
static int score_at(FIL *file, uint32_t skip, uint32_t image, uint32_t at,
                    uint8_t out[TANG_SNES_HEADER_LEN])
{
    if (image < at + TANG_SNES_HEADER_LEN) {
        return -1;
    }
    UINT got = 0;
    if (f_lseek(file, (FSIZE_t)(skip + at)) != FR_OK ||
        f_read(file, out, TANG_SNES_HEADER_LEN, &got) != FR_OK ||
        got != TANG_SNES_HEADER_LEN) {
        return -1;
    }
    return tang_snes_header_score(out);
}

static int send_locked(uint8_t type, const uint8_t *payload, size_t length)
{
    tang_fpga_lock();
    const int rc = tang_fpga_frame(type, payload, length);
    tang_fpga_unlock();
    return rc;
}

/* A load that stopped part-way: the core stays in reset, and the overlay is
 * put back as it was before the load hid it. */
static int fail(bool was_shown)
{
    (void)tang_osd_set(was_shown);
    return 1;
}

static int load(FIL *file, char *why, size_t why_len)
{
    const uint32_t size = (uint32_t)f_size(file);
    const uint32_t skip = tang_snes_copier_skip(size);
    const uint32_t image = size - skip;
    if (image == 0u) {
        snprintf(why, why_len, "the file is empty");
        return 1;
    }
    if (image > TANG_SNES_MAX_IMAGE) {
        snprintf(why, why_len, "%u bytes is more than the core maps (8 MiB)",
                 (unsigned)image);
        return 1;
    }

    uint8_t lo[TANG_SNES_HEADER_LEN];
    uint8_t hi[TANG_SNES_HEADER_LEN];
    const int lo_score = score_at(file, skip, image, TANG_SNES_LOROM_HDR, lo);
    const int hi_score = score_at(file, skip, image, TANG_SNES_HIROM_HDR, hi);
    const uint32_t at = tang_snes_pick_header(lo_score, hi_score);
    if (at == 0u) {
        snprintf(why, why_len, "no SNES header at 0x7FC0 or 0xFFC0");
        return 1;
    }
    const uint8_t *header = at == TANG_SNES_HIROM_HDR ? hi : lo;

    if (tang_fpga_link_open() != 0) {
        snprintf(why, why_len, "cannot bring up UART1");
        return 1;
    }
    if (tang_fpga_pipe_open()) {
        snprintf(why, why_len, "the host's fpgapipe owns the link");
        return 1;
    }
    if (f_lseek(file, (FSIZE_t)skip) != FR_OK) {
        snprintf(why, why_len, "cannot rewind the file");
        return 1;
    }

    /* The overlay goes first, so the stream runs over the core's colour bars,
     * as TinyNES's and TinyGBC's loads do, rather than under the desk.  Through
     * the OSD module, so its record stays true and the desk's re-arm after a
     * core load restores "hidden".  A load that fails from here on puts the
     * overlay back the way it was found. */
    const bool was_shown = tang_osd_shown();
    if (tang_osd_set(false) != 0) {
        snprintf(why, why_len, "cannot clear the overlay");
        return 1;
    }

    /* Non-zero holds the core in reset with its ROM window open. */
    tang_fpga_lock();
    tang_fpga_drain();
    const int opened = tang_fpga_frame(FPGA_CMD_SET_LOAD, (const uint8_t[]){ 1 }, 1);
    tang_fpga_unlock();
    if (opened != 0) {
        snprintf(why, why_len, "cannot start the load");
        return fail(was_shown);
    }
    vTaskDelay(pdMS_TO_TICKS(SNES_OPEN_MS));

    /* The first 64 bytes the core sees are the header it maps by; the image
     * then follows from its own first byte. */
    if (send_locked(FPGA_CMD_ROM_DATA, header, TANG_SNES_HEADER_LEN) != 0) {
        snprintf(why, why_len, "the core refused the header");
        return fail(was_shown);
    }

    uint32_t sent = 0;
    while (sent < image) {
        const uint32_t left = image - sent;
        const UINT want = (UINT)(left < SNES_CHUNK ? left : SNES_CHUNK);
        UINT got = 0;
        if (f_read(file, s_chunk, want, &got) != FR_OK || got != want) {
            snprintf(why, why_len, "card read failed at %u", (unsigned)sent);
            return fail(was_shown);
        }
        if (send_locked(FPGA_CMD_ROM_DATA, s_chunk, got) != 0) {
            snprintf(why, why_len, "the core stopped taking data at %u",
                     (unsigned)sent);
            return fail(was_shown);
        }
        sent += got;
        taskYIELD();                    /* keep the console and USB alive */
    }

    vTaskDelay(pdMS_TO_TICKS(SNES_DRAIN_MS));
    if (send_locked(FPGA_CMD_SET_LOAD, (const uint8_t[]){ 0 }, 1) != 0) {
        snprintf(why, why_len, "%u bytes in, but the core did not start",
                 (unsigned)sent);
        return fail(was_shown);
    }
    vTaskDelay(pdMS_TO_TICKS(SNES_START_MS));
    /* Hidden again, as the host loader does after the release: HOME may have
     * brought the desk up while the cartridge was going in. */
    if (tang_osd_set(false) != 0) {
        snprintf(why, why_len, "%u bytes in and running, but the overlay "
                 "did not clear", (unsigned)sent);
        return 1;
    }

    snprintf(why, why_len, "%u bytes, %s%s; running", (unsigned)sent,
             at == TANG_SNES_HIROM_HDR ? "HiROM" : "LoROM",
             skip != 0u ? ", copier header dropped" : "");
    return 0;
}

int tang_snes_load(const char *real, char *why, size_t why_len)
{
    taskENTER_CRITICAL();
    const bool busy = s_busy;
    s_busy = true;
    taskEXIT_CRITICAL();
    if (busy) {
        snprintf(why, why_len, "another cartridge is loading");
        return 1;
    }

    FIL file;
    int rc;
    if (f_open(&file, real, FA_READ) != FR_OK) {
        snprintf(why, why_len, "cannot open %s", real);
        rc = 1;
    } else {
        rc = load(&file, why, why_len);
        (void)f_close(&file);
    }

    taskENTER_CRITICAL();
    s_busy = false;
    taskEXIT_CRITICAL();
    return rc;
}
