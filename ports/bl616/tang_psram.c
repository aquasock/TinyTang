// TinyTang — read the BL616's efuse device information, and optionally bring
// PSRAM up.
//
// The firmware's RAM is the binding constraint on this project: about 300 KB of
// .bss leaves roughly 130 KB of heap, and TinyDesk's own buffers plus a script
// worker's stack have repeatedly exhausted it.  The BL616 has a PSRAM
// controller, the SDK carries a driver for it, and the linker template already
// describes a ram_psram region that defaults to 4 MB -- but whether this part
// can use any of that is recorded per die in efuse, not in a datasheet we have.
//
// The answer decides the whole memory strategy, and it costs one read:
// bflb_efuse_device_info_type carries psram_info and a psram_info_name string.
//
// The read must come first, because the SDK's own bring-up does this:
//
//     bflb_efuse_get_device_info(&device_info);
//     if (device_info.psram_info == 0) {
//         puts("This chip has no psram, please disable CONFIG_PSRAM\r\n");
//         while (1) {}
//     }
//
// So enabling CONFIG_PSRAM on a part without PSRAM hangs the board at boot,
// before the console exists, and recovering it means the BOOT button and a
// case.  This command therefore reads and prints by default, and only calls
// board_psram_x8_init() when asked explicitly and only when the efuse says the
// die supports PSRAM.
//
//   psram          read-only: the efuse's package, flash and PSRAM fields
//   psram init     initialise the controller and report the size it finds
//
// "psram init" proves a chip is fitted; the efuse alone only proves the die
// could drive one.  Registering it as a heap is a separate step, because the
// region and its symbols only exist when the build enables CONFIG_PSRAM.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bflb_efuse.h"

/* Declared by bsp/board/bl616dk/board_flash_psram.c, which the board's
 * CMakeLists compiles into every image; the header is on the include path
 * because the board directory adds itself. */
#include "board_flash_psram.h"

#include "tdsh.h"

int tdsh_printf(const char *fmt, ...);

/* ---------------------------------------------------------------- reporting */

static void print_field(const char *label, unsigned value, const char *name)
{
    tdsh_printf("  %-11s %u%s%s\r\n", label, value,
                (name != NULL) ? "  " : "",
                (name != NULL) ? name : "");
}

static void report_efuse(void)
{
    bflb_efuse_device_info_type info;
    memset(&info, 0, sizeof(info));
    bflb_efuse_get_device_info(&info);

    tdsh_printf("psram: efuse device info, read only, nothing configured\r\n");
    print_field("package", info.package, info.package_name);
    print_field("flash", info.flash_info, info.flash_info_name);
    print_field("psram", info.psram_info, info.psram_info_name);
    print_field("version", info.version, NULL);

    if (info.psram_info == 0) {
        tdsh_printf("psram: this die reports no PSRAM; CONFIG_PSRAM must stay off\r\n");
        tdsh_printf("psram: the heap stays at OCRAM + WRAM, so the remaining levers\r\n");
        tdsh_printf("psram: are the display buffers and boot-time fragmentation\r\n");
    } else {
        tdsh_printf("psram: this die reports PSRAM support; whether a chip is fitted\r\n");
        tdsh_printf("psram: is a second question -- run 'psram init' to find out\r\n");
    }
}

/* -------------------------------------------------------------------- init */

static int bring_up(void)
{
    bflb_efuse_device_info_type info;
    memset(&info, 0, sizeof(info));
    bflb_efuse_get_device_info(&info);

    /* The guard is the point of this command: board_psram_x8_init() spins for
     * ever when psram_info is zero, and that happens before the console the
     * user would need to see it. */
    if (info.psram_info == 0) {
        tdsh_printf("psram: refused: the efuse says this die has no PSRAM, and the\r\n");
        tdsh_printf("psram: SDK's init would hang the board instead of returning\r\n");
        return 1;
    }

    tdsh_printf("psram: efuse psram_info %u, initialising\r\n", info.psram_info);
    const uint32_t rc = board_psram_x8_init();
    if (rc != 0) {
        tdsh_printf("psram: init failed, rc %lu; no usable PSRAM on this board\r\n",
                    (unsigned long)rc);
        return 1;
    }

    const uint32_t bytes = board_psram_size_get();
    tdsh_printf("psram: init OK, %lu bytes (%lu KB) reported\r\n",
                (unsigned long)bytes, (unsigned long)(bytes / 1024U));
    tdsh_printf("psram: this is not a heap yet; CONFIG_PSRAM builds the region\r\n");
    return 0;
}

/* ----------------------------------------------------------------- command */

static int cmd_psram(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;

    if (argc < 2) {
        report_efuse();
        return 0;
    }
    if (strcmp(argv[1], "init") == 0) {
        return bring_up();
    }

    tdsh_printf("psram: unknown argument %s\r\n", argv[1]);
    tdsh_printf("usage: psram              read the efuse device info\r\n");
    tdsh_printf("       psram init         initialise PSRAM if the die supports it\r\n");
    return 1;
}

/* ------------------------------------------------------------ registration */

static const tdsh_command_t s_psram_commands[] = {
    { "psram", "psram [init]",
      "Read the efuse device info; 'init' brings PSRAM up", cmd_psram, 0 },
};

int tang_psram_register(void)
{
    return tdsh_register_commands(s_psram_commands,
                                  sizeof(s_psram_commands) / sizeof(s_psram_commands[0]));
}
