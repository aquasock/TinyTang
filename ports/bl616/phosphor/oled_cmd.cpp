// TinyTang — `oled`: print text once to the OLED on the desktop core.
//
//     oled print "first line" "second line"   blank the panel and print
//     oled clear                               blank it, dark as at boot
//     oled font [standard|compact|graphics]    the set the next print uses
//
// Each argument after `print` is one line, wrapped at the font's width; the
// shell keeps only the character after a backslash, so there is no escape for
// a line break.  A print writes the font set to 0x118, all 384 cells to 0x200
// in six 64-word blocks and a hidden cursor to 0x114, and then nothing more is
// sent: the core keeps showing those cells until the next print, a clear, or
// a core load, which resets them to dark.  It holds no task and no heap, so it
// runs from the USB console and from the desktop's Terminal alike; every
// register access holds the FPGA link lock across its reply, the lock the
// desktop layer's own traffic takes.
//
// Only the desktop core (0x54) has the OLED, from desktop ABI 1.2 (the font
// sets), and only when /tang.ini has declared the OLEDrgb in a socket.  The
// core is asked for its ID with the legacy command every core answers before
// any extended frame is sent, so a game core is never sent one.
//
// SPDX-License-Identifier: MIT

#include <cstring>

extern "C" {
#include "tdsh.h"
#include "tang_fpga_link.h"
#include "../oled_text.h"
int tdsh_printf(const char *fmt, ...);
int tang_oled_register(void);
}

#include "fpga_debug.h"

namespace {

constexpr uint8_t DESKTOP_ID = 0x54;
constexpr uint32_t DESKTOP_IDENTITY = 0x00544453;   // "TDS" at 0x00
constexpr uint32_t FONT_ABI = 0x00010002;           // register 0x118
constexpr uint32_t REG_IDENTITY = 0x00;
constexpr uint32_t REG_ABI = 0x04;
constexpr uint32_t REG_SOCKETS = 0xc0;
constexpr uint32_t REG_CURSOR = 0x114;
constexpr uint32_t REG_FONT = 0x118;
constexpr uint32_t REG_CELLS = 0x200;
constexpr unsigned BLOCK_WORDS = 64;
constexpr uint32_t PERS_OLEDRGB = 1;

// Indexed by the core's 0x118 value.
struct font_set {
    const char *name;
    int cols, rows;
};
constexpr font_set k_fonts[] = {
    {"graphics", 24, 16},
    {"standard", 12, 8},
    {"compact", 16, 8},
};
constexpr int FONT_COUNT = sizeof(k_fonts) / sizeof(k_fonts[0]);

int s_font = 1;   // Standard

bool read_reg(uint32_t address, uint32_t *value)
{
    fpga_debug_result r = {};
    if (!fpga_debug_request(FPGA_EXT_READ32, address, 0, &r) || r.status != 0) {
        return false;
    }
    *value = r.data;
    return true;
}

bool write_reg(uint32_t address, uint32_t value)
{
    fpga_debug_result r = {};
    return fpga_debug_request(FPGA_EXT_WRITE32, address, value, &r) && r.status == 0;
}

// The loaded core is the desktop core with the font sets and a declared OLED;
// otherwise say why not.
bool oled_ready(void)
{
    uint8_t id = 0;
    if (!tang_fpga_core_id(&id, 500)) {
        tdsh_printf("oled: no core answering\r\n");
        return false;
    }
    uint32_t identity = 0, abi = 0, sockets = 0;
    if (id != DESKTOP_ID || !read_reg(REG_IDENTITY, &identity) || identity != DESKTOP_IDENTITY ||
        !read_reg(REG_ABI, &abi) || abi < FONT_ABI) {
        tdsh_printf("oled: the loaded core has no OLED (the desktop core, ABI 1.2 or later)\r\n");
        return false;
    }
    if (!read_reg(REG_SOCKETS, &sockets)) {
        tdsh_printf("oled: the core did not answer\r\n");
        return false;
    }
    if (((sockets >> 4) & 0xfu) != PERS_OLEDRGB && ((sockets >> 8) & 0xfu) != PERS_OLEDRGB) {
        tdsh_printf("oled: /tang.ini does not declare the OLED (layout = i2s2_oled)\r\n");
        return false;
    }
    return true;
}

// Font, cells, then a hidden cursor.  False, saying so, on a refused write.
bool send(const uint16_t cells[OLED_CELLS])
{
    if (!write_reg(REG_FONT, static_cast<uint32_t>(s_font))) {
        tdsh_printf("oled: the core refused the font set\r\n");
        return false;
    }
    uint32_t words[BLOCK_WORDS];
    for (unsigned first = 0; first < OLED_CELLS; first += BLOCK_WORDS) {
        const unsigned count = OLED_CELLS - first < BLOCK_WORDS ? OLED_CELLS - first : BLOCK_WORDS;
        for (unsigned i = 0; i < count; i++) {
            words[i] = cells[first + i];
        }
        fpga_debug_result r = {};
        if (!fpga_debug_write_block(REG_CELLS + first * 4, words, count, &r) || r.status != 0) {
            tdsh_printf("oled: the core refused cells %u to %u\r\n", first, first + count - 1);
            return false;
        }
    }
    if (!write_reg(REG_CURSOR, 0)) {
        tdsh_printf("oled: the core refused the cursor\r\n");
        return false;
    }
    return true;
}

void usage(void)
{
    tdsh_printf("usage: oled print <line> [line...] | clear | font [standard|compact|graphics]\r\n");
}

int command(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    if (argc >= 2 && std::strcmp(argv[1], "font") == 0) {
        if (argc == 2) {
            const font_set &f = k_fonts[s_font];
            tdsh_printf("oled: font %s, %dx%d\r\n", f.name, f.cols, f.rows);
            return 0;
        }
        for (int f = 0; argc == 3 && f < FONT_COUNT; f++) {
            if (std::strcmp(argv[2], k_fonts[f].name) == 0) {
                s_font = f;   // used by the next print
                return 0;
            }
        }
        usage();
        return 1;
    }
    const bool print = argc >= 3 && std::strcmp(argv[1], "print") == 0;
    const bool clear = argc == 2 && std::strcmp(argv[1], "clear") == 0;
    if (!print && !clear) {
        usage();
        return 1;
    }
    if (!oled_ready()) {
        return 1;
    }
    // On the shell's stack: 768 bytes, and the command holds nothing after.
    uint16_t cells[OLED_CELLS];
    const font_set &f = k_fonts[s_font];
    const bool fitted = oled_text_layout(print ? argv + 2 : nullptr, print ? argc - 2 : 0,
                                         f.cols, f.rows, cells);
    if (!send(cells)) {
        return 1;
    }
    if (!fitted) {
        tdsh_printf("oled: the text was cut at %dx%d (font %s)\r\n", f.cols, f.rows, f.name);
    }
    return 0;
}

} // namespace

extern "C" int tang_oled_register(void)
{
    static const tdsh_command_t commands[] = {
        {"oled", "oled print <line> [line...] | clear | font [name]",
         "Print text to the OLED once, or clear it", command, 0},
    };
    return tdsh_register_commands(commands, sizeof(commands) / sizeof(commands[0]));
}
