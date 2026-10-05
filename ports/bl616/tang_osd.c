// TinyTang — the on-screen text page, driven from the shell.
//
// The page is the core's (see tang_osd.h for what it is and its two fixed
// properties).  This file is the layer between the shell and three frames:
//
//     0x04 x y          move the cursor to a cell
//     0x05 <text>       write characters, advancing the cursor across the row
//     0x08 on           show or hide the page
//
// plus the `osd` command, which is the whole user interface to it.

#include "tang_osd.h"
#include "tang_osd_desk.h"
#include "tang_fpga_link.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tdsh.h"

int tdsh_printf(const char *fmt, ...);

/* What we last told the core.  The core's own state is the truth, and this can
 * be stale after the core is reprogrammed (it comes back with the page
 * shown).  It is only used to skip a redundant frame, so being wrong costs one
 * harmless command rather than a wrong picture. */
static bool s_shown;

/* -------------------------------------------------------------- primitives */

/* Caller holds the transmit lock. */
static int overlay_locked(bool on)
{
    const uint8_t value = on ? 1u : 0u;
    if (tang_fpga_frame(FPGA_CMD_OVERLAY, &value, 1) != 0) {
        return -1;
    }
    s_shown = on;
    return 0;
}

/* Caller holds the transmit lock. */
static int put_locked(uint8_t x, uint8_t y, const char *text, size_t length)
{
    if (x >= TANG_OSD_COLS || y >= TANG_OSD_ROWS) {
        return -1;
    }
    const size_t room = (size_t)(TANG_OSD_COLS - x);
    if (length > room) {
        length = room;      /* the core drops the rest with no error */
    }

    const uint8_t position[2] = { x, y };
    if (tang_fpga_frame(FPGA_CMD_OSD_CURSOR, position, sizeof(position)) != 0) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }
    return tang_fpga_frame(FPGA_CMD_OSD_TEXT, (const uint8_t *)text, length);
}

/* ---------------------------------------------------------------- the API */

int tang_osd_set(bool on)
{
    if (tang_fpga_link_open() != 0) {
        return -1;
    }
    tang_fpga_lock();
    const int rc = overlay_locked(on);
    tang_fpga_unlock();
    return rc;
}

bool tang_osd_shown(void)
{
    return s_shown;
}

int tang_osd_put(uint8_t x, uint8_t y, const char *text, size_t length)
{    if (tang_fpga_link_open() != 0) {
        return -1;
    }
    if (tang_osd_set(true) != 0) {
        return -1;
    }
    tang_fpga_lock();
    const int rc = put_locked(x, y, text, length);
    tang_fpga_unlock();
    return rc;
}

int tang_osd_clear(void)
{
    if (tang_fpga_link_open() != 0) {
        return -1;
    }
    if (tang_osd_set(true) != 0) {
        return -1;
    }

    char blank[TANG_OSD_COLS];
    memset(blank, ' ', sizeof(blank));

    tang_fpga_lock();
    int rc = 0;
    for (int y = 0; y < TANG_OSD_ROWS && rc == 0; y++) {
        rc = put_locked(0, (uint8_t)y, blank, sizeof(blank));
    }
    tang_fpga_unlock();
    return rc;
}

/* --------------------------------------------------------------- the command */

/* Join the words from `first` onward with single spaces, clipped to one row.
 * Returns the length. */
static size_t join_words(int argc, char **argv, int first, char *out, size_t cap)
{
    size_t n = 0;
    for (int i = first; i < argc && n < cap; i++) {
        if (i > first && n < cap) {
            out[n++] = ' ';
        }
        for (const char *p = argv[i]; *p != '\0' && n < cap; p++) {
            out[n++] = *p;
        }
    }
    return n;
}

static void usage(void)
{
    tdsh_printf("usage: osd on | osd off | osd clear\r\n");
    tdsh_printf("       osd at <x> <y> <text...>\r\n");
    tdsh_printf("       osd menu <selected> <title> <item...>\r\n");
    tdsh_printf("       osd desk on | osd desk off   -- TinyDesk on the extended layer\r\n");
    tdsh_printf("The page is %d columns by %d rows; column 0 always draws in\r\n",
                TANG_OSD_COLS, TANG_OSD_ROWS);
    tdsh_printf("the core's cursor colour, so a menu marks its selection there.\r\n");
    tdsh_printf("The core's logo owns rows 25 and 26.  The page is the NES core's;\r\n");
    tdsh_printf("the menu core does not have one.\r\n");
}

static int cmd_osd_at(int argc, char **argv)
{
    if (argc < 5) {
        tdsh_printf("usage: osd at <x> <y> <text...>\r\n");
        return 1;
    }
    const int x = (int)strtol(argv[2], NULL, 10);
    const int y = (int)strtol(argv[3], NULL, 10);
    if (x < 0 || x >= TANG_OSD_COLS || y < 0 || y >= TANG_OSD_ROWS) {
        tdsh_printf("osd: (%d, %d) is off the page (%dx%d)\r\n",
                    x, y, TANG_OSD_COLS, TANG_OSD_ROWS);
        return 1;
    }

    char text[TANG_OSD_COLS];
    const size_t length = join_words(argc, argv, 4, text, sizeof(text));
    if (tang_osd_put((uint8_t)x, (uint8_t)y, text, length) != 0) {
        tdsh_printf("osd: write failed\r\n");
        return 1;
    }
    return 0;
}

/* A list with a marked row, which is the shape the page is good at: the
 * marker lands in column 0 where the core draws it in its cursor colour. */
static int cmd_osd_menu(int argc, char **argv)
{
    if (argc < 5) {
        tdsh_printf("usage: osd menu <selected> <title> <item...>\r\n");
        return 1;
    }
    const int selected = (int)strtol(argv[2], NULL, 10);
    const char *title = argv[3];

    int items = argc - 4;
    const int max_items = TANG_OSD_ROWS - 2;    /* the title and the rule */
    if (items > max_items) {
        items = max_items;
    }

    if (tang_fpga_link_open() != 0) {
        tdsh_printf("osd: cannot bring up UART1\r\n");
        return 1;
    }
    if (tang_osd_set(true) != 0) {
        tdsh_printf("osd: cannot reach the core\r\n");
        return 1;
    }

    char row[TANG_OSD_COLS];
    int rc = 0;

    tang_fpga_lock();

    /* The title starts at column 1, leaving column 0 to the selection marks.
     * Every row is padded to the full width so a shorter string cannot leave
     * the tail of a longer previous one behind. */
    memset(row, ' ', sizeof(row));
    size_t tl = strlen(title);
    if (tl > sizeof(row) - 1) {
        tl = sizeof(row) - 1;
    }
    memcpy(row + 1, title, tl);
    rc = put_locked(0, 0, row, sizeof(row));

    if (rc == 0) {
        memset(row, '-', sizeof(row));
        rc = put_locked(0, 1, row, sizeof(row));
    }

    for (int i = 0; i < items && rc == 0; i++) {
        memset(row, ' ', sizeof(row));
        row[0] = (i == selected) ? '>' : ' ';
        size_t n = strlen(argv[4 + i]);
        if (n > sizeof(row) - 2) {
            n = sizeof(row) - 2;
        }
        memcpy(row + 2, argv[4 + i], n);
        rc = put_locked(0, (uint8_t)(2 + i), row, sizeof(row));
    }

    tang_fpga_unlock();

    if (rc != 0) {
        tdsh_printf("osd: draw failed\r\n");
        return 1;
    }
    return 0;
}

static int cmd_osd(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;

    if (argc < 2) {
        usage();
        return 1;
    }
    const char *sub = argv[1];

    if (strcmp(sub, "on") == 0 || strcmp(sub, "off") == 0) {
        const bool on = (sub[1] == 'n');
        if (tang_osd_set(on) != 0) {
            tdsh_printf("osd: cannot reach the core\r\n");
            return 1;
        }
        tdsh_printf("osd: %s\r\n", on ? "shown" : "hidden");
        return 0;
    }
    if (strcmp(sub, "clear") == 0) {
        if (tang_osd_clear() != 0) {
            tdsh_printf("osd: clear failed\r\n");
            return 1;
        }
        return 0;
    }
    /* The desktop layer is its own thing -- a different grid, per-cell colour,
     * and the console tap rather than the page -- so it keeps its own file and
     * only borrows the command name. */
    if (strcmp(sub, "desk") == 0) {
        return tang_osd_desk_command(argc, argv);
    }
    if (strcmp(sub, "at") == 0) {
        return cmd_osd_at(argc, argv);
    }
    if (strcmp(sub, "menu") == 0) {
        return cmd_osd_menu(argc, argv);
    }

    tdsh_printf("osd: unknown subcommand %s\r\n", sub);
    usage();
    return 1;
}

/* ------------------------------------------------------------ registration */

static const tdsh_command_t s_osd_commands[] = {
    { "osd", "osd <on|off|clear|at|menu>",
      "Draw on the core's on-screen text page", cmd_osd, 0 },
};

int tang_osd_register(void)
{
    return tdsh_register_commands(s_osd_commands,
                                  sizeof(s_osd_commands) / sizeof(s_osd_commands[0]));
}
