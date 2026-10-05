// TinyTang: the `phosphor` shell command -- the first slice of driving the
// Tang-Phosphor core from this firmware.
//
// Phosphor speaks Tang-Control's extended protocol (EXTCTL-001..005): register
// reads and writes over frame type 0x10, and files streamed over 0x11.  The
// transport underneath is ported from Tang-Control (fpga_debug, fpga_stream,
// fpga_file_stream); this file is only the shell's view of it.  A WAV or FLAC
// track is played the way Tang-Control's loader plays a playlist entry: stream
// the file, then poll the player's state until it reports the track complete.

#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

#include "fpga_debug.h"
#include "fpga_file_stream.h"
#include "fpga_stream.h"

namespace {

// Phosphor's player registers (Tang-Control core/phosphor.cpp).
constexpr uint32_t PHOSPHOR_AUDIO_STATUS = 0x0000005c;
constexpr uint8_t PHOSPHOR_STATE_COMPLETE = 4;
constexpr uint8_t PHOSPHOR_STATE_ERROR = 5;
constexpr uint8_t PHOSPHOR_STATE_CANCELLED = 6;

// Once the last byte is accepted the core still holds up to one PCM FIFO of
// audio (about 0.4 s), so completion follows the stream closely; Tang-Control
// gives it five seconds.
constexpr uint32_t PLAYER_COMPLETE_TIMEOUT_MS = 5000;

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

bool ends_with(const char *text, const char *suffix)
{
    const size_t n = strlen(text);
    const size_t m = strlen(suffix);
    return n >= m && strcasecmp(text + n - m, suffix) == 0;
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

int cmd_play(tdsh_session_t *session, const char *path)
{
    // The desk layer sends its cell frames over the same link, and the core
    // has no layer to draw them on: with it running they would only take
    // bandwidth from the audio.
    if (tang_osd_desk_enabled()) {
        tdsh_printf("phosphor: the desk layer is on; run 'osd desk off' first\r\n");
        return 1;
    }
    char real[TDSH_MAX_REAL_PATH];
    char logical[TDSH_MAX_PATH];
    if (tdsh_path_to_real(session, path, real, sizeof(real), logical,
                          sizeof(logical)) != 0) {
        tdsh_printf("phosphor: bad path %s\r\n", path);
        return 1;
    }

    fpga_file_stream_options options;
    options.stream_id = fpga_file_stream_reserve_id();
    options.reduce_flac_metadata = ends_with(path, ".flac");

    tdsh_printf("phosphor: streaming %s as stream %u (Ctrl-C stops)\r\n", path,
                static_cast<unsigned>(options.stream_id));
    fpga_stream_timing_reset();
    const fpga_file_stream_result s = fpga_file_stream(real, ctrl_c, nullptr, options);
    fpga_stream_timing t;
    fpga_stream_timing_get(&t);
    tdsh_printf("phosphor: %s: %lu bytes in %lu ms, CRC-32 0x%08lx",
                fpga_file_stream_status_text(s.status),
                static_cast<unsigned long>(s.bytes),
                static_cast<unsigned long>(s.elapsed_ms),
                static_cast<unsigned long>(s.crc32));
    if (s.status != fpga_file_stream_status::OK) {
        tdsh_printf(", fs %d, transport %u\r\n", static_cast<int>(s.filesystem_status),
                    static_cast<unsigned>(s.transport_status));
        return 1;
    }
    tdsh_printf("\r\n");
    tdsh_printf("phosphor: %lu frames at %lu baud: send %lu ms, ack wait %lu ms, "
                "card read %lu ms\r\n",
                static_cast<unsigned long>(t.frames),
                static_cast<unsigned long>(t.baud),
                static_cast<unsigned long>(t.send_us / 1000),
                static_cast<unsigned long>(t.ack_us / 1000),
                static_cast<unsigned long>(fpga_file_stream_read_us() / 1000));

    // Tang-Control's wait_for_player_complete, without the pause handling a
    // shell command has no way to request.
    const uint64_t started = bflb_mtimer_get_time_ms();
    for (;;) {
        if (ctrl_c(nullptr)) {
            tdsh_printf("phosphor: stopped while the core was still playing\r\n");
            return 1;
        }
        fpga_debug_result r;
        if (!fpga_debug_request(FPGA_EXT_READ32, PHOSPHOR_AUDIO_STATUS, 0, &r) ||
            r.status != 0) {
            tdsh_printf("phosphor: the player status did not answer\r\n");
            return 1;
        }
        const uint8_t state = static_cast<uint8_t>(r.data & 0x0f);
        if (state == PHOSPHOR_STATE_COMPLETE) {
            tdsh_printf("phosphor: playback complete\r\n");
            return 0;
        }
        if (state == PHOSPHOR_STATE_ERROR) {
            tdsh_printf("phosphor: the decoder reported error %u\r\n",
                        static_cast<unsigned>((r.data >> 6) & 0xff));
            return 1;
        }
        if (state == PHOSPHOR_STATE_CANCELLED) {
            tdsh_printf("phosphor: the core cancelled playback\r\n");
            return 1;
        }
        if (bflb_mtimer_get_time_ms() - started > PLAYER_COMPLETE_TIMEOUT_MS) {
            tdsh_printf("phosphor: playback did not complete (state %u)\r\n",
                        static_cast<unsigned>(state));
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
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
    tdsh_printf("usage: phosphor caps | peek <addr> | poke <addr> <value> | "
                "play <file.wav|file.flac> | stats\r\n");
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
        return cmd_play(session, argv[2]);
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
