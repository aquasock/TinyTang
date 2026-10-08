// TinyTang — reflash the BL616 from a file on the SD card, without BOOT mode.
//
// The staging, verification and commit live in tang_fw_update.c, which the
// desktop's Software Update window uses too.
//
// Two shell commands:
//   tangput <size> <path>   receive <size> raw bytes over the console and
//                           write them to <path> on the SD, so a new image
//                           never needs a card reader
//   tangflash <path>        validate, stage, commit and reset

#include "tang_fw_update.h"
#include "tdsh_bl616.h"
#include "tang_oled.h"
#include "tang_osd_desk.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "ff.h"

#include "tdsh.h"

#define FW_SECTOR_SIZE     0x1000u

static uint8_t s_sector[FW_SECTOR_SIZE];

int tdsh_printf(const char *fmt, ...);

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

    static tang_fw_update_t update;
    const char *why = tang_fw_update_open(&update, real);
    if (why != NULL) {
        tdsh_printf("tangflash: %s: %s\r\n", path, why);
        return 1;
    }

    tdsh_printf("tangflash: staging %u bytes\r\n", (unsigned)update.image_size);
    bool done = false;
    while (!done) {
        why = tang_fw_update_step(&update, &done);
        if (why != NULL) {
            tdsh_printf("tangflash: %s\r\n", why);
            return 1;
        }
        taskYIELD();
    }

    /* "OK committing" is what tools/tinytang_flash.py waits for.  The commit
     * ends in a soft reset, which lands in the vendor loader rather than in
     * the new image (FLS-002), so the user has to power-cycle the board. */
    tdsh_printf("tangflash: OK committing; power-cycle the board to run the new firmware\r\n");
    vTaskDelay(pdMS_TO_TICKS(80));   /* let the message reach the host */
    tang_fw_update_commit(&update);
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
    /* The file's bytes come from the USB console, which only that console's
     * shell may read; from the OLED shell this would take the host's typing. */
    if (tdsh_bl616_route_current()) {
        tdsh_printf("tangput: run it from the USB console\r\n");
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
void tang_ini_core_loaded(void);

/* The Phosphor playback task (ports/bl616/phosphor/phosphor_player.cpp). */
void tang_phosphor_core_replacing(void);

/* Two shells can now run at once (the OLED terminal's and the console's), and
 * the JTAG programmer has one set of pins: a second load waits for nothing and
 * is refused. */
static bool s_programming;

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
    taskENTER_CRITICAL();
    const bool busy = s_programming;
    s_programming = true;
    taskEXIT_CRITICAL();
    if (busy) {
        tdsh_printf("tangload: another tangload is running\r\n");
        return 1;
    }

    /* A track playing on a Phosphor core is stopped while that core can still
     * be told, so the playback task never talks to the core that replaces it. */
    tang_phosphor_core_replacing();
    tang_oled_core_replacing();

    tdsh_printf("tangload: programming the FPGA from %s\r\n", argv[1]);
    const bool ok = fpga_program(real);
    tdsh_printf("tangload: %s\r\n", ok ? "core loaded" : "failed");
    if (ok) {
        /* Reconfiguring the FPGA wipes anything the core was holding, the
         * desktop layer included.  Tell it, or the layer stays gone with no
         * sign of why. */
        tang_osd_desk_core_reloaded();
        /* A core with the PMOD sockets brings them up released; declare
         * what /tang.ini says is seated (phosphor/pmod_sockets.cpp). */
        tang_ini_core_loaded();
        tang_oled_core_loaded();
    }
    s_programming = false;
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
