// SPDX-License-Identifier: MIT
// Drive the real OLED cell link (ports/bl616/phosphor/oled_link.cpp) against
// a scripted core: which cores it identifies and how, that only the desktop
// core is sent extended frames, which blocks it repaints, and what core
// replacement, a refused write and a socket change do.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "fpga_debug.h"
#include "tang_oled.h"
extern "C" {
#include "tang_fpga_link.h"
}

static int failures;
#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            failures++;                                                       \
        }                                                                     \
    } while (0)

// ------------------------------------------------------------ the fake core
static bool g_started = true;            // tang_oled_snapshot answers
static bool g_id_answers = true;
static uint8_t g_id = 0x54;
static uint32_t g_abi = 0x00010001, g_sockets = 0x0010;
static bool g_refuse_blocks;
static uint16_t g_cells[384];
static uint32_t g_cursor = 0x10000;
static uint16_t g_panel[384];            // what the core has been sent
static uint32_t g_panel_cursor;
static int g_ids, g_ext, g_blocks, g_cursor_writes, g_block_words;

// Counts the link's own heap use (linked with -Wl,--wrap=calloc).
static int g_callocs;
extern "C" void *__real_calloc(size_t n, size_t size);
extern "C" void *__wrap_calloc(size_t n, size_t size)
{
    g_callocs++;
    return __real_calloc(n, size);
}

extern "C" bool tang_oled_running(void) { return g_started; }

extern "C" bool tang_oled_snapshot(uint16_t cells[384], uint32_t *cursor)
{
    if (!g_started) return false;
    std::memcpy(cells, g_cells, sizeof(g_cells));
    *cursor = g_cursor;
    return true;
}

extern "C" bool tang_fpga_core_id(uint8_t *id, uint32_t timeout_ms)
{
    (void)timeout_ms;
    g_ids++;
    if (!g_id_answers) return false;
    *id = g_id;
    return true;
}

bool fpga_debug_request(uint8_t opcode, uint32_t address, uint32_t data,
                        fpga_debug_result *result, uint32_t timeout_ms)
{
    (void)timeout_ms;
    g_ext++;
    std::memset(result, 0, sizeof(*result));
    if (g_id != 0x54) return false;      // a core without the protocol
    if (opcode == FPGA_EXT_READ32) {
        if (address == 0) result->data = 0x00544453;
        else if (address == 4) result->data = g_abi;
        else if (address == 0xc0) result->data = g_sockets;
        else result->status = 4;
        return true;
    }
    if (opcode == FPGA_EXT_WRITE32 && address == 0x114) {
        g_cursor_writes++;
        g_panel_cursor = data;
        return true;
    }
    result->status = 4;
    return true;
}

bool fpga_debug_write_block(uint32_t address, const uint32_t *words, size_t count,
                            fpga_debug_result *result, uint32_t timeout_ms)
{
    (void)timeout_ms;
    g_ext++;
    std::memset(result, 0, sizeof(*result));
    if (g_id != 0x54) return false;
    g_blocks++;
    g_block_words += (int)count;
    if (g_refuse_blocks) {
        result->status = 6;
        return true;
    }
    for (size_t i = 0; i < count; i++) g_panel[(address - 0x200) / 4 + i] = (uint16_t)words[i];
    return true;
}

static void reset_counts() { g_ids = g_ext = g_blocks = g_cursor_writes = g_block_words = 0; }
static void polls(int n) { for (int i = 0; i < n; i++) tang_oled_poll(); }
static bool panel_matches() { return std::memcmp(g_panel, g_cells, sizeof(g_cells)) == 0; }
static void load(uint8_t id)
{
    tang_oled_core_replacing();
    g_id = id;
    std::memset(g_panel, 0, sizeof(g_panel));
    tang_oled_core_loaded();
}

int main()
{
    for (int i = 0; i < 384; i++) g_cells[i] = (uint16_t)(0x0700 | ('A' + i % 26));

    // Not started: nothing is asked of any core and nothing is allocated.
    g_started = false;
    load(0x54);
    reset_counts();
    g_callocs = 0;
    polls(100);
    CHECK(g_ids == 0 && g_ext == 0);
    CHECK(g_callocs == 0);
    g_started = true;
    polls(1);
    CHECK(g_callocs >= 1);                // the buffers, and the lock
    const int after_start = g_callocs;
    polls(50);
    CHECK(g_callocs == after_start);      // allocated once

    // A game core is asked its ID once and then left alone, with no
    // extended frame, however long the terminal runs.
    load(0x01);
    reset_counts();
    polls(500);
    CHECK(g_ids == 1);
    CHECK(g_ext == 0);

    // A core that does not answer is asked three times, a second apart, and
    // then not again until the next load.
    g_id_answers = false;
    load(0x54);
    reset_counts();
    polls(1);
    CHECK(g_ids == 1);
    polls(24);
    CHECK(g_ids == 1);
    polls(1);                            // 25 polls, one second, later
    CHECK(g_ids == 2);
    polls(500);
    CHECK(g_ids == 3);
    CHECK(g_ext == 0);
    g_id_answers = true;

    // Desktop ABI 1.0 cannot drive the panel: register reads only.
    g_abi = 0x00010000;
    load(0x54);
    reset_counts();
    polls(100);
    CHECK(g_ids == 1);
    CHECK(g_blocks == 0 && g_cursor_writes == 0);
    CHECK(g_ext >= 3 && g_ext <= 4 * 2);  // one check a second: ID, ABI per check
    g_abi = 0x00010001;

    // ABI 1.1 with the OLED on PMOD0: the first poll paints every cell and the
    // cursor, the next sends nothing.
    load(0x54);
    reset_counts();
    polls(1);
    CHECK(g_blocks == 6 && g_block_words == 384);
    CHECK(g_cursor_writes == 1 && g_panel_cursor == g_cursor);
    CHECK(panel_matches());
    reset_counts();
    polls(10);
    CHECK(g_blocks == 0 && g_cursor_writes == 0);

    // One changed cell sends its 64-cell block alone; a cursor move sends only
    // the cursor.
    g_cells[200] = 0x1f5a;
    reset_counts();
    polls(1);
    CHECK(g_blocks == 1 && g_block_words == 64 && g_cursor_writes == 0);
    CHECK(panel_matches());
    g_cursor = 0x10305;
    reset_counts();
    polls(1);
    CHECK(g_blocks == 0 && g_cursor_writes == 1 && g_panel_cursor == 0x10305);

    // Replacement: nothing at all is sent while the core is being replaced;
    // the next load is identified again and repainted whole.
    tang_oled_core_replacing();
    reset_counts();
    polls(100);
    CHECK(g_ids == 0 && g_ext == 0);
    g_id = 0x54;
    std::memset(g_panel, 0, sizeof(g_panel));
    tang_oled_core_loaded();
    polls(1);
    CHECK(g_ids == 1 && g_blocks == 6 && panel_matches());

    // A refused block write stops drawing until the next check, which then
    // repaints everything.
    g_cells[0] = 0x2f41;
    g_refuse_blocks = true;
    reset_counts();
    polls(1);
    CHECK(g_blocks == 1);
    reset_counts();
    polls(20);
    CHECK(g_blocks == 0);
    g_refuse_blocks = false;
    polls(10);
    CHECK(g_blocks == 6 && panel_matches());

    // The panel moved to PMOD1 still counts; sockets released stop drawing
    // within a check, and selecting it again repaints it whole.
    g_sockets = 0x0100;
    polls(30);
    g_cells[383] = 0x3f42;
    reset_counts();
    polls(1);
    CHECK(g_blocks == 1);
    g_sockets = 0;
    polls(30);
    g_cells[10] = 0x4f43;
    reset_counts();
    polls(60);
    CHECK(g_blocks == 0);
    g_sockets = 0x0010;
    reset_counts();
    polls(30);
    CHECK(g_blocks == 6 && panel_matches());

    // From the desktop to a game core and back: the game core gets nothing,
    // the desktop is repainted.
    load(0x01);
    reset_counts();
    polls(200);
    CHECK(g_ids == 1 && g_ext == 0);
    load(0x54);
    polls(1);
    CHECK(panel_matches());

    if (failures) {
        std::printf("test_oled_link: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("test_oled_link: all checks passed\n");
    return 0;
}
