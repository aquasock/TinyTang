#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# The desktop core's pipelined TMDS encoder against the HDMI library's
# original, taken from commit 31fd782 (the tree before the change): the new
# output must be the original's one pixel clock later, on every channel and in
# every mode (tools/tests/sim/tb_tmds_pipeline.sv).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
command -v verilator >/dev/null 2>&1 || { echo "test_tmds_pipeline: verilator not found" >&2; exit 1; }
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
git -C "$root" show 31fd782:fpga/desktop/hdmi/tmds_channel.sv |
    sed 's/^module tmds_channel$/module tmds_channel_ref/' > "$work/tmds_channel_ref.sv"
grep -q '^module tmds_channel_ref' "$work/tmds_channel_ref.sv"
verilator --binary --timing -j 0 -Wno-fatal -Wno-DECLFILENAME -Wno-WIDTHEXPAND \
    -Wno-WIDTHTRUNC -Wno-UNUSEDSIGNAL -Wno-CASEINCOMPLETE -Wno-MULTIDRIVEN \
    --top-module tb_tmds_pipeline --Mdir "$work" -o tb \
    "$root/tools/tests/sim/tb_tmds_pipeline.sv" \
    "$root/fpga/desktop/hdmi/tmds_channel.sv" "$work/tmds_channel_ref.sv" \
    > "$work/build.log" 2>&1 || { tail -40 "$work/build.log"; exit 1; }
"$work/tb" | tee "$work/run.log"
grep -q 'tmds pipeline: .* PASS' "$work/run.log"
