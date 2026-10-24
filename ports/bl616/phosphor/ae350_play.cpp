// TinyTang: Tang-Control's resident AE350 player loader (utils/ae350_play.cpp,
// Apache-2.0), ported.  Resident AE350 (Rockbox) playback over the FPGA
// stream loader.
//
// The register sequence is Tang-Control's with one addition, the settle delay
// after a restart (below).  The player's path also changes: Tang-Control mounts
// the card as "sd:", this firmware mounts it at "/sd" (tdsh_fs_bl616.c), so the
// FatFS path is "/sd/root/ae350/tinyphosphor-play.tpi".

#include "ae350_play.h"

#include <string.h>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "ae350_load.h"
}

#include "ae350_file_server.h"
#include "fpga_debug.h"
#include "fpga_file_stream.h"

namespace {

constexpr char PLAYER_IMAGE[] = "/sd/root/ae350/tinyphosphor-play.elf";

// Tang-Phosphor register ABI 1.8 moved cpu_mode from bit 0 of 0xc0, which it
// shared with the PMOD socket declaration, to a word of its own.  An older core
// ignores a write to 0xa8 and would play the file's bytes raw, so refuse it.
constexpr uint32_t REG_ABI = 0x00000004u;
constexpr uint32_t REG_ABI_CPU_MODE = 0x00010008u;
constexpr uint32_t REG_CPU_MODE = 0x000000a8u;

// The boot ROM's image header (software/ae350/include/ae350.h, written by
// tools/ae350_run.py pack): eight little-endian words.  A player deployed as
// an ELF, the way the game players are, is turned into that image here, so the
// card needs no pack step and the core still receives the TPI1 it always did.
constexpr uint32_t IMAGE_MAGIC = 0x31495054u;        /* "TPI1" */
constexpr uint32_t IMAGE_HEADER_BYTES = 32;

void put_le32(uint8_t *at, uint32_t value)
{
    at[0] = static_cast<uint8_t>(value);
    at[1] = static_cast<uint8_t>(value >> 8);
    at[2] = static_cast<uint8_t>(value >> 16);
    at[3] = static_cast<uint8_t>(value >> 24);
}

int elf_read(void *ctx, uint32_t offset, uint8_t *dst, uint32_t length)
{
    FIL *file = static_cast<FIL *>(ctx);
    UINT got = 0;
    if (f_lseek(file, offset) != FR_OK) {
        return -1;
    }
    if (f_read(file, dst, length, &got) != FR_OK || got != length) {
        return -1;
    }
    return 0;
}

bool player_is_elf(const char *path)
{
    FIL file;
    uint8_t magic[4];
    UINT got = 0;
    if (f_open(&file, path, FA_READ) != FR_OK) {
        return false;
    }
    const bool elf = f_read(&file, magic, sizeof magic, &got) == FR_OK &&
                     got == sizeof magic && memcmp(magic, "\x7f" "ELF", 4) == 0;
    (void)f_close(&file);
    return elf;
}

/* Stream the player: a packed .tpi as it is, or an ELF turned into the boot
 * ROM's image.  Only a single loadable segment can be flattened into that
 * image, so a multi-segment ELF is refused rather than mis-packed. */
bool stream_player(const char *path, const char **error_out,
                   fpga_file_stream_cancel cancel, void *cancel_context)
{
    if (!player_is_elf(path)) {
        const fpga_file_stream_result plain =
            fpga_file_stream(path, cancel, cancel_context);
        if (plain.status == fpga_file_stream_status::OK) {
            return true;
        }
        if (error_out != nullptr) {
            *error_out = fpga_file_stream_status_text(plain.status);
        }
        return false;
    }

    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK) {
        if (error_out != nullptr) {
            *error_out = "the player image is missing";
        }
        return false;
    }
    ae350_elf_t elf;
    bool ok = ae350_elf_parse(&elf, elf_read, &file) == AE350_OK && elf.count == 1;
    const ae350_segment_t segment = ok ? elf.segment[0] : ae350_segment_t{};
    if (ok && ((segment.size & 3u) != 0)) {
        ok = false;
    }
    uint32_t crc = 0;
    uint8_t chunk[512];
    for (uint32_t at = 0; ok && at < segment.size;) {
        const uint32_t left = segment.size - at;
        const uint32_t n = left < sizeof chunk ? left : sizeof chunk;
        if (elf_read(&file, segment.offset + at, chunk, n) != 0) {
            ok = false;
            break;
        }
        crc = ae350_crc32(crc, chunk, n);
        at += n;
    }
    (void)f_close(&file);
    if (!ok) {
        if (error_out != nullptr) {
            *error_out = "the player ELF is not one 4-byte-aligned segment";
        }
        return false;
    }

    uint8_t header[IMAGE_HEADER_BYTES];
    put_le32(header + 0, IMAGE_MAGIC);
    put_le32(header + 4, IMAGE_HEADER_BYTES);
    put_le32(header + 8, elf.base);
    put_le32(header + 12, elf.entry);
    put_le32(header + 16, segment.size);
    put_le32(header + 20, crc);
    put_le32(header + 24, 0);
    put_le32(header + 28, 0);

    fpga_file_stream_options options;
    options.offset = segment.offset;
    options.length = IMAGE_HEADER_BYTES + segment.size;
    options.prefix = header;
    options.prefix_length = IMAGE_HEADER_BYTES;

    const fpga_file_stream_result sent =
        fpga_file_stream(path, cancel, cancel_context, options);
    if (sent.status == fpga_file_stream_status::OK) {
        return true;
    }
    if (error_out != nullptr) {
        *error_out = fpga_file_stream_status_text(sent.status);
    }
    return false;
}

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
    if (!stream_player(PLAYER_IMAGE, error_out, cancel, cancel_context)) {
        return false;
    }

    // Leave cpu_mode set so the AE350's decoded PCM reaches the pcm_sink.
    return true;
}
