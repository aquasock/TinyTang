// TinyTang: host test for phosphor_track.h, the rule that decides when a track
// on the resident AE350 player has ended, failed or stalled.  The playback task
// reads the registers every 250 ms; these sequences feed the same readings.

#include "phosphor_track.h"

#include <stdio.h>
#include <stdlib.h>

namespace {

using phosphor_track::observation;
using phosphor_track::tracker;
using phosphor_track::verdict;

int failures;

void check(bool ok, const char *what)
{
    if (!ok) {
        printf("FAIL %s\n", what);
        ++failures;
    }
}

constexpr uint32_t RUN = 0x03;            // loader running the player
constexpr uint32_t RETURNED = 1u << 16;   // one completed run
constexpr uint32_t PLAYING = 3;
constexpr uint32_t COMPLETE = 4;

uint32_t error_status(uint8_t code) { return (static_cast<uint32_t>(code) << 6) | 5u; }

// The previous track's "complete" lingers until this one plays: that alone
// must not end the track, and the track ends once the run count has moved.
void test_complete_needs_the_run_count()
{
    tracker t;
    phosphor_track::begin(t, 0);
    check(phosphor_track::step(t, {RUN, COMPLETE, 441000, 250}) == verdict::RUNNING,
          "a stale complete before the track starts");
    check(phosphor_track::step(t, {RUN, PLAYING, 1000, 500}) == verdict::RUNNING,
          "the track starts");
    check(phosphor_track::step(t, {RUN, PLAYING, 12000, 750}) == verdict::RUNNING,
          "the track plays");
    check(phosphor_track::step(t, {RUN | RETURNED, PLAYING, 400000, 1000}) == verdict::RUNNING,
          "the player returned, the sink still draining");
    check(phosphor_track::step(t, {RUN | RETURNED, COMPLETE, 441000, 1250}) == verdict::COMPLETE,
          "returned and complete");
}

void test_loader_trap()
{
    tracker t;
    phosphor_track::begin(t, 0);
    check(phosphor_track::step(t, {0x81, 0, 0, 250}) == verdict::LOADER_STOPPED,
          "a trapped loader");
    check(phosphor_track::loader_state(0x00020085) == 0x85, "loader state field");
}

void test_decoder_error()
{
    tracker t;
    phosphor_track::begin(t, 0);
    check(phosphor_track::step(t, {RUN, error_status(0x23), 0, 250}) == verdict::RUNNING,
          "an error before the player returns is not yet final");
    check(phosphor_track::step(t, {RUN | RETURNED, error_status(0x23), 0, 500}) ==
              verdict::DECODER_ERROR,
          "an error after the player returns");
    check(phosphor_track::decoder_error(error_status(0x23)) == 0x23, "decoder error field");
}

void test_returned_without_completing()
{
    tracker t;
    phosphor_track::begin(t, 0);
    check(phosphor_track::step(t, {RUN | RETURNED, PLAYING, 5, 1000}) == verdict::RUNNING,
          "returned, waiting for complete");
    check(phosphor_track::step(t, {RUN | RETURNED, PLAYING, 6, 6000}) == verdict::RUNNING,
          "returned 5 s ago exactly");
    check(phosphor_track::step(t, {RUN | RETURNED, PLAYING, 7, 6001}) ==
              verdict::RETURNED_INCOMPLETE,
          "returned more than 5 s ago and never completed");
}

// Decoding comes before the first sample, so a slow start has 30 s.
void test_slow_start_and_no_start()
{
    tracker t;
    phosphor_track::begin(t, 1000);
    check(phosphor_track::step(t, {RUN, 0, 0, 20000}) == verdict::RUNNING,
          "19 s of decoding is not a stall");
    check(phosphor_track::step(t, {RUN, 0, 0, 31000}) == verdict::RUNNING,
          "30 s exactly");
    check(phosphor_track::step(t, {RUN, 0, 0, 31001}) == verdict::NO_START,
          "no sample after 30 s");
}

void test_stall_mid_track()
{
    tracker t;
    phosphor_track::begin(t, 0);
    phosphor_track::step(t, {RUN, PLAYING, 100, 250});
    check(phosphor_track::step(t, {RUN, PLAYING, 200, 500}) == verdict::RUNNING, "playing");
    check(phosphor_track::step(t, {RUN, PLAYING, 200, 5500}) == verdict::RUNNING,
          "5 s without a sample exactly");
    check(phosphor_track::step(t, {RUN, PLAYING, 200, 5501}) == verdict::STALLED,
          "more than 5 s without a sample");
}

// A tracker begun again (the next track) forgets the previous one.
void test_begin_resets()
{
    tracker t;
    phosphor_track::begin(t, 0);
    phosphor_track::step(t, {RUN | RETURNED, PLAYING, 10, 100});
    phosphor_track::begin(t, 10000);
    check(!t.returned && !t.playing && !t.have_last, "begin clears the previous track");
    check(phosphor_track::step(t, {RUN, COMPLETE, 10, 10250}) == verdict::RUNNING,
          "the old run count is gone");
}

} // namespace

int main()
{
    test_complete_needs_the_run_count();
    test_loader_trap();
    test_decoder_error();
    test_returned_without_completing();
    test_slow_start_and_no_start();
    test_stall_mid_track();
    test_begin_resets();
    if (failures != 0) {
        printf("phosphor_track: %d failures\n", failures);
        return EXIT_FAILURE;
    }
    printf("PASS phosphor_track end-of-track rules\n");
    return 0;
}
