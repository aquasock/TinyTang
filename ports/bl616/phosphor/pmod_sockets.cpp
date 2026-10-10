// TinyTang — applying /tang.ini to a core that has the PMOD sockets.
//
// After every successful tangload the loaded core is asked for its ID; a core
// in s_socket_cores, at a register ABI new enough for that register to mean
// socket control, is sent the word tang_ini.c makes from /tang.ini, and the
// word is read back.  Every core load goes through tangload -- the boot
// script, phosphor.tdsh, the Phosphor app, a tangload typed in the Terminal --
// so the sockets follow the file whichever way a core arrives.  Other cores
// are left alone: nothing is written to a core without the sockets.
//
// `tangini` shows what the file declares, its problems and the loaded core's
// sockets; `tangini apply` sends the declaration again without reloading the
// core, after a module has been reseated.
//
// SPDX-License-Identifier: MIT

#include <string.h>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "tdsh.h"
#include "tang_fpga_link.h"
#include "tang_ini.h"

int tdsh_printf(const char *fmt, ...);
int tang_tangini_register(void);
void tang_ini_core_loaded(void);
}

#include "fpga_debug.h"

namespace {

constexpr char INI_REAL[] = "/sd/tang.ini";
constexpr uint32_t REG_ABI = 0x04;
// Socket control word: hold [0], personalities [7:4] [11:8], flips [12] [13];
// a read adds the renderer's frame selector at [18:16] (PMOD-004).
constexpr uint32_t SOCKET_BITS = 0x3ff0u;

struct socket_core {
    uint8_t id;          // core ID on the UART
    uint32_t abi_min;    // register ABI from which `reg` is socket control
    uint32_t reg;
};

// Tang-Phosphor's merged core: 0xc0 from ABI 1.8 (PHOS-011, PMOD-004).  Its
// bring-up core also reports 0x50 but has no ABI word, so it is not matched.
// TinyTang desktop: 0xc0 from its own ABI 1.0, supporting none and VGA pairs.
const socket_core s_socket_cores[] = {
    {0x50, 0x00010008u, 0x00c0u},
    {0x54, 0x00010000u, 0x00c0u},
};

// Kept off the stack: the parser holds a line and its notes.
tang_ini_t s_ini;

bool core_id(uint8_t *id)
{
    if (tang_fpga_link_open() != 0) {
        return false;
    }
    // A core that has only just been configured may miss the first ask.
    for (int attempt = 0; attempt < 3; attempt++) {
        tang_fpga_lock();
        tang_fpga_drain();
        const int sent = tang_fpga_frame(FPGA_CMD_CORE_ID, nullptr, 0);
        tang_fpga_unlock();
        if (sent == 0 && tang_fpga_wait(FPGA_RESP_CORE_ID, id, 1, 500) == 1) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return false;
}

// A register access can miss for the same reason the ID probe can: a core that
// has only just been configured may miss the first ask, and the link is shared
// with the desktop layer and the OLED cells.  core_id() retries for exactly
// that; a single unretried read here decides whether the core has PMOD sockets
// at all, so one miss returns nullptr, the socket declaration is skipped and
// the screen stays dark with nothing printed to say why.  Retry as core_id()
// does.  Writes are retried too, and safely: the socket write is idempotent,
// so a repeat that follows a lost one writes the same word.
constexpr int REG_ATTEMPTS = 3;
constexpr uint32_t REG_RETRY_MS = 20;

bool read_reg(uint32_t address, uint32_t *value)
{
    for (int attempt = 0; attempt < REG_ATTEMPTS; attempt++) {
        fpga_debug_result r;
        if (fpga_debug_request(FPGA_EXT_READ32, address, 0, &r) && r.status == 0) {
            *value = r.data;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(REG_RETRY_MS));
    }
    return false;
}

bool write_reg(uint32_t address, uint32_t value)
{
    for (int attempt = 0; attempt < REG_ATTEMPTS; attempt++) {
        fpga_debug_result r;
        if (fpga_debug_request(FPGA_EXT_WRITE32, address, value, &r) && r.status == 0) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(REG_RETRY_MS));
    }
    return false;
}

// The loaded core's socket register, or nullptr; *id is set when it answered.
const socket_core *socket_core_loaded(bool *answered, uint8_t *id, uint32_t *abi)
{
    *answered = core_id(id);
    *abi = 0;
    if (!*answered) {
        return nullptr;
    }
    for (const socket_core &c : s_socket_cores) {
        if (c.id == *id && read_reg(REG_ABI, abi) && (*abi >> 16) == (c.abi_min >> 16) &&
            *abi >= c.abi_min) {
            return &c;
        }
    }
    return nullptr;
}

// Parse /tang.ini into s_ini.  Returns false when there is no file.
bool parse_file(void)
{
    tang_ini_begin(&s_ini);
    FIL f;
    if (f_open(&f, INI_REAL, FA_READ) != FR_OK) {
        tang_ini_end(&s_ini);
        return false;
    }
    char chunk[128];
    UINT got = 0;
    while (f_read(&f, chunk, sizeof(chunk), &got) == FR_OK && got > 0) {
        tang_ini_feed(&s_ini, chunk, got);
    }
    f_close(&f);
    tang_ini_end(&s_ini);
    return true;
}

void print_declaration(bool present)
{
    if (!present) {
        tdsh_printf("tang.ini: no /tang.ini; both sockets released\r\n");
        return;
    }
    for (unsigned i = 0; i < s_ini.nnotes; i++) {
        const tang_ini_note_t &n = s_ini.notes[i];
        if (n.line != 0) {
            tdsh_printf("tang.ini: line %u: %s\r\n", (unsigned)n.line, n.text);
        } else {
            tdsh_printf("tang.ini: %s\r\n", n.text);
        }
    }
    if (s_ini.more_notes != 0) {
        tdsh_printf("tang.ini: (%u more)\r\n", s_ini.more_notes);
    }
    tdsh_printf("tang.ini: pmod0 %s%s, pmod1 %s%s; word 0x%04lx\r\n",
                tang_ini_module_name(s_ini.module[0]), s_ini.flip[0] ? " (flipped)" : "",
                tang_ini_module_name(s_ini.module[1]), s_ini.flip[1] ? " (flipped)" : "",
                static_cast<unsigned long>(s_ini.word));
}

// Send s_ini.word to `core` and read it back.
bool send(const socket_core &core)
{
    uint32_t back = 0;
    if (!write_reg(core.reg, s_ini.word) || !read_reg(core.reg, &back)) {
        tdsh_printf("tang.ini: the core did not take the socket declaration\r\n");
        return false;
    }
    if ((back & SOCKET_BITS) != s_ini.word) {
        tdsh_printf("tang.ini: wrote 0x%04lx to 0x%02lx, read back 0x%08lx\r\n",
                    static_cast<unsigned long>(s_ini.word),
                    static_cast<unsigned long>(core.reg), static_cast<unsigned long>(back));
        return false;
    }
    tdsh_printf("tang.ini: sockets declared (0x%02lx = 0x%04lx)\r\n",
                static_cast<unsigned long>(core.reg), static_cast<unsigned long>(s_ini.word));
    return true;
}

int cmd_tangini(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    const bool apply = argc == 2 && strcmp(argv[1], "apply") == 0;
    if (argc > 2 || (argc == 2 && !apply)) {
        tdsh_printf("usage: tangini [apply]\r\n");
        return 1;
    }
    const bool present = parse_file();
    print_declaration(present);

    bool answered = false;
    uint8_t id = 0;
    uint32_t abi = 0;
    const socket_core *core = socket_core_loaded(&answered, &id, &abi);
    if (!answered) {
        tdsh_printf("tang.ini: no core answering\r\n");
        return apply ? 1 : 0;
    }
    if (core == nullptr) {
        tdsh_printf("tang.ini: core 0x%02x has no PMOD sockets\r\n", (unsigned)id);
        return apply ? 1 : 0;
    }
    if (apply) {
        return send(*core) ? 0 : 1;
    }
    uint32_t now = 0;
    if (read_reg(core->reg, &now)) {
        tdsh_printf("tang.ini: core 0x%02x (ABI %lu.%lu) has 0x%02lx = 0x%04lx%s\r\n",
                    (unsigned)id, static_cast<unsigned long>(abi >> 16),
                    static_cast<unsigned long>(abi & 0xffffu),
                    static_cast<unsigned long>(core->reg),
                    static_cast<unsigned long>(now & SOCKET_BITS),
                    (now & SOCKET_BITS) == s_ini.word ? ", as declared"
                                                      : "; `tangini apply` sends the file's");
    }
    return 0;
}

const tdsh_command_t s_commands[] = {
    { "tangini", "tangini [apply]",
      "Show /tang.ini's PMOD sockets, or send them to the loaded core", cmd_tangini, 0 },
};

} // namespace

void tang_ini_core_loaded(void)
{
    bool answered = false;
    uint8_t id = 0;
    uint32_t abi = 0;
    const socket_core *core = socket_core_loaded(&answered, &id, &abi);
    if (core == nullptr) {
        return;                            // no sockets: nothing to say
    }
    print_declaration(parse_file());
    (void)send(*core);
}

int tang_tangini_register(void)
{
    return tdsh_register_commands(s_commands, sizeof(s_commands) / sizeof(s_commands[0]));
}
