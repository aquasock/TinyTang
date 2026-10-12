#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Simulate the desktop compositor: the layer fills the whole visible raster
# when it is enabled and the overlay is up, and the output is black otherwise.
#
#   tools/tests/test_composite.sh
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"

desktop="$root/fpga/desktop"

if ! command -v verilator >/dev/null 2>&1; then
    echo "test_composite: verilator not found" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Everything under hdmi/ except the audio clock regeneration packet, which
# Verilator cannot fold; tools/tests/sim/acr_packet_stub.sv stands in for it.
hdmi_srcs=()
for f in "$desktop"/hdmi/*.sv; do
    [[ "$(basename "$f")" == "audio_clock_regeneration_packet.sv" ]] && continue
    hdmi_srcs+=("$f")
done

verilator --binary --timing -j 0 \
    -Wall -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-UNDRIVEN \
    -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC -Wno-BLKSEQ -Wno-VARHIDDEN \
    -Wno-CASEINCOMPLETE -Wno-LATCH -Wno-SYNCASYNCNET -Wno-PINNOTFOUND \
    -Wno-UNOPTFLAT -Wno-MULTIDRIVEN -Wno-BLKANDNBLK \
    --top-module tb_composite \
    +incdir+"$desktop" \
    --Mdir "$work" \
    -o tb \
    "$root/tools/tests/sim/tb_composite.sv" \
    "$root/tools/tests/sim/acr_packet_stub.sv" \
    "$root/tools/tests/sim/elvds_obuf_stub.sv" \
    "$desktop/desktop_hdmi.sv" \
    "$desktop/textdisp_wide.sv" \
    "${hdmi_srcs[@]}"

cd "$work"
./tb
