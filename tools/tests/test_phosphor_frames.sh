#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Host tests for the Phosphor transport's pure encoders, carried from
# Tang-Control with the headers they test: the extended-protocol frame and CRC
# encoders, and the FLAC metadata reduction.  A wrong byte here is not rejected
# by anything on the board until the core refuses the frame, so the bytes are
# checked on the host first.  With them, the playback task's end-of-track rule
# (phosphor_track.h), which only shows on the board as a track that never ends
# or ends too soon.
#
#   tools/tests/test_phosphor_frames.sh
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

for t in fpga_ext_frame_test flac_stream_prefix_test phosphor_track_test phosphor_media_test; do
    c++ -std=c++17 -O1 -Wall -Wextra \
        -I "$root/ports/bl616/phosphor" \
        -o "$work/$t" "$root/tools/tests/$t.cpp"
    "$work/$t"
done
echo "test_phosphor_frames: PASS"
