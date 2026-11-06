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
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "ae350_load.h"        /* ae350_crc32, shared with the other save paths */
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

/* ------------------------------------------------------------------ saves */

/* The core's frames for the cartridge array.  0x18 stores a run of bytes from
 * a 24-bit address, 0x19 reads one back, and both are refused unless the core
 * is held in reset -- the core says so itself, with a 0x19 reply carrying no
 * data bytes, rather than relying on this side to remember. */
#define SNES_SAVE_WRITE 0x18u
#define SNES_SAVE_READ  0x19u

/* The reply timeout.  A 250-byte read is 253 bytes back at 2 Mbaud, about
 * 1.3 ms on the wire; 200 ms leaves room for a joystick frame overtaking it
 * and for the core's own frame scheduling, and fails fast enough that a
 * 128 KiB readout (525 blocks) cannot stall for long if the core stops
 * answering. */
#define SNES_SAVE_WAIT_MS 200u

/* CRC-16/CCITT-FALSE, the core's own polynomial, over the reply's type byte
 * and data bytes.  The UART has no other integrity check, so this is what
 * tells a corrupted block from a good one. */
static uint16_t crc16_byte(uint16_t crc, uint8_t data)
{
    uint16_t value = (uint16_t)(crc ^ (uint16_t)((uint16_t)data << 8));
    for (int i = 0; i < 8; i++) {
        value = (value & 0x8000u) ? (uint16_t)((value << 1) ^ 0x1021u)
                                  : (uint16_t)(value << 1);
    }
    return value;
}

uint32_t tang_snes_save_size(const uint8_t header[TANG_SNES_HEADER_LEN],
                             uint8_t expansion_ram)
{
    /* Byte 0x16's low nibble names the cartridge's add-on chips; the ones with
     * a battery are 2, 5, 6, 9 and A (the SNES header's battery bit is the
     * other way round: bit 0 of the same nibble is not it, the whole nibble is
     * the chip, and these are the chips that carry SRAM). */
    const unsigned chip = header[0x16] & 0x0Fu;
    if (chip != 2u && chip != 5u && chip != 6u && chip != 9u && chip != 0xAu) {
        return 0u;
    }
    /* 0x18 holds the SRAM size as a power of two, 1 KiB << n.  A SuperFX
     * cartridge leaves it zero and puts the real size in the expansion-RAM
     * byte, three bytes before the header. */
    unsigned shift = header[0x18] & 0x0Fu;
    if (shift == 0u) {
        shift = expansion_ram & 0x0Fu;
    }
    if (shift > 7u) {
        shift = 7u;                       /* 128 KiB, the core's whole window */
    }
    const uint32_t size = 1024u << shift;
    return size > TANG_SNES_SAVE_MAX ? TANG_SNES_SAVE_MAX : size;
}

int tang_snes_save_path(const char *cart_real, char *out, size_t size)
{
    if (cart_real == NULL || out == NULL) {
        return 1;
    }
    const char *base = strrchr(cart_real, '/');
    base = base != NULL ? base + 1 : cart_real;
    const char *dot = strrchr(base, '.');
    const size_t stem = dot != NULL && dot != base ? (size_t)(dot - base) : strlen(base);
    if (stem == 0u || stem > 200u) {
        return 1;
    }
    /* /sd/root/media/snes/S/Zelda.sfc -> /sd/root/saves/snes/Zelda.sav, the
     * naming TinyNES and TinyGBC already use for their own saves. */
    const int n = snprintf(out, size, "/sd/root/saves/snes/%.*s.sav", (int)stem, base);
    return n > 0 && (size_t)n < size ? 0 : 1;
}

/* Every folder above the file, from the card's root down: a card that has
 * never saved lacks them. */
static void save_make_folders(const char *path)
{
    char folder[256];
    snprintf(folder, sizeof folder, "%s", path);
    for (char *slash = strchr(folder + 4, '/'); slash != NULL;
         slash = strchr(slash + 1, '/')) {
        *slash = '\0';
        (void)f_mkdir(folder);
        *slash = '/';
    }
}

/* Read the whole file back and hash it.  A card that did not take the bytes,
 * a file that closed short and a full card all look the same from the
 * writer's side, so without this "saved" is a promise rather than a fact. */
static bool save_verify(const char *path, uint32_t expect, uint32_t crc)
{
    FIL file;
    uint32_t running = 0u, total = 0u;
    bool ok = false;

    if (f_open(&file, path, FA_READ) != FR_OK) {
        return false;
    }
    for (;;) {
        UINT got = 0;
        if (f_read(&file, s_chunk, sizeof s_chunk, &got) != FR_OK) {
            break;
        }
        if (got == 0u) {
            ok = total == expect && running == crc;
            break;
        }
        running = ae350_crc32(running, s_chunk, got);
        total += got;
        if (total > expect) {
            break;
        }
    }
    (void)f_close(&file);
    return ok;
}

int tang_snes_save_write(const char *path, uint32_t size, char *why, size_t why_len)
{
    if (size == 0u || size > TANG_SNES_SAVE_MAX) {
        snprintf(why, why_len, "a %lu byte save is not a cartridge SRAM",
                 (unsigned long)size);
        return 1;
    }

    if (tang_fpga_link_open() != 0) {
        snprintf(why, why_len, "cannot bring up UART1");
        return 1;
    }
    if (tang_fpga_pipe_open()) {
        snprintf(why, why_len, "the host's fpgapipe owns the link");
        return 1;
    }

    save_make_folders(path);
    char temp[256];
    const int named = snprintf(temp, sizeof temp, "%s.part", path);
    if (named <= 0 || (size_t)named >= sizeof temp) {
        snprintf(why, why_len, "that save path is too long");
        return 1;
    }

    /* Hold the core in reset: that is what lets the array be read, and it
     * freezes the game rather than letting it write under the readout. */
    tang_fpga_lock();
    tang_fpga_drain();
    const int held = tang_fpga_frame(FPGA_CMD_SET_LOAD, (const uint8_t[]){ 1 }, 1);
    tang_fpga_unlock();
    if (held != 0) {
        snprintf(why, why_len, "cannot hold the core in reset");
        return 1;
    }
    vTaskDelay(pdMS_TO_TICKS(SNES_OPEN_MS));

    FIL file;
    if (f_open(&file, temp, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        snprintf(why, why_len, "cannot write %s", temp);
        return 1;
    }

    static uint8_t reply[SNES_CHUNK];
    uint32_t running = 0u;
    uint32_t done = 0u;
    const char *fail = NULL;

    while (done < size && fail == NULL) {
        const uint32_t n = (size - done) < TANG_SNES_SAVE_BLOCK
                               ? (size - done) : TANG_SNES_SAVE_BLOCK;
        const uint8_t req[4] = {
            (uint8_t)(done >> 16), (uint8_t)(done >> 8), (uint8_t)done, (uint8_t)n,
        };
        if (send_locked(SNES_SAVE_READ, req, sizeof req) != 0) {
            fail = "the link stopped taking frames";
            break;
        }
        const int got = tang_fpga_wait(SNES_SAVE_READ, reply, sizeof reply,
                                      SNES_SAVE_WAIT_MS);
        if (got < 0) {
            fail = "the core did not answer a save read";
            break;
        }
        if ((uint32_t)got != n + 2u) {
            /* No data bytes means the core is not held in reset, which is the
             * one refusal this frame has. */
            fail = (got == 0) ? "the core refused the save read (not in reset)"
                              : "the core answered a save read with the wrong length";
            break;
        }
        uint16_t crc = crc16_byte(0xFFFFu, (uint8_t)SNES_SAVE_READ);
        for (uint32_t i = 0; i < n; i++) {
            crc = crc16_byte(crc, reply[i]);
        }
        if (reply[n] != (uint8_t)(crc >> 8) || reply[n + 1] != (uint8_t)crc) {
            fail = "a save block arrived corrupted";
            break;
        }
        UINT wrote = 0;
        if (f_write(&file, reply, n, &wrote) != FR_OK || wrote != n) {
            fail = "the card stopped taking the save";
            break;
        }
        running = ae350_crc32(running, reply, n);
        done += n;
        taskYIELD();
    }
    (void)f_close(&file);

    if (fail != NULL) {
        (void)f_unlink(temp);
        snprintf(why, why_len, "%s; the old save is untouched", fail);
        return 1;
    }

    if (!save_verify(temp, size, running)) {
        (void)f_unlink(temp);
        snprintf(why, why_len, "the card does not hold the save that was read");
        return 1;
    }

    /* FatFS will not rename onto a name that exists, so the old one goes
     * first -- after the new one is known good, never before. */
    (void)f_unlink(path);
    if (f_rename(temp, path) != FR_OK) {
        (void)f_unlink(temp);
        snprintf(why, why_len, "the card will not put %s in place", path);
        return 1;
    }

    snprintf(why, why_len, "saved %lu bytes to %s, read back and verified",
             (unsigned long)size, path);
    return 0;
}

/* The cartridge's own save, sized from its file.  A separate entry point from
 * the load because the exit gesture runs long after the load has finished and
 * the header is no longer in hand -- and because it costs one card read, which
 * is better paid on the way out than held for the whole session. */
int tang_snes_save_for(const char *cart_real, uint32_t *size,
                       char *path, size_t path_size)
{
    FIL file;
    if (f_open(&file, cart_real, FA_READ) != FR_OK) {
        return 1;
    }
    const uint32_t total = (uint32_t)f_size(&file);
    const uint32_t skip = tang_snes_copier_skip(total);
    const uint32_t image = total - skip;

    uint8_t lo[TANG_SNES_HEADER_LEN];
    uint8_t hi[TANG_SNES_HEADER_LEN];
    const int lo_score = score_at(&file, skip, image, TANG_SNES_LOROM_HDR, lo);
    const int hi_score = score_at(&file, skip, image, TANG_SNES_HIROM_HDR, hi);
    const uint32_t at = tang_snes_pick_header(lo_score, hi_score);
    if (at == 0u) {
        (void)f_close(&file);
        return 1;
    }
    const uint8_t *header = at == TANG_SNES_HIROM_HDR ? hi : lo;

    uint8_t expansion = 0u;
    UINT got = 0;
    if (f_lseek(&file, (FSIZE_t)(skip + at - 3u)) == FR_OK) {
        (void)f_read(&file, &expansion, 1, &got);
        if (got != 1u) {
            expansion = 0u;
        }
    }
    (void)f_close(&file);

    const uint32_t bytes = tang_snes_save_size(header, expansion);
    if (bytes == 0u) {
        return 1;
    }
    if (tang_snes_save_path(cart_real, path, path_size) != 0) {
        return 1;
    }
    *size = bytes;
    return 0;
}

void tang_snes_resume(void)
{
    (void)send_locked(FPGA_CMD_SET_LOAD, (const uint8_t[]){ 0 }, 1);
}

int tang_snes_save_restore(const char *path, uint32_t size, char *why, size_t why_len)
{
    if (size == 0u || size > TANG_SNES_SAVE_MAX) {
        snprintf(why, why_len, "unusable save size");
        return 1;
    }
    FIL file;
    const FRESULT opened = f_open(&file, path, FA_READ);
    if (opened == FR_NO_FILE || opened == FR_NO_PATH) {
        snprintf(why, why_len, "no save yet");
        return TANG_SNES_RESTORE_NONE;  /* a first run, not a failure */
    }
    if (opened != FR_OK) {
        snprintf(why, why_len, "cannot read %s", path);
        return 1;
    }

    static uint8_t payload[3 + SNES_CHUNK];
    uint32_t done = 0u;
    const char *fail = NULL;

    while (done < size && fail == NULL) {
        const uint32_t room = size - done;
        /* One block size for both directions: a 0x18 could carry more, but a
         * single size is what makes the readout and the restore testable
         * against each other, and 250 bytes per frame costs a few tens of
         * milliseconds on a load that already spends seconds streaming. */
        UINT want = room < TANG_SNES_SAVE_BLOCK ? (UINT)room : TANG_SNES_SAVE_BLOCK;
        UINT got = 0;
        if (f_read(&file, payload + 3, want, &got) != FR_OK) {
            fail = "the card stopped giving the save";
            break;
        }
        if (got == 0u) {
            break;                      /* shorter than the header claims */
        }
        payload[0] = (uint8_t)(done >> 16);
        payload[1] = (uint8_t)(done >> 8);
        payload[2] = (uint8_t)done;
        if (send_locked(SNES_SAVE_WRITE, payload, 3u + got) != 0) {
            fail = "the link stopped taking the save";
            break;
        }
        done += got;
        taskYIELD();
    }
    (void)f_close(&file);

    if (fail != NULL) {
        snprintf(why, why_len, "%s at %lu of %lu", fail,
                 (unsigned long)done, (unsigned long)size);
        return 1;
    }
    if (done < size) {
        /* A save shorter than the cartridge's SRAM: fill the rest with 0xFF,
         * which is what an unset array reads as, rather than leaving whatever
         * the previous cartridge left there. */
        static uint8_t fill[TANG_SNES_SAVE_BLOCK];
        memset(fill, 0xFF, sizeof fill);
        while (done < size) {
            uint32_t n = size - done;
            if (n > sizeof fill) {
                n = sizeof fill;
            }
            uint8_t frame[3 + TANG_SNES_SAVE_BLOCK];
            frame[0] = (uint8_t)(done >> 16);
            frame[1] = (uint8_t)(done >> 8);
            frame[2] = (uint8_t)done;
            memcpy(frame + 3, fill, n);
            if (send_locked(SNES_SAVE_WRITE, frame, 3u + n) != 0) {
                snprintf(why, why_len, "the link stopped taking the padding");
                return 1;
            }
            done += n;
        }
    }
    snprintf(why, why_len, "restored %lu bytes", (unsigned long)size);
    return 0;
}

/* A load that stopped part-way: the core stays in reset, and the overlay is
 * put back as it was before the load hid it. */
static int fail(bool was_shown)
{
    (void)tang_osd_set(was_shown);
    return 1;
}

static int load(FIL *file, const char *real, char *why, size_t why_len)
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

    /* The cartridge's battery save, sized before anything is sent: the size
     * decides whether there is a file to restore at all.  The expansion-RAM
     * byte sits three bytes before the header and carries the real SRAM size
     * for a SuperFX cartridge, whose own 0x18 is zero. */
    uint8_t expansion = 0u;
    {
        const uint32_t exp_at = skip + at - 3u;
        UINT got = 0;
        if (f_lseek(file, (FSIZE_t)exp_at) == FR_OK &&
            f_read(file, &expansion, 1, &got) == FR_OK && got != 1u) {
            expansion = 0u;
        }
    }
    const uint32_t save_size = tang_snes_save_size(header, expansion);
    char save_path[256];
    save_path[0] = '\0';
    if (save_size != 0u && tang_snes_save_path(real, save_path, sizeof save_path) != 0) {
        save_path[0] = '\0';
    }

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

    /* The battery save goes back before the reset is released, which is the
     * only window in which the core will accept it: the array was just laid
     * down by the stream above, and the game is about to read it.  A failure
     * here is reported but does not stop the game -- a cartridge that boots
     * with empty SRAM is what a first run looks like anyway. */
    bool restored = false;
    if (save_size != 0u && save_path[0] != '\0') {
        char save_why[96];
        restored = tang_snes_save_restore(save_path, save_size, save_why,
                                          sizeof save_why) == 0;
    }

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

    snprintf(why, why_len, "%u bytes, %s%s; %s", (unsigned)sent,
             at == TANG_SNES_HIROM_HDR ? "HiROM" : "LoROM",
             skip != 0u ? ", copier header dropped" : "",
             restored ? "save restored, running" : "running");
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
        rc = load(&file, real, why, why_len);
        (void)f_close(&file);
    }

    taskENTER_CRITICAL();
    s_busy = false;
    taskEXIT_CRITICAL();
    return rc;
}
