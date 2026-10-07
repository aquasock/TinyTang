// TinyTang: Tang-Control's resident AE350 player loader (utils/ae350_play.cpp,
// Apache-2.0), ported.  Resident AE350 (Rockbox) playback over the FPGA
// stream loader.
//
// The register sequence is Tang-Control's with one addition, the settle delay
// after a restart (below).  The player's path also changes: Tang-Control mounts
// the card as "sd:", this firmware mounts it at "/sd" (tdsh_fs_bl616.c), so the
// FatFS path is "/sd/ae350/resident.tpi".

#include "ae350_play.h"

#include <string.h>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}

#include "ae350_file_server.h"
#include "fpga_debug.h"
#include "fpga_file_stream.h"

namespace {

constexpr char PLAYER_TPI[] = "/sd/ae350/resident.tpi";

// Tang-Phosphor register ABI 1.8 moved cpu_mode from bit 0 of 0xc0, which it
// shared with the PMOD socket declaration, to a word of its own.  An older core
// ignores a write to 0xa8 and would play the file's bytes raw, so refuse it.
constexpr uint32_t REG_ABI = 0x00000004u;
constexpr uint32_t REG_ABI_CPU_MODE = 0x00010008u;
constexpr uint32_t REG_CPU_MODE = 0x000000a8u;

} // namespace

bool ae350_poke32(uint32_t address, uint32_t value)
{
    fpga_debug_result result;
    return fpga_debug_request(FPGA_EXT_WRITE32, address, value, &result) &&
           result.status == 0;
}

bool ae350_read32(uint32_t address, uint32_t *value)
{
    fpga_debug_result result;
    if (!fpga_debug_request(FPGA_EXT_READ32, address, 0, &result) ||
        result.status != 0) {
        return false;
    }
    *value = result.data;
    return true;
}

bool ae350_select(const char **error_out)
{
    uint32_t abi = 0;
    if (!ae350_read32(REG_ABI, &abi)) {
        if (error_out != nullptr)
            *error_out = "core did not respond";
        return false;
    }
    if ((abi >> 16) != (REG_ABI_CPU_MODE >> 16) || abi < REG_ABI_CPU_MODE) {
        if (error_out != nullptr)
            *error_out = "core register ABI older than 1.8; rebuild Tang-Phosphor";
        return false;
    }

    // Route the stream/debug to the AE350.  Idempotent, so it is asserted on
    // every play rather than only when the loader is restarted.
    if (!ae350_poke32(REG_CPU_MODE, 1u)) {
        if (error_out != nullptr)
            *error_out = "AE350 did not respond";
        return false;
    }
    return true;
}

bool ae350_restart_loader(const char **error_out)
{
    if (!ae350_poke32(AE350_REG_RESTART, 1u)) {
        if (error_out != nullptr)
            *error_out = "AE350 did not respond";
        return false;
    }

    // The restart is not immediate: the request crosses into the AE350's
    // clock domain and holds the CPU in reset for a 16-bit counter's worth
    // of transport clocks (about 0.9 ms) before the boot loader runs again
    // (ae350_subsystem.sv).  A loader that had just finished a track is
    // already in WAIT, so polling for WAIT straight away sees the old state
    // and sends the player into a CPU that is about to be reset -- which is
    // what back-to-back plays did here.  Tang-Control's slower transport
    // hid it.  Twenty milliseconds is far beyond reset plus boot.
    vTaskDelay(pdMS_TO_TICKS(20));

    // Wait for the loader to reach WAIT (state 0x01).
    for (int i = 0; i < 100; ++i) {
        uint32_t state = 0;
        if (!ae350_read32(AE350_REG_STATE, &state)) {
            if (error_out != nullptr)
                *error_out = "AE350 did not respond";
            return false;
        }
        if ((state & 0xffu) == 0x01u) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (error_out != nullptr)
        *error_out = "AE350 loader did not reach WAIT";
    return false;
}

bool ae350_start_player(uint32_t *baseline, const char **error_out,
                        fpga_file_stream_cancel cancel, void *cancel_context)
{
    // The player plays one track and returns, so every track restarts the
    // loader and sends it again; the track's file is then read on demand
    // through the request mailbox, whose baseline is taken in between.
    if (!ae350_select(error_out) || !ae350_restart_loader(error_out)) {
        return false;
    }
    if (!ae350_request_baseline(baseline)) {
        if (error_out != nullptr)
            *error_out = "AE350 did not respond";
        return false;
    }
    const fpga_file_stream_result player = fpga_file_stream(PLAYER_TPI, cancel, cancel_context);
    if (player.status != fpga_file_stream_status::OK) {
        if (error_out != nullptr)
            *error_out = fpga_file_stream_status_text(player.status);
        return false;
    }

    // Leave cpu_mode set so the AE350's decoded PCM reaches the pcm_sink.
    return true;
}
