// TinyTang: the Phosphor playback task.
//
// One FreeRTOS task owns playback on the resident AE350 player, so the shell
// does not have to sit in a command for the length of a track.  It sends the
// player and the file, watches the track to its end with phosphor_track, and
// keeps what it saw where `phosphor status`, a blocking `phosphor play` and
// the TinyDesk app can all read it.  It prints nothing itself.
#pragma once

#include <stdint.h>

enum class phosphor_player_state : uint8_t {
    IDLE,     // nothing played since boot
    LOADING,  // sending the player and the file to the AE350
    PLAYING,  // the AE350 has the file and the track is being watched
    ENDED,    // the track played to its end
    STOPPED,  // stopped by request, by a newer play, or by a core change
    FAILED,   // the send or the track failed; `error` says how
};

struct phosphor_player_status {
    phosphor_player_state state;
    uint32_t track;      // which play this is; each play request takes the next
    char path[128];      // the path as it was given, cut to fit
    char real[192];      // the FatFS path, for reading the file's header
    uint32_t load_ms;    // how long sending the player took, once it is done
    uint32_t first_sample_ms;  // from the play to its first sample, once heard
    uint32_t samples;    // samples presented so far
    uint32_t rate;       // HDMI audio rate in hertz, 0 until known
    uint32_t underruns;
    uint32_t result;     // the player's result word, at the end
    bool paused;         // a playing track held silent by phosphor_player_pause
    char error[96];
};

// Create the task.  Called once when the `phosphor` command registers.
bool phosphor_player_init(void);

// Start a track: any track already loading or playing is stopped first.  The
// request is queued and this returns at once, with the number the track will
// carry in its status.  `real_path` is a FatFS path (tdsh_path_to_real);
// `shown_path` is what status reports.  Fails only if the task cannot start.
bool phosphor_player_play(const char *real_path, const char *shown_path,
                          uint32_t *track_out);

// Stop whatever is loading or playing.  Waits up to `timeout_ms` for the task
// to have stopped it; returns whether it had.
bool phosphor_player_stop(uint32_t timeout_ms);

// Pause or resume the track loading or playing, through Phosphor's playback
// control (0x78); the task applies it at once and `paused` in the status
// follows.  A pause made while the track loads takes effect when it would have
// started.  Returns false if no track is loading or playing.  A new play or a
// stop always clears the pause.
bool phosphor_player_pause(bool pause);

void phosphor_player_get(phosphor_player_status *out);

bool phosphor_player_active(const phosphor_player_status &status);

const char *phosphor_player_state_name(phosphor_player_state state);

// Called by `tangload` before it reprograms the FPGA, so a track is stopped
// while its core is still there to be told, and the task never sends the
// player's requests to whatever core comes next.
extern "C" void tang_phosphor_core_replacing(void);
