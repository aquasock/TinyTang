// TinyTang: the `phosphor` shell command, which drives the Tang-Phosphor core
// from this firmware.
//
// Phosphor speaks Tang-Control's extended protocol (EXTCTL-001..005): register
// reads and writes over frame type 0x10, and files streamed over 0x11.  The
// transport underneath is ported from Tang-Control (fpga_debug, fpga_stream,
// fpga_file_stream, ae350_play); this file is only the shell's view of it.
// `play` hands a file to the resident Rockbox player on the AE350, which takes
// every supported format, as Tang-Control plays a single file.  The playback
// task (phosphor_player) does the sending and watches the track; `play` waits
// on it unless told `nowait`, and `status` and `stop` reach it from the shell
// at any time.  The AE350 is the only way to play: the merged core's FPGA
// player is a raw-PCM sink (pcm_sink.sv) with no WAV or FLAC parser of its own,
// fed by the AE350, so sending it a file directly plays the file's bytes as
// samples at whatever rate was last set.

#include <stdlib.h>
#include <string.h>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "tdsh.h"
#include "tdsh_bl616.h"

int tdsh_printf(const char *fmt, ...);
int tang_phosphor_register(void);
}

#include "fpga_debug.h"
#include "fpga_file_stream.h"
#include "phosphor_player.h"

namespace {

// How long Ctrl-C or `phosphor stop` waits for the task to have stopped.
constexpr uint32_t STOP_TIMEOUT_MS = 3000;

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

// Ctrl-C stops a waited-for play.  It is read where the shell reads, so it
// works from the TinyDesk Terminal and the desk keyboard as well as over USB.
// The shell is inside this command until it returns, so nothing else is
// reading: anything else typed meanwhile is dropped rather than left to run as
// a command afterwards.
bool ctrl_c(void *)
{
    int byte;
    while ((byte = tdsh_bl616_input_read_byte()) >= 0) {
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

// The desk layer may stay on: the core carries it (commands 0x13-0x15), and
// its cell frames take the link lock one frame at a time, so they interleave
// with the file stream rather than corrupting it.  Shared by both ways of
// playing a file.
bool resolve_for_playback(tdsh_session_t *session, const char *path, char *real,
                          size_t real_size)
{
    char logical[TDSH_MAX_PATH];
    if (tdsh_path_to_real(session, path, real, real_size, logical, sizeof(logical)) != 0) {
        tdsh_printf("phosphor: bad path %s\r\n", path);
        return false;
    }
    return true;
}

// Follow a track the task is playing until it ends, printing what the blocking
// play always printed, so tools/phosphor_format_sweep.py reads it unchanged.
int wait_for_track(uint32_t track)
{
    bool reported_send = false;
    for (;;) {
        if (ctrl_c(nullptr)) {
            if (!phosphor_player_stop(STOP_TIMEOUT_MS)) {
                tdsh_printf("phosphor: the playback task did not stop in time\r\n");
                return 1;
            }
            tdsh_printf("phosphor: stopped\r\n");
            return 1;
        }
        phosphor_player_status s;
        phosphor_player_get(&s);
        if (s.track != track) {
            tdsh_printf("phosphor: another play replaced this one\r\n");
            return 1;
        }
        if (!reported_send && s.load_ms != 0) {
            tdsh_printf("phosphor: player and file sent in %lu ms\r\n",
                        static_cast<unsigned long>(s.load_ms));
            reported_send = true;
        }
        switch (s.state) {
        case phosphor_player_state::ENDED:
            tdsh_printf("phosphor: playback complete: %lu samples, %lu underruns, %lu Hz "
                        "(player result 0x%08lx)\r\n",
                        static_cast<unsigned long>(s.samples),
                        static_cast<unsigned long>(s.underruns),
                        static_cast<unsigned long>(s.rate),
                        static_cast<unsigned long>(s.result));
            return 0;
        case phosphor_player_state::FAILED:
        case phosphor_player_state::STOPPED:
            tdsh_printf("phosphor: %s\r\n", s.error);
            return 1;
        default:
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Through the resident AE350 Rockbox player: every supported format, the way
// Tang-Control plays a single file.  A track already playing is stopped first.
// The file is checked here, so a wrong path fails the command even with
// `nowait`; anything that goes wrong later is in `phosphor status`.
int cmd_play(tdsh_session_t *session, const char *path, bool wait)
{
    char real[TDSH_MAX_REAL_PATH];
    if (!resolve_for_playback(session, path, real, sizeof(real))) {
        return 1;
    }
    FILINFO info;
    if (f_stat(real, &info) != FR_OK || (info.fattrib & AM_DIR) != 0) {
        tdsh_printf("phosphor: no file at %s\r\n", path);
        return 1;
    }
    uint32_t track = 0;
    if (!phosphor_player_play(real, path, &track)) {
        tdsh_printf("phosphor: the playback task is not running\r\n");
        return 1;
    }
    if (!wait) {
        tdsh_printf("phosphor: playing %s in the background "
                    "(phosphor status, phosphor stop)\r\n", path);
        return 0;
    }
    tdsh_printf("phosphor: playing %s on the AE350 (Ctrl-C stops it)\r\n", path);
    return wait_for_track(track);
}

int cmd_status(void)
{
    phosphor_player_status s;
    phosphor_player_get(&s);
    if (s.state == phosphor_player_state::IDLE) {
        tdsh_printf("phosphor: idle, nothing played since boot\r\n");
        return 0;
    }
    tdsh_printf("phosphor: %s%s %s (track %lu)\r\n", phosphor_player_state_name(s.state),
                s.paused ? ", paused," : "", s.path, static_cast<unsigned long>(s.track));
    if (s.load_ms != 0) {
        const uint32_t seconds = s.rate != 0 ? s.samples / s.rate : 0;
        tdsh_printf("phosphor: %lu:%02lu, %lu samples at %lu Hz, %lu underruns, "
                    "sent in %lu ms\r\n",
                    static_cast<unsigned long>(seconds / 60),
                    static_cast<unsigned long>(seconds % 60),
                    static_cast<unsigned long>(s.samples), static_cast<unsigned long>(s.rate),
                    static_cast<unsigned long>(s.underruns),
                    static_cast<unsigned long>(s.load_ms));
    }
    // A plain stop's reason repeats the state already shown on the first line.
    if (s.error[0] != '\0' && strcmp(s.error, phosphor_player_state_name(s.state)) != 0) {
        tdsh_printf("phosphor: %s\r\n", s.error);
    }
    return 0;
}

int cmd_stop(void)
{
    phosphor_player_status s;
    phosphor_player_get(&s);
    if (!phosphor_player_active(s)) {
        tdsh_printf("phosphor: nothing is playing\r\n");
        return 0;
    }
    if (!phosphor_player_stop(STOP_TIMEOUT_MS)) {
        tdsh_printf("phosphor: the playback task did not stop in time\r\n");
        return 1;
    }
    tdsh_printf("phosphor: stopped %s\r\n", s.path);
    return 0;
}

int cmd_pause(bool pause)
{
    if (!phosphor_player_pause(pause)) {
        tdsh_printf("phosphor: nothing is playing\r\n");
        return 1;
    }
    tdsh_printf("phosphor: %s\r\n", pause ? "paused" : "resumed");
    return 0;
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
                "       phosphor play <file> [nowait]  any format, on the AE350 Rockbox player\r\n"
                "       phosphor status | stop         the track playing in the background\r\n"
                "       phosphor pause | resume        silence it and carry on\r\n");
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
    if (strcmp(sub, "status") == 0 && argc == 2) {
        return cmd_status();
    }
    if (strcmp(sub, "stop") == 0 && argc == 2) {
        return cmd_stop();
    }
    if (strcmp(sub, "pause") == 0 && argc == 2) {
        return cmd_pause(true);
    }
    if (strcmp(sub, "resume") == 0 && argc == 2) {
        return cmd_pause(false);
    }
    return usage();
}

const tdsh_command_t s_commands[] = {
    { "phosphor", "phosphor caps|peek|poke|play|status|stop|pause|resume|stats",
      "Drive a loaded Tang-Phosphor core over the extended protocol", cmd_phosphor, 0 },
};

} // namespace

int tang_phosphor_register(void)
{
    // Without the task `play` says so; the other subcommands still work.
    (void)phosphor_player_init();
    return tdsh_register_commands(s_commands, sizeof(s_commands) / sizeof(s_commands[0]));
}
