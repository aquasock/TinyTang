#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
#
# The SDRAM framebuffer (fpga/desktop/fb_sdram.sv) against a timing-checking
# model of the W9825G6KH-6: a whole 1280x720 frame compared pixel by pixel,
# no command early by the datasheet, no line shown before its fetch, enough
# refreshes (tools/tests/sim/tb_fb_sdram.sv).
#
#   tools/tests/test_fb_sdram.sh [read-delay]
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
desktop="$root/fpga/desktop"
delay="${1:-2}"
command -v verilator >/dev/null 2>&1 || { echo "test_fb_sdram: verilator not found" >&2; exit 1; }
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
verilator --binary --timing -j 0 -Wno-fatal -Wno-DECLFILENAME -Wno-WIDTHEXPAND \
    -Wno-WIDTHTRUNC -Wno-UNOPTFLAT -Wno-CASEINCOMPLETE -Wno-BLKANDNBLK \
    -Wno-MULTIDRIVEN -Wno-INITIALDLY -Wno-REALCVT \
    --top-module tb_fb_sdram -GREAD_DELAY="$delay" \
    --Mdir "$work" -o tb \
    "$root/tools/tests/sim/tb_fb_sdram.sv" \
    "$root/tools/tests/sim/sdram_w9825_model.sv" \
    "$desktop/fb_sdram.sv" > "$work/build.log" 2>&1 || { tail -60 "$work/build.log"; exit 1; }
(cd "$work" && ./tb) | tee "$work/run.log"
grep -q 'fb_sdram: PASS' "$work/run.log"
