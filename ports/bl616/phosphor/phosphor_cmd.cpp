// TinyTang: the `phosphor` shell command, which drives the Tang-Phosphor core
// from this firmware.
//
// Phosphor speaks Tang-Control's extended protocol (EXTCTL-001..005): register
// reads and writes over frame type 0x10, and files streamed over 0x11.  The
// transport underneath is ported from Tang-Control (fpga_debug, fpga_stream,
// fpga_file_stream, ae350_play); this file is only the shell's view of it.
// `play` hands a file to the resident Rockbox player on the AE350, which takes
// every supported format, as Tang-Control plays a single file.  It is the only
// way to play: the merged core's FPGA player is a raw-PCM sink (pcm_sink.sv)
// with no WAV or FLAC parser of its own, fed by the AE350, so sending it a file
// directly plays the file's bytes as samples at whatever rate was last set.

#include <stdlib.h>
#include <string.h>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "bflb_mtimer.h"
#include "tdsh.h"
#include "tdsh_bl616.h"
#include "tang_osd_desk.h"

int tdsh_printf(const char *fmt, ...);
int tang_phosphor_register(void);
}

#include "ae350_play.h"
#include "fpga_debug.h"
#include "fpga_file_stream.h"

namespace {

// Phosphor's player registers (Tang-Control core/phosphor.cpp).
constexpr uint32_t PHOSPHOR_AUDIO_STATUS = 0x0000005c;
constexpr uint32_t PHOSPHOR_SAMPLES_PLAYED = 0x00000068;
constexpr uint32_t PHOSPHOR_UNDERRUNS = 0x0000006c;
constexpr uint32_t PHOSPHOR_HDMI_AUDIO_RATE = 0x00000070;
constexpr uint32_t AE350_LOADER_STATE = 0x00004020;    // state 7:0, completed runs 31:16
constexpr uint32_t AE350_LOADER_RESULT = 0x0000402c;   // the program's result word
constexpr uint8_t PHOSPHOR_STATE_COMPLETE = 4;
constexpr uint8_t PHOSPHOR_STATE_ERROR = 5;
constexpr uint8_t PHOSPHOR_STATE_CANCELLED = 6;

// How long the player may go without playing a sample before a wait gives up.
constexpr uint32_t PLAYER_STALL_TIMEOUT_MS = 5000;
// How long the AE350 may take to decode before its first sample plays.
constexpr uint32_t AE350_START_TIMEOUT_MS = 30000;

bool parse_u32(const char *text, uint32_t *out)
{
    if (text == nullptr || *text == '\0') {
        return false;
    }
    char *end = nullptr;
    const unsigned long value = strtoul(text, &end, 0);
    if (end == nullptr || *end != '\0') {
        return false;
    }
    *out = static_cast<uint32_t>(value);
    return true;
}

// Ctrl-C at the console stops a play.  The shell is inside this command until
// it returns, so nothing else is reading the console: anything else typed
// meanwhile is dropped rather than left to run as a command afterwards.
bool ctrl_c(void *)
{
    int byte;
    while ((byte = tdsh_bl616_console_read_byte()) >= 0) {
        if (byte == 0x03) {
            return true;
        }
    }
    return false;
}

int cmd_caps(void)
{
    fpga_debug_result r;
    if (!fpga_debug_request(FPGA_EXT_CAPABILITIES, 0, 0, &r)) {
        tdsh_printf("phosphor: no answer to the capabilities request "
                    "(is a Phosphor core loaded?)\r\n");
        return 1;
    }
    tdsh_printf("phosphor: status %u, capabilities 0x%08lx:%s%s%s%s%s\r\n",
                static_cast<unsigned>(r.status), static_cast<unsigned long>(r.data),
                (r.data & FPGA_EXT_CAP_READ32) ? " read32" : "",
                (r.data & FPGA_EXT_CAP_WRITE32) ? " write32" : "",
                (r.data & FPGA_EXT_CAP_STREAM) ? " stream" : "",
                (r.data & FPGA_EXT_CAP_BAUD_SWITCH) ? " baud-switch" : "",
                (r.data & FPGA_EXT_CAP_WRITE_BLOCK) ? " write-block" : "");
    return r.status == 0 ? 0 : 1;
}

int cmd_peek(const char *address_text)
{
    uint32_t address;
    if (!parse_u32(address_text, &address)) {
        tdsh_printf("phosphor: bad address %s\r\n", address_text);
        return 1;
    }
    fpga_debug_result r;
    if (!fpga_debug_request(FPGA_EXT_READ32, address, 0, &r)) {
        tdsh_printf("phosphor: no answer reading 0x%08lx\r\n",
                    static_cast<unsigned long>(address));
        return 1;
    }
    tdsh_printf("0x%08lx: 0x%08lx (status %u)\r\n", static_cast<unsigned long>(address),
                static_cast<unsigned long>(r.data), static_cast<unsigned>(r.status));
    return r.status == 0 ? 0 : 1;
}

int cmd_poke(const char *address_text, const char *value_text)
{
    uint32_t address;
    uint32_t value;
    if (!parse_u32(address_text, &address) || !parse_u32(value_text, &value)) {
        tdsh_printf("phosphor: usage: phosphor poke <address> <value>\r\n");
        return 1;
    }
    fpga_debug_result r;
    if (!fpga_debug_request(FPGA_EXT_WRITE32, address, value, &r)) {
        tdsh_printf("phosphor: no answer writing 0x%08lx\r\n",
                    static_cast<unsigned long>(address));
        return 1;
    }
    tdsh_printf("0x%08lx <- 0x%08lx (status %u)\r\n", static_cast<unsigned long>(address),
                static_cast<unsigned long>(value), static_cast<unsigned>(r.status));
    return r.status == 0 ? 0 : 1;
}

// The desk layer sends its cell frames over the same link, and the core has
// no layer to draw them on: with it running they would only take bandwidth
// from the audio.  Shared by both ways of playing a file.
bool resolve_for_playback(tdsh_session_t *session, const char *path, char *real,
                          size_t real_size)
{
    if (tang_osd_desk_enabled()) {
        tdsh_printf("phosphor: the desk layer is on; run 'osd desk off' first\r\n");
        return false;
    }
    char logical[TDSH_MAX_PATH];
    if (tdsh_path_to_real(session, path, real, real_size, logical, sizeof(logical)) != 0) {
        tdsh_printf("phosphor: bad path %s\r\n", path);
        return false;
    }
    return true;
}

bool read_reg(uint32_t address, uint32_t *value)
{
    fpga_debug_result r;
    if (!fpga_debug_request(FPGA_EXT_READ32, address, 0, &r) || r.status != 0) {
        return false;
    }
    *value = r.data;
    return true;
}

// Wait for an AE350 track to finish.
//
// The player register can still say "complete" from the previous track until
// this one starts playing, so completion alone proves nothing here.  What does
// is the loader's run count (0x4020, bits 31:16): ae350_play_file restarts the
// loader, which zeroes it, and the single-shot player returns -- bumping it --
// only after it has queued all of its audio.  So the track is done when the run
// count has moved and the player then reports complete.  Decoding happens
// before any sample plays, so the first sample is given longer than the stall
// rule allows between samples.
int wait_for_ae350_player(void)
{
    const uint64_t started = bflb_mtimer_get_time_ms();
    uint32_t last_samples = 0;
    bool have_last = false;
    bool playing = false;
    uint64_t last_progress = started;
    uint64_t returned_at = 0;
    for (;;) {
        if (ctrl_c(nullptr)) {
            tdsh_printf("phosphor: stopped while the core was still playing\r\n");
            return 1;
        }
        uint32_t loader;
        uint32_t status;
        uint32_t samples;
        if (!read_reg(AE350_LOADER_STATE, &loader) ||
            !read_reg(PHOSPHOR_AUDIO_STATUS, &status) ||
            !read_reg(PHOSPHOR_SAMPLES_PLAYED, &samples)) {
            tdsh_printf("phosphor: the player status did not answer\r\n");
            return 1;
        }
        const uint64_t now = bflb_mtimer_get_time_ms();
        const uint8_t loader_state = static_cast<uint8_t>(loader & 0xff);
        if (loader_state >= 0x81) {
            uint32_t result = 0;
            (void)read_reg(AE350_LOADER_RESULT, &result);
            tdsh_printf("phosphor: the AE350 loader stopped with state 0x%02x "
                        "(result 0x%08lx)\r\n", static_cast<unsigned>(loader_state),
                        static_cast<unsigned long>(result));
            return 1;
        }
        if (have_last && samples != last_samples) {
            playing = true;
            last_progress = now;
        }
        last_samples = samples;
        have_last = true;

        const uint8_t state = static_cast<uint8_t>(status & 0x0f);
        const bool returned = (loader >> 16) != 0;
        if (returned && returned_at == 0) {
            returned_at = now;
        }
        if (returned && state == PHOSPHOR_STATE_COMPLETE) {
            uint32_t underruns = 0;
            uint32_t rate = 0;
            uint32_t result = 0;
            (void)read_reg(PHOSPHOR_UNDERRUNS, &underruns);
            (void)read_reg(PHOSPHOR_HDMI_AUDIO_RATE, &rate);
            (void)read_reg(AE350_LOADER_RESULT, &result);
            tdsh_printf("phosphor: playback complete: %lu samples, %lu underruns, %lu Hz "
                        "(player result 0x%08lx)\r\n",
                        static_cast<unsigned long>(samples),
                        static_cast<unsigned long>(underruns),
                        static_cast<unsigned long>(rate),
                        static_cast<unsigned long>(result));
            return 0;
        }
        if (state == PHOSPHOR_STATE_ERROR && returned) {
            tdsh_printf("phosphor: the decoder reported error %u\r\n",
                        static_cast<unsigned>((status >> 6) & 0xff));
            return 1;
        }
        if (returned && now - returned_at > PLAYER_STALL_TIMEOUT_MS) {
            uint32_t result = 0;
            (void)read_reg(AE350_LOADER_RESULT, &result);
            tdsh_printf("phosphor: the player returned (result 0x%08lx) but playback "
                        "did not complete (state %u)\r\n",
                        static_cast<unsigned long>(result), static_cast<unsigned>(state));
            return 1;
        }
        if (!playing && !returned && now - started > AE350_START_TIMEOUT_MS) {
            tdsh_printf("phosphor: the AE350 did not start playing (loader 0x%08lx)\r\n",
                        static_cast<unsigned long>(loader));
            return 1;
        }
        if (playing && !returned && now - last_progress > PLAYER_STALL_TIMEOUT_MS) {
            tdsh_printf("phosphor: playback stalled at %lu samples\r\n",
                        static_cast<unsigned long>(samples));
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Through the resident AE350 Rockbox player: every supported format, the way
// Tang-Control plays a single file.
int cmd_play(tdsh_session_t *session, const char *path, bool wait)
{
    char real[TDSH_MAX_REAL_PATH];
    if (!resolve_for_playback(session, path, real, sizeof(real))) {
        return 1;
    }
    tdsh_printf("phosphor: playing %s on the AE350 (Ctrl-C stops once it is playing)\r\n",
                path);
    const uint64_t started = bflb_mtimer_get_time_ms();
    const char *error = nullptr;
    if (!ae350_play_file(real, &error)) {
        tdsh_printf("phosphor: %s\r\n", error != nullptr ? error : "playback failed");
        return 1;
    }
    tdsh_printf("phosphor: player and file sent in %lu ms\r\n",
                static_cast<unsigned long>(bflb_mtimer_get_time_ms() - started));
    // `nowait` leaves the link alone while the AE350 decodes, as Tang-Control's
    // own single-file play does, for telling a decode problem apart from one
    // caused by reading the player's registers while it runs.
    return wait ? wait_for_ae350_player() : 0;
}

int cmd_stats(void)
{
    fpga_debug_stats s;
    fpga_debug_get_stats(&s);
    tdsh_printf("phosphor: %lu requests, %lu responses, %lu timeouts, %lu CRC errors, "
                "%lu malformed, %lu unexpected\r\n",
                static_cast<unsigned long>(s.requests), static_cast<unsigned long>(s.responses),
                static_cast<unsigned long>(s.timeouts), static_cast<unsigned long>(s.crc_errors),
                static_cast<unsigned long>(s.malformed), static_cast<unsigned long>(s.unexpected));
    return 0;
}

int usage(void)
{
    tdsh_printf("usage: phosphor caps | peek <addr> | poke <addr> <value> | stats\r\n"
                "       phosphor play <file> [nowait]  any format, on the AE350 Rockbox player\r\n");
    return 1;
}

int cmd_phosphor(tdsh_session_t *session, int argc, char **argv)
{
    if (argc < 2) {
        return usage();
    }
    const char *sub = argv[1];
    if (strcmp(sub, "caps") == 0 && argc == 2) {
        return cmd_caps();
    }
    if (strcmp(sub, "peek") == 0 && argc == 3) {
        return cmd_peek(argv[2]);
    }
    if (strcmp(sub, "poke") == 0 && argc == 4) {
        return cmd_poke(argv[2], argv[3]);
    }
    if (strcmp(sub, "play") == 0 && argc == 3) {
        return cmd_play(session, argv[2], true);
    }
    if (strcmp(sub, "play") == 0 && argc == 4 && strcmp(argv[3], "nowait") == 0) {
        return cmd_play(session, argv[2], false);
    }
    if (strcmp(sub, "stats") == 0 && argc == 2) {
        return cmd_stats();
    }
    return usage();
}

const tdsh_command_t s_commands[] = {
    { "phosphor", "phosphor caps|peek|poke|play|stats",
      "Drive a loaded Tang-Phosphor core over the extended protocol", cmd_phosphor, 0 },
};

} // namespace

int tang_phosphor_register(void)
{
    return tdsh_register_commands(s_commands, sizeof(s_commands) / sizeof(s_commands[0]));
}
