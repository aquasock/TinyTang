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

#include "fpga_debug.h"
#include "fpga_file_stream.h"

namespace {

constexpr char PLAYER_TPI[] = "/sd/ae350/resident.tpi";

bool poke32(uint32_t address, uint32_t value)
{
    fpga_debug_result result;
    return fpga_debug_request(FPGA_EXT_WRITE32, address, value, &result) &&
           result.status == 0;
}

bool read32(uint32_t address, uint32_t *value)
{
    fpga_debug_result result;
    if (!fpga_debug_request(FPGA_EXT_READ32, address, 0, &result) ||
        result.status != 0) {
        return false;
    }
    *value = result.data;
    return true;
}

} // namespace

bool ae350_play_file(const char *full_path, const char **error_out)
{
    // The resident player loops forever once loaded: the loader stays in RUN
    // state (0x03) while the player waits for the next stream and while it
    // plays.  Only a fresh core (WAIT) or a trapped/crashed player needs a
    // reload, so the player image is streamed at most once per core load.
    uint32_t state = 0;
    if (!read32(0x00004020u, &state)) {
        if (error_out != nullptr)
            *error_out = "AE350 did not respond";
        return false;
    }

    if ((state & 0xffu) != 0x03u) {
        // Route the stream/debug to the AE350 and restart its loader.
        if (!poke32(0x000000c0u, 1u) || !poke32(0x000043f0u, 1u)) {
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
        bool waited = false;
        for (int i = 0; i < 100; ++i) {
            if (!read32(0x00004020u, &state)) {
                if (error_out != nullptr)
                    *error_out = "AE350 did not respond";
                return false;
            }
            if ((state & 0xffu) == 0x01u) {
                waited = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (!waited) {
            if (error_out != nullptr)
                *error_out = "AE350 loader did not reach WAIT";
            return false;
        }

        const fpga_file_stream_result player = fpga_file_stream(PLAYER_TPI);
        if (player.status != fpga_file_stream_status::OK) {
            if (error_out != nullptr)
                *error_out = fpga_file_stream_status_text(player.status);
            return false;
        }
    }

    const fpga_file_stream_result audio = fpga_file_stream(full_path);
    if (audio.status != fpga_file_stream_status::OK) {
        if (error_out != nullptr)
            *error_out = fpga_file_stream_status_text(audio.status);
        return false;
    }

    // Leave cpu_mode set so the AE350's decoded PCM reaches the pcm_sink.
    return true;
}
