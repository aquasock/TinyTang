// TinyTang — replace the BL616 firmware with an image on the SD card.
//
// The mechanism is forced by the hardware (FLS-002):
//
//   * The application lives at flash offset 0x40000, and the code executes
//     from that same XIP flash.  The image therefore cannot be written in
//     place: it is staged in an erased region at 0x120000 first, verified, and
//     only then copied into the application slot.
//   * The copy itself must run from TCM.  The moment the first application
//     sector is erased, no instruction may be fetched from the application's
//     flash, so commit_staged_image() is placed in .tcm_code, runs with
//     interrupts disabled, makes no library calls, and resets without
//     returning.
//
// The boot header is validated before anything is erased: magic at 0x00, 0x08
// and 0x64, a CRC-32 of the first 252 bytes at 0xFC, and a body length at
// 0x84 that counts bytes after the 4 KiB header region.

#include "tang_fw_update.h"
#include "tang_crash.h"

#include <stdio.h>
#include <string.h>

#include "bflb_flash.h"
#include "bflb_irq.h"
#include "bl616_glb.h"
#include "compiler/compiler_ld.h"

#define FW_APP_BASE        0x00040000u
#define FW_APP_MAX_SIZE    0x000E0000u
#define FW_STAGING_BASE    0x00120000u
#define FW_VENDOR_DATA     0x00200000u
#define FW_SECTOR_SIZE     0x1000u
#define FW_BOOT_HEADER     0x100u
#define FW_HEADER_REGION   0x1000u

/* The slot and the staging area share the erased gap below the vendor's data
 * record at 0x200000 (FLS-002). */
_Static_assert(FW_APP_BASE + FW_APP_MAX_SIZE <= FW_STAGING_BASE, "app slot overlaps staging");
_Static_assert(FW_STAGING_BASE + FW_APP_MAX_SIZE <= FW_VENDOR_DATA, "staging overlaps vendor data");

/* File data, and flash read back; the commit reuses both. */
static uint8_t s_file[FW_SECTOR_SIZE];
static uint8_t s_flash[FW_SECTOR_SIZE];

/* ------------------------------------------------------------------ helpers */

static uint32_t crc32_le(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xffffffffu;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; bit++) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xedb88320u : (crc >> 1);
        }
    }
    return ~crc;
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t round_to_sector(uint32_t size)
{
    return (size + FW_SECTOR_SIZE - 1u) & ~(FW_SECTOR_SIZE - 1u);
}

/* Returns NULL if the header is valid, else a short reason string. */
static const char *check_boot_header(const uint8_t header[FW_BOOT_HEADER], uint32_t *image_size)
{
    if (header[0] != 'B' || header[1] != 'F' || header[2] != 'N' || header[3] != 'P' ||
        header[8] != 'F' || header[9] != 'C' || header[10] != 'F' || header[11] != 'G' ||
        header[0x64] != 'P' || header[0x65] != 'C' || header[0x66] != 'F' ||
        header[0x67] != 'G') {
        return "not a BL616 boot image";
    }
    if (crc32_le(header, FW_BOOT_HEADER - 4) != read_le32(&header[FW_BOOT_HEADER - 4])) {
        return "boot header CRC mismatch";
    }
    const uint32_t body = read_le32(&header[0x84]);
    if (body == 0 || body > FW_APP_MAX_SIZE - FW_HEADER_REGION) {
        return "image length out of range";
    }
    *image_size = FW_HEADER_REGION + body;
    return NULL;
}

static bool running_from_app_slot(void)
{
    const uint32_t xip = bflb_flash_get_image_offset();
    return xip >= FW_HEADER_REGION && (xip - FW_HEADER_REGION) == FW_APP_BASE;
}

/* Close the image and keep the reason where the caller can print it. */
static const char *fail(tang_fw_update_t *u, const char *why, uint32_t at)
{
    snprintf(u->error, sizeof(u->error), why, (unsigned)at);
    tang_fw_update_close(u);
    return u->error;
}

/* ------------------------------------------------------------------- commit */

// Runs entirely from TCM: once the first application sector is erased no
// instruction may be fetched from the application's XIP flash.  Interrupts
// stay disabled and the loops make no library calls.
__attribute__((noinline, noreturn, optimize("no-tree-loop-distribute-patterns")))
ATTR_TCM_SECTION static void commit_staged_image(uint32_t image_size)
{
    (void)bflb_irq_save();

    for (uint32_t offset = 0; offset < round_to_sector(image_size); offset += FW_SECTOR_SIZE) {
        for (unsigned attempt = 0; attempt < 3; attempt++) {
            bflb_flash_erase(FW_APP_BASE + offset, FW_SECTOR_SIZE);
            bflb_flash_read(FW_STAGING_BASE + offset, s_file, FW_SECTOR_SIZE);
            bflb_flash_write(FW_APP_BASE + offset, s_file, FW_SECTOR_SIZE);
            bflb_flash_read(FW_APP_BASE + offset, s_flash, FW_SECTOR_SIZE);

            uint32_t difference = 0;
            for (uint32_t i = 0; i < FW_SECTOR_SIZE; i++) {
                difference |= (uint32_t)(s_file[i] ^ s_flash[i]);
            }
            if (difference == 0) {
                break;
            }
        }
    }

    GLB_SW_POR_Reset();
    for (;;) {
    }
}

/* ---------------------------------------------------------------- the engine */

const char *tang_fw_update_open(tang_fw_update_t *u, const char *real_path)
{
    memset(u, 0, sizeof(*u));
    if (!running_from_app_slot()) {
        return "running outside the app slot; refusing";
    }

    const FRESULT opened = f_open(&u->file, real_path, FA_READ);
    if (opened == FR_NO_FILE || opened == FR_NO_PATH) {
        return "no such file";
    }
    if (opened != FR_OK) {
        return "cannot open the image";
    }
    u->open = true;

    const uint32_t file_size = (uint32_t)f_size(&u->file);
    UINT got = 0;
    uint32_t image_size = 0;
    const char *why = "image shorter than its boot header";
    if (file_size >= FW_BOOT_HEADER &&
        f_read(&u->file, s_file, FW_BOOT_HEADER, &got) == FR_OK &&
        got == FW_BOOT_HEADER) {
        why = check_boot_header(s_file, &image_size);
    }
    if (why != NULL) {
        tang_fw_update_close(u);
        return why;
    }
    if (image_size != file_size) {
        snprintf(u->error, sizeof(u->error), "file is %u bytes, header says %u",
                 (unsigned)file_size, (unsigned)image_size);
        tang_fw_update_close(u);
        return u->error;
    }
    u->image_size = image_size;
    return NULL;
}

const char *tang_fw_update_step(tang_fw_update_t *u, bool *done)
{
    *done = false;
    if (!u->open) {
        return "no image open";
    }
    UINT got = 0;

    /* Stage: erase a sector of the staging area and copy the file into it. */
    if (u->staged < u->image_size) {
        const uint32_t offset = u->staged;
        const uint32_t want = (u->image_size - offset) < FW_SECTOR_SIZE
                              ? (u->image_size - offset) : FW_SECTOR_SIZE;
        if (offset == 0 && f_lseek(&u->file, 0) != FR_OK) {
            return fail(u, "cannot rewind", 0);
        }
        if (bflb_flash_erase(FW_STAGING_BASE + offset, FW_SECTOR_SIZE) != 0) {
            return fail(u, "cannot erase staging at %u", offset);
        }
        if (f_read(&u->file, s_file, want, &got) != FR_OK || got != want) {
            return fail(u, "read failed at %u", offset);
        }
        if (bflb_flash_write(FW_STAGING_BASE + offset, s_file, want) != 0) {
            return fail(u, "staging write failed at %u", offset);
        }
        u->staged += want;
        return NULL;
    }

    /* Verify the staged copy against the file before the app slot is touched:
     * this is the last point at which a bad image can be caught safely. */
    const uint32_t offset = u->verified;
    const uint32_t want = (u->image_size - offset) < FW_SECTOR_SIZE
                          ? (u->image_size - offset) : FW_SECTOR_SIZE;
    if (offset == 0 && f_lseek(&u->file, 0) != FR_OK) {
        return fail(u, "cannot rewind for verify", 0);
    }
    if (f_read(&u->file, s_file, want, &got) != FR_OK || got != want) {
        return fail(u, "verify read failed at %u", offset);
    }
    if (bflb_flash_read(FW_STAGING_BASE + offset, s_flash, want) != 0) {
        return fail(u, "verify flash read failed at %u", offset);
    }
    for (uint32_t i = 0; i < want; i++) {
        if (s_file[i] != s_flash[i]) {
            return fail(u, "staged image differs at %u", offset + i);
        }
    }
    u->verified += want;
    if (u->verified == u->image_size) {
        f_close(&u->file);
        u->open = false;
        *done = true;
    }
    return NULL;
}

unsigned tang_fw_update_percent(const tang_fw_update_t *u)
{
    if (u->image_size == 0) {
        return 0;
    }
    return (unsigned)(((uint64_t)u->staged + u->verified) * 100u / (2u * (uint64_t)u->image_size));
}

void tang_fw_update_close(tang_fw_update_t *u)
{
    if (u->open) {
        f_close(&u->file);
        u->open = false;
    }
}

void tang_fw_update_commit(tang_fw_update_t *u)
{
    /* The commit runs with interrupts off for longer than the watchdog's
     * timeout; a reset in the middle of it would leave no application. */
    tang_crash_suspend();
    commit_staged_image(u->image_size);
}
