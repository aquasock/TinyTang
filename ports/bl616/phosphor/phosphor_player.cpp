// TinyTang: the Phosphor playback task; see phosphor_player.h.
//
// Requests are a single slot, latest wins: a play or a stop replaces whatever
// the task has not yet taken, raises s_cancel so a send in progress gives way
// at its next frame, and wakes the task.  The status belongs to the newest
// track: a play claims it at once (state LOADING), and the task updates it
// only while its track is still the newest, so a superseded track can never
// overwrite the one that replaced it.

#include "phosphor_player.h"

#include <stdio.h>
#include <string.h>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bflb_mtimer.h"
#include "tdsh.h"
}

#include "ae350_play.h"
#include "fpga_debug.h"
#include "phosphor_track.h"

namespace {

// Phosphor's player registers (docs/debug-registers.md in Tang-Phosphor) and
// the AE350 subsystem's, through the merged core's 0x4000 window.
constexpr uint32_t PHOSPHOR_STREAM_SESSIONS = 0x00000030;  // STARTs the sink has taken
constexpr uint32_t PHOSPHOR_AUDIO_STATUS = 0x0000005c;
constexpr uint32_t PHOSPHOR_FIFO_LEVEL = 0x00000064;
constexpr uint32_t PHOSPHOR_SAMPLES_PLAYED = 0x00000068;
constexpr uint32_t PHOSPHOR_UNDERRUNS = 0x0000006c;
constexpr uint32_t PHOSPHOR_HDMI_AUDIO_RATE = 0x00000070;
constexpr uint32_t PHOSPHOR_PLAYBACK_CONTROL = 0x00000078;  // bit 0: pause
constexpr uint32_t AE350_LOADER_STATE = 0x00004020;         // state 7:0, runs 31:16
constexpr uint32_t AE350_LOADER_RESULT = 0x0000402c;
constexpr uint32_t AE350_RESTART = 0x000043f0;

// How often a playing track's registers are read.  The blocking play this
// replaces read them every 20 ms; a quarter second is plenty to follow a track
// and leaves the link to the desktop's frames.
constexpr uint32_t POLL_MS = 250;
// While a track is held paused for its START (start_track), look more often so
// the sound follows the START promptly.
constexpr uint32_t HOLD_POLL_MS = 50;
// A stopped track's tail is at most the sink's 2048 samples and the stream
// queue's 512, under 60 ms at 44.1 kHz; drain() gives it a generous bound.
constexpr uint32_t DRAIN_STEP_MS = 10;
constexpr int DRAIN_STEPS = 30;
// A file send runs on this stack: FatFS's FIL and the stream's frame state.
constexpr uint32_t TASK_STACK_WORDS = 2048;
constexpr UBaseType_t TASK_PRIORITY = 2;

enum class command : uint8_t { NONE, PLAY, STOP };

SemaphoreHandle_t s_mutex;
TaskHandle_t s_task;
volatile bool s_cancel;

// Guarded by s_mutex.
command s_command = command::NONE;
char s_command_real[TDSH_MAX_REAL_PATH];
uint32_t s_next_track;
phosphor_player_status s_status;

// The task's own: whether the current track is held paused until the sink
// takes its START, and the START count it is waiting to see move.
bool s_held;
uint32_t s_sessions_before;
// The sink's underrun count when the track began.  Despite its description the
// register is cleared only with the core, so a track's own underruns are the
// difference.
uint32_t s_underruns_before;

void lock() { (void)xSemaphoreTake(s_mutex, portMAX_DELAY); }
void unlock() { (void)xSemaphoreGive(s_mutex); }

uint64_t now_ms() { return bflb_mtimer_get_time_ms(); }

bool cancelled(void *) { return s_cancel; }

bool read_reg(uint32_t address, uint32_t *value)
{
    fpga_debug_result r;
    if (!fpga_debug_request(FPGA_EXT_READ32, address, 0, &r) || r.status != 0) {
        return false;
    }
    *value = r.data;
    return true;
}

bool write_reg(uint32_t address, uint32_t value)
{
    fpga_debug_result r;
    return fpga_debug_request(FPGA_EXT_WRITE32, address, value, &r) && r.status == 0;
}

// Silence the track at once and stop its decode.  The pause holds back what is
// already queued for the sink, which the next play drains (start_track).  The
// restart resets the AE350, so the next play sends the player again, as after a
// fresh core.  The restart takes effect about a millisecond later
// (ae350_play.cpp), so wait it out before anything reads the loader state.
void halt()
{
    (void)write_reg(PHOSPHOR_PLAYBACK_CONTROL, 1);
    (void)write_reg(AE350_RESTART, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
}

void set_error(phosphor_player_status &s, const char *text)
{
    snprintf(s.error, sizeof(s.error), "%s", text != nullptr ? text : "");
}

// Finish `track` in `state`, unless a newer track has claimed the status.
void finish(uint32_t track, phosphor_player_state state, const char *error)
{
    lock();
    if (s_status.track == track) {
        s_status.state = state;
        set_error(s_status, error);
    }
    unlock();
}

// Play out what a stopped track left queued: the sink's samples and, behind
// them in the AE350's stream queue, more of its samples.  Entries leave that
// queue strictly in order, so the next track's START waits behind them and a
// paused sink would never take it.  Drained means the sink is empty and its
// count no longer moves.  Leaves the sink unpaused.
bool drain()
{
    if (!write_reg(PHOSPHOR_PLAYBACK_CONTROL, 0)) {
        return false;
    }
    uint32_t last = 0;
    bool have_last = false;
    for (int i = 0; i < DRAIN_STEPS; ++i) {
        uint32_t level;
        uint32_t samples;
        if (!read_reg(PHOSPHOR_FIFO_LEVEL, &level) ||
            !read_reg(PHOSPHOR_SAMPLES_PLAYED, &samples)) {
            return false;
        }
        if (level == 0 && have_last && samples == last) {
            return true;
        }
        last = samples;
        have_last = true;
        vTaskDelay(pdMS_TO_TICKS(DRAIN_STEP_MS));
    }
    return false;
}

// After the drain the sink is held paused until the new track's START: a track
// stopped part way leaves the sink in its playing state with nothing queued,
// and unpaused it would count every sample period of the next load as an
// underrun (the count only clears with the core).  Paused, it counts nothing,
// and the START puts it back to receiving.  A paused sink also holds the player
// back once its queue fills, so nothing is lost while the hold lasts.  Should
// the drain fail, the track plays unheld, as before the hold, rather than wait
// on a START that cannot arrive.
bool start_track(uint32_t track, const char *real)
{
    const uint64_t started = now_ms();
    const char *error = nullptr;
    s_held = drain() && write_reg(PHOSPHOR_PLAYBACK_CONTROL, 1) &&
             read_reg(PHOSPHOR_STREAM_SESSIONS, &s_sessions_before);
    if (!s_held) {
        (void)write_reg(PHOSPHOR_PLAYBACK_CONTROL, 0);
    }
    bool ok = read_reg(PHOSPHOR_UNDERRUNS, &s_underruns_before);
    if (!ok) {
        error = "the core did not answer (is a Phosphor core loaded?)";
    } else {
        ok = ae350_play_file(real, &error, cancelled, nullptr);
    }
    if (!ok) {
        if (s_cancel) {
            halt();
            finish(track, phosphor_player_state::STOPPED, "stopped while loading");
        } else {
            finish(track, phosphor_player_state::FAILED,
                   error != nullptr ? error : "playback failed");
        }
        return false;
    }
    const uint32_t load_ms = static_cast<uint32_t>(now_ms() - started);
    lock();
    if (s_status.track == track) {
        s_status.state = phosphor_player_state::PLAYING;
        s_status.load_ms = load_ms;
    }
    unlock();
    return true;
}

// One look at a playing track.  Returns whether it is still playing.
bool poll_track(uint32_t track, phosphor_track::tracker &t)
{
    using phosphor_track::verdict;
    uint32_t sessions;
    if (s_held && read_reg(PHOSPHOR_STREAM_SESSIONS, &sessions) &&
        sessions != s_sessions_before && write_reg(PHOSPHOR_PLAYBACK_CONTROL, 0)) {
        s_held = false;
    }
    uint32_t loader;
    uint32_t status;
    uint32_t samples;
    if (!read_reg(AE350_LOADER_STATE, &loader) || !read_reg(PHOSPHOR_AUDIO_STATUS, &status) ||
        !read_reg(PHOSPHOR_SAMPLES_PLAYED, &samples)) {
        finish(track, phosphor_player_state::FAILED, "the player status did not answer");
        return false;
    }
    uint32_t rate = 0;
    uint32_t underruns = 0;
    (void)read_reg(PHOSPHOR_HDMI_AUDIO_RATE, &rate);
    if (read_reg(PHOSPHOR_UNDERRUNS, &underruns)) {
        underruns -= s_underruns_before;
    }

    const verdict v = phosphor_track::step(t, {loader, status, samples, now_ms()});
    phosphor_player_state next = phosphor_player_state::FAILED;
    uint32_t result = 0;
    char error[sizeof(s_status.error)] = "";
    switch (v) {
    case verdict::RUNNING:
        next = phosphor_player_state::PLAYING;
        break;
    case verdict::COMPLETE:
        next = phosphor_player_state::ENDED;
        (void)read_reg(AE350_LOADER_RESULT, &result);
        break;
    case verdict::LOADER_STOPPED:
        (void)read_reg(AE350_LOADER_RESULT, &result);
        snprintf(error, sizeof(error),
                 "the AE350 loader stopped with state 0x%02x (result 0x%08lx)",
                 static_cast<unsigned>(phosphor_track::loader_state(loader)),
                 static_cast<unsigned long>(result));
        break;
    case verdict::DECODER_ERROR:
        snprintf(error, sizeof(error), "the decoder reported error %u",
                 static_cast<unsigned>(phosphor_track::decoder_error(status)));
        break;
    case verdict::RETURNED_INCOMPLETE:
        (void)read_reg(AE350_LOADER_RESULT, &result);
        snprintf(error, sizeof(error),
                 "the player returned (result 0x%08lx) but playback did not complete "
                 "(state %u)",
                 static_cast<unsigned long>(result),
                 static_cast<unsigned>(phosphor_track::player_state(status)));
        break;
    case verdict::NO_START:
        snprintf(error, sizeof(error), "the AE350 did not start playing (loader 0x%08lx)",
                 static_cast<unsigned long>(loader));
        break;
    case verdict::STALLED:
        snprintf(error, sizeof(error), "playback stalled at %lu samples",
                 static_cast<unsigned long>(samples));
        break;
    }
    if (next == phosphor_player_state::FAILED) {
        // Leave the next play a clean AE350 rather than one still decoding.
        halt();
    }
    lock();
    if (s_status.track == track) {
        s_status.state = next;
        s_status.samples = samples;
        s_status.rate = rate;
        s_status.underruns = underruns;
        s_status.result = result;
        set_error(s_status, error);
    }
    unlock();
    return next == phosphor_player_state::PLAYING;
}

void player_task(void *)
{
    static char real[TDSH_MAX_REAL_PATH];
    bool running = false;
    uint32_t track = 0;
    phosphor_track::tracker tracker;
    for (;;) {
        const TickType_t wait =
            running ? pdMS_TO_TICKS(s_held ? HOLD_POLL_MS : POLL_MS) : portMAX_DELAY;
        (void)ulTaskNotifyTake(pdTRUE, wait);
        lock();
        const command c = s_command;
        s_command = command::NONE;
        const uint32_t newest = s_status.track;
        if (c == command::PLAY) {
            memcpy(real, s_command_real, sizeof(real));
        }
        s_cancel = false;
        unlock();

        if (c != command::NONE && running) {
            halt();
            running = false;
            finish(track, phosphor_player_state::STOPPED, "stopped");
        }
        if (c == command::STOP) {
            // Also a play that was queued and stopped before this task took it.
            lock();
            if (phosphor_player_active(s_status)) {
                s_status.state = phosphor_player_state::STOPPED;
                set_error(s_status, "stopped");
            }
            unlock();
        } else if (c == command::PLAY) {
            track = newest;
            running = start_track(track, real);
            if (running) {
                phosphor_track::begin(tracker, now_ms());
            }
        } else if (running) {
            running = poll_track(track, tracker);
        }
    }
}

bool request(command c, const char *real, const char *shown, uint32_t *track_out)
{
    if (s_task == nullptr) {
        return false;
    }
    lock();
    if (c == command::PLAY) {
        const uint32_t track = ++s_next_track;
        memset(&s_status, 0, sizeof(s_status));
        s_status.track = track;
        s_status.state = phosphor_player_state::LOADING;
        snprintf(s_status.path, sizeof(s_status.path), "%s", shown != nullptr ? shown : real);
        snprintf(s_command_real, sizeof(s_command_real), "%s", real);
        if (track_out != nullptr) {
            *track_out = track;
        }
    }
    s_command = c;
    s_cancel = true;
    unlock();
    xTaskNotifyGive(s_task);
    return true;
}

} // namespace

bool phosphor_player_init(void)
{
    if (s_task != nullptr) {
        return true;
    }
    if (s_mutex == nullptr) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == nullptr) {
            return false;
        }
    }
    if (xTaskCreate(player_task, "phosplay", TASK_STACK_WORDS, nullptr, TASK_PRIORITY,
                    &s_task) != pdPASS) {
        s_task = nullptr;
        return false;
    }
    return true;
}

bool phosphor_player_play(const char *real_path, const char *shown_path, uint32_t *track_out)
{
    if (real_path == nullptr || real_path[0] == '\0') {
        return false;
    }
    return request(command::PLAY, real_path, shown_path, track_out);
}

bool phosphor_player_stop(uint32_t timeout_ms)
{
    if (!request(command::STOP, nullptr, nullptr, nullptr)) {
        return true;
    }
    const uint64_t deadline = now_ms() + timeout_ms;
    for (;;) {
        phosphor_player_status s;
        phosphor_player_get(&s);
        if (!phosphor_player_active(s)) {
            return true;
        }
        if (now_ms() >= deadline) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void phosphor_player_get(phosphor_player_status *out)
{
    if (out == nullptr) {
        return;
    }
    if (s_mutex == nullptr) {
        memset(out, 0, sizeof(*out));
        return;
    }
    lock();
    *out = s_status;
    unlock();
}

bool phosphor_player_active(const phosphor_player_status &status)
{
    return status.state == phosphor_player_state::LOADING ||
           status.state == phosphor_player_state::PLAYING;
}

const char *phosphor_player_state_name(phosphor_player_state state)
{
    switch (state) {
    case phosphor_player_state::IDLE: return "idle";
    case phosphor_player_state::LOADING: return "loading";
    case phosphor_player_state::PLAYING: return "playing";
    case phosphor_player_state::ENDED: return "ended";
    case phosphor_player_state::STOPPED: return "stopped";
    case phosphor_player_state::FAILED: return "failed";
    }
    return "unknown";
}

extern "C" void tang_phosphor_core_replacing(void)
{
    phosphor_player_status s;
    phosphor_player_get(&s);
    if (phosphor_player_active(s)) {
        (void)phosphor_player_stop(3000);
    }
}
