// TinyTang — reflash the BL616 from a file on the SD card, without BOOT mode.
//
// Mirrors the mechanism the working firmware on this board uses, and the shape
// is forced by the hardware:
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
// Two shell commands:
//   tangput <size> <path>   receive <size> raw bytes over the console and
//                           write them to <path> on the SD, so a new image
//                           never needs a card reader
//   tangflash <path>        validate, stage, commit and reset
//
// The boot header is validated before anything is erased: magic at 0x00, 0x08
// and 0x64, a CRC-32 of the first 252 bytes at 0xFC, and a body length at
// 0x84 that counts bytes after the 4 KiB header region.

#include "tang_crash.h"
#include "tdsh_bl616.h"
#include "tang_osd_desk.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bflb_flash.h"
#include "bflb_irq.h"
#include "bl616_glb.h"
#include "compiler/compiler_ld.h"
#include "ff.h"

#include "tdsh.h"

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

static uint8_t s_sector[FW_SECTOR_SIZE];
static uint8_t s_stage[FW_SECTOR_SIZE];
static uint8_t s_verify[FW_SECTOR_SIZE];

int tdsh_printf(const char *fmt, ...);

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

/* Read the console blocking; the shell task is the only reader. */
static int console_getc(void)
{
    for (;;) {
        int c = tdsh_bl616_console_read_byte();
        if (c >= 0) return c;
        vTaskDelay(1);
    }
}

/* The shell hands commands the path as the user typed it, and the sandbox
 * translation lives in tdsh_path_to_real: that is what turns "/x" into the
 * real "/sd/x" and what keeps a command inside the shell's root. */
static bool resolve_path(tdsh_session_t *session, const char *in,
                         char *out, size_t out_size)
{
    char logical[TDSH_MAX_PATH];
    return tdsh_path_to_real(session, in, out, out_size,
                             logical, sizeof(logical)) == 0;
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
            bflb_flash_read(FW_STAGING_BASE + offset, s_stage, FW_SECTOR_SIZE);
            bflb_flash_write(FW_APP_BASE + offset, s_stage, FW_SECTOR_SIZE);
            bflb_flash_read(FW_APP_BASE + offset, s_verify, FW_SECTOR_SIZE);

            uint32_t difference = 0;
            for (uint32_t i = 0; i < FW_SECTOR_SIZE; i++) {
                difference |= (uint32_t)(s_stage[i] ^ s_verify[i]);
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

/* ------------------------------------------------------------- tangflash */

static int cmd_tangflash(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    if (argc < 2) {
        tdsh_printf("usage: tangflash <path>\r\n");
        return 1;
    }
    const char *path = argv[1];
    char real[TDSH_MAX_REAL_PATH];
    if (!resolve_path(session, path, real, sizeof(real))) {
        tdsh_printf("tangflash: bad path %s\r\n", path);
        return 1;
    }

    if (!running_from_app_slot()) {
        tdsh_printf("tangflash: running outside the app slot; refusing\r\n");
        return 1;
    }

    FIL file;
    if (f_open(&file, real, FA_READ) != FR_OK) {
        tdsh_printf("tangflash: cannot open %s\r\n", path);
        return 1;
    }
    const uint32_t file_size = (uint32_t)f_size(&file);

    UINT got = 0;
    uint32_t image_size = 0;
    const char *why = "image shorter than its boot header";
    if (file_size >= FW_BOOT_HEADER &&
        f_read(&file, s_sector, FW_BOOT_HEADER, &got) == FR_OK &&
        got == FW_BOOT_HEADER) {
        why = check_boot_header(s_sector, &image_size);
    }
    if (why != NULL) {
        f_close(&file);
        tdsh_printf("tangflash: %s\r\n", why);
        return 1;
    }
    if (image_size != file_size) {
        f_close(&file);
        tdsh_printf("tangflash: file is %u bytes, header says %u\r\n", file_size, image_size);
        return 1;
    }

    tdsh_printf("tangflash: staging %u bytes\r\n", image_size);

    for (uint32_t offset = 0; offset < round_to_sector(image_size); offset += FW_SECTOR_SIZE) {
        if (bflb_flash_erase(FW_STAGING_BASE + offset, FW_SECTOR_SIZE) != 0) {
            f_close(&file);
            tdsh_printf("tangflash: cannot erase staging\r\n");
            return 1;
        }
        taskYIELD();
    }

    if (f_lseek(&file, 0) != FR_OK) {
        f_close(&file);
        tdsh_printf("tangflash: cannot rewind\r\n");
        return 1;
    }
    for (uint32_t offset = 0; offset < image_size; offset += FW_SECTOR_SIZE) {
        const uint32_t want = (image_size - offset) < FW_SECTOR_SIZE
                              ? (image_size - offset) : FW_SECTOR_SIZE;
        if (f_read(&file, s_sector, want, &got) != FR_OK || got != want) {
            f_close(&file);
            tdsh_printf("tangflash: read failed at %u\r\n", offset);
            return 1;
        }
        if (bflb_flash_write(FW_STAGING_BASE + offset, s_sector, want) != 0) {
            f_close(&file);
            tdsh_printf("tangflash: staging write failed at %u\r\n", offset);
            return 1;
        }
        taskYIELD();
    }

    /* Verify the staged copy against the file before the app slot is touched:
     * this is the last point at which a bad image can be caught safely. */
    if (f_lseek(&file, 0) != FR_OK) {
        f_close(&file);
        tdsh_printf("tangflash: cannot rewind for verify\r\n");
        return 1;
    }
    for (uint32_t offset = 0; offset < image_size; offset += FW_SECTOR_SIZE) {
        const uint32_t want = (image_size - offset) < FW_SECTOR_SIZE
                              ? (image_size - offset) : FW_SECTOR_SIZE;
        if (f_read(&file, s_sector, want, &got) != FR_OK || got != want) {
            f_close(&file);
            tdsh_printf("tangflash: verify read failed\r\n");
            return 1;
        }
        if (bflb_flash_read(FW_STAGING_BASE + offset, s_stage, want) != 0) {
            f_close(&file);
            tdsh_printf("tangflash: verify flash read failed\r\n");
            return 1;
        }
        for (uint32_t i = 0; i < want; i++) {
            if (s_sector[i] != s_stage[i]) {
                f_close(&file);
                tdsh_printf("tangflash: staged image differs at %u\r\n", offset + i);
                return 1;
            }
        }
    }
    f_close(&file);

    /* "OK committing" is what tools/tinytang_flash.py waits for.  The commit
     * ends in a soft reset, which lands in the vendor loader rather than in
     * the new image (FLS-001), so the user has to power-cycle the board. */
    tdsh_printf("tangflash: OK committing; power-cycle the board to run the new firmware\r\n");
    vTaskDelay(pdMS_TO_TICKS(80));   /* let the message reach the host */
    /* The commit runs with interrupts off for longer than the watchdog's
     * timeout; a reset in the middle of it would leave no application. */
    tang_crash_suspend();
    commit_staged_image(image_size);
    return 0;
}

/* --------------------------------------------------------------- tangput */

static int cmd_tangput(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    if (argc < 3) {
        tdsh_printf("usage: tangput <size> <path>\r\n");
        return 1;
    }
    const uint32_t size = (uint32_t)strtoul(argv[1], NULL, 0);
    const char *path = argv[2];
    if (size == 0) {
        tdsh_printf("tangput: bad size\r\n");
        return 1;
    }
    char real[TDSH_MAX_REAL_PATH];
    if (!resolve_path(session, path, real, sizeof(real))) {
        tdsh_printf("tangput: bad path %s\r\n", path);
        return 1;
    }

    FIL file;
    if (f_open(&file, real, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        tdsh_printf("tangput: cannot create %s\r\n", path);
        return 1;
    }

    /* The file's bytes are data, not a status probe (tdsh_bl616.h). */
    tdsh_bl616_console_set_raw(true);
    tdsh_printf("tangput: ready for %u bytes\r\n", size);

    uint32_t done = 0;
    while (done < size) {
        uint32_t chunk = size - done;
        if (chunk > FW_SECTOR_SIZE) chunk = FW_SECTOR_SIZE;
        for (uint32_t i = 0; i < chunk; i++) {
            s_sector[i] = (uint8_t)console_getc();
        }
        UINT wrote = 0;
        if (f_write(&file, s_sector, chunk, &wrote) != FR_OK || wrote != chunk) {
            tdsh_bl616_console_set_raw(false);
            f_close(&file);
            tdsh_printf("tangput: write failed at %u\r\n", done);
            return 1;
        }
        done += chunk;
    }
    tdsh_bl616_console_set_raw(false);
    f_close(&file);

    tdsh_printf("tangput: wrote %u bytes to %s\r\n", done, path);
    return 0;
}

/* ------------------------------------------------------------- tangload */

/* The vendored Gowin JTAG programmer (ports/bl616/tang_jtag_programmer.c).
 * It loads the FPGA's SRAM directly, so this is what "boot our core" means
 * on this board: the .bin is a bitstream, not anything the BL616 executes. */
bool fpga_program(const char *fname);

/* The Phosphor playback task (ports/bl616/phosphor/phosphor_player.cpp). */
void tang_phosphor_core_replacing(void);

static int cmd_tangload(tdsh_session_t *session, int argc, char **argv)
{
    if (argc < 2) {
        tdsh_printf("usage: tangload <path>\r\n");
        return 1;
    }
    char real[TDSH_MAX_REAL_PATH];
    if (!resolve_path(session, argv[1], real, sizeof(real))) {
        tdsh_printf("tangload: bad path %s\r\n", argv[1]);
        return 1;
    }

    /* A track playing on a Phosphor core is stopped while that core can still
     * be told, so the playback task never talks to the core that replaces it. */
    tang_phosphor_core_replacing();

    tdsh_printf("tangload: programming the FPGA from %s\r\n", argv[1]);
    const bool ok = fpga_program(real);
    tdsh_printf("tangload: %s\r\n", ok ? "core loaded" : "failed");
    if (ok) {
        /* Reconfiguring the FPGA wipes anything the core was holding, the
         * desktop layer included.  Tell it, or the layer stays gone with no
         * sign of why. */
        tang_osd_desk_core_reloaded();
    }
    return ok ? 0 : 1;
}

/* ------------------------------------------------------------ registration */

static const tdsh_command_t s_tang_flash_commands[] = {
    { "tangflash", "tangflash <path>", "Reflash the BL616 from an image on the SD; power-cycle after",
      cmd_tangflash, 0 },
    { "tangput",   "tangput <size> <path>", "Receive raw bytes over the console into a file",
      cmd_tangput, 0 },
    { "tangload",  "tangload <path>", "Program the FPGA with a core image from the SD",
      cmd_tangload, 0 },
};

int tdsh_bl616_tang_flash_register(void)
{
    return tdsh_register_commands(s_tang_flash_commands,
                                  sizeof(s_tang_flash_commands) / sizeof(s_tang_flash_commands[0]));
}
