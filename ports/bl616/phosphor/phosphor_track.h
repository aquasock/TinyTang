// TinyTang: when a track on the resident AE350 player has finished.
//
// Pure logic over three register readings, so the host can test it
// (tools/tests/phosphor_track_test.cpp); the player task does the reading.
//
// The player register can still say "complete" from the previous track until
// this one starts playing, so completion alone proves nothing.  What does is
// the loader's run count (0x4020, bits 31:16): ae350_play_file restarts the
// loader, which zeroes it, and the player returns -- bumping it -- only after
// it has queued all of its audio.  So the track is done when the run count has
// moved and the player then reports complete.  Decoding happens before any
// sample plays, so the first sample is given longer than the stall rule allows
// between samples.
#pragma once

#include <stdint.h>

namespace phosphor_track {

// Phosphor's audio status (0x5c): state [3:0], error [13:6].
constexpr uint8_t STATE_COMPLETE = 4;
constexpr uint8_t STATE_ERROR = 5;

// How long the player may go without playing a sample before it is stalled.
constexpr uint32_t STALL_TIMEOUT_MS = 5000;
// How long the AE350 may take to decode before its first sample plays.
constexpr uint32_t START_TIMEOUT_MS = 30000;

enum class verdict : uint8_t {
    RUNNING,
    COMPLETE,
    LOADER_STOPPED,       // the AE350 loader trapped (state 0x81 and up)
    DECODER_ERROR,        // the player returned and the sink reports an error
    RETURNED_INCOMPLETE,  // the player returned but the sink never completed
    NO_START,             // no sample within START_TIMEOUT_MS
    STALLED,              // samples stopped moving mid-track
};

struct observation {
    uint32_t loader;   // 0x4020
    uint32_t status;   // 0x5c
    uint32_t samples;  // 0x68
    uint64_t now_ms;
};

struct tracker {
    uint64_t started_ms = 0;
    uint64_t last_progress_ms = 0;
    uint64_t returned_ms = 0;
    uint32_t last_samples = 0;
    bool have_last = false;
    bool playing = false;
    bool returned = false;
};

inline uint8_t loader_state(uint32_t loader) { return static_cast<uint8_t>(loader & 0xff); }
inline uint8_t player_state(uint32_t status) { return static_cast<uint8_t>(status & 0x0f); }
inline uint8_t decoder_error(uint32_t status) { return static_cast<uint8_t>((status >> 6) & 0xff); }

inline void begin(tracker &t, uint64_t now_ms)
{
    t = tracker{};
    t.started_ms = now_ms;
    t.last_progress_ms = now_ms;
}

inline verdict step(tracker &t, const observation &o)
{
    if (loader_state(o.loader) >= 0x81) {
        return verdict::LOADER_STOPPED;
    }
    if (t.have_last && o.samples != t.last_samples) {
        t.playing = true;
        t.last_progress_ms = o.now_ms;
    }
    t.last_samples = o.samples;
    t.have_last = true;

    const uint8_t state = player_state(o.status);
    const bool returned = (o.loader >> 16) != 0;
    if (returned && !t.returned) {
        t.returned = true;
        t.returned_ms = o.now_ms;
    }
    if (returned && state == STATE_COMPLETE) {
        return verdict::COMPLETE;
    }
    if (returned && state == STATE_ERROR) {
        return verdict::DECODER_ERROR;
    }
    if (returned && o.now_ms - t.returned_ms > STALL_TIMEOUT_MS) {
        return verdict::RETURNED_INCOMPLETE;
    }
    if (!t.playing && !returned && o.now_ms - t.started_ms > START_TIMEOUT_MS) {
        return verdict::NO_START;
    }
    if (t.playing && !returned && o.now_ms - t.last_progress_ms > STALL_TIMEOUT_MS) {
        return verdict::STALLED;
    }
    return verdict::RUNNING;
}

} // namespace phosphor_track
