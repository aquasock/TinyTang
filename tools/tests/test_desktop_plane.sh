#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
#
# The desktop core's pixel plane end to end (desktop ABI 1.9): UART frames
# 0x18-0x1B into desktop_link, through fb_sdram and a timing-checking model of
# the dock's SDRAM, out of desktop_hdmi's raster, checked pixel by pixel and by
# the plane's own frame checksum (tools/tests/sim/tb_desktop_plane.sv).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
desktop="$root/fpga/desktop"
command -v verilator >/dev/null 2>&1 || { echo "test_desktop_plane: verilator not found" >&2; exit 1; }
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
hdmi_srcs=()
for source in "$desktop"/hdmi/*.sv; do
    [[ "$(basename "$source")" == audio_clock_regeneration_packet.sv ]] && continue
    hdmi_srcs+=("$source")
done
verilator --binary --timing -j 0 -Wno-fatal -Wno-DECLFILENAME \
    -Wno-PINNOTFOUND -Wno-PINMISSING -Wno-BLKANDNBLK -Wno-MULTIDRIVEN \
    -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC -Wno-UNOPTFLAT -Wno-CASEINCOMPLETE \
    -Wno-LATCH -Wno-ASSIGNIN \
    -I"$desktop" --top-module tb_desktop_plane \
    --Mdir "$work" -o tb \
    "$root/tools/tests/sim/tb_desktop_plane.sv" \
    "$root/tools/tests/sim/acr_packet_stub.sv" \
    "$root/tools/tests/sim/elvds_obuf_stub.sv" \
    "$desktop/desktop_link.v" "$desktop/link_uart.sv" \
    "$desktop/desktop_regs.sv" "$desktop/clock_monitor.sv" \
    "$desktop/desktop_hdmi.sv" "$desktop/textdisp_wide.sv" "$desktop/fb_sdram.sv" \
    "$root/tools/tests/sim/sdram_w9825_model.sv" \
    "${hdmi_srcs[@]}" > "$work/build.log" 2>&1 || { tail -60 "$work/build.log"; exit 1; }
(cd "$work" && ./tb) | tee "$work/run.log"
grep -q 'desktop plane: PASS' "$work/run.log"
# Again at 10 Mbaud (desktop ABI 1.10), with sixteen whole rows back to back.
verilator --binary --timing -j 0 -Wno-fatal -Wno-DECLFILENAME \
    -Wno-PINNOTFOUND -Wno-PINMISSING -Wno-BLKANDNBLK -Wno-MULTIDRIVEN \
    -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC -Wno-UNOPTFLAT -Wno-CASEINCOMPLETE \
    -Wno-LATCH -Wno-ASSIGNIN -GFAST=1 \
    -I"$desktop" --top-module tb_desktop_plane \
    --Mdir "$work/fast" -o tb \
    "$root/tools/tests/sim/tb_desktop_plane.sv" \
    "$root/tools/tests/sim/acr_packet_stub.sv" \
    "$root/tools/tests/sim/elvds_obuf_stub.sv" \
    "$desktop/desktop_link.v" "$desktop/link_uart.sv" \
    "$desktop/desktop_regs.sv" "$desktop/clock_monitor.sv" \
    "$desktop/desktop_hdmi.sv" "$desktop/textdisp_wide.sv" "$desktop/fb_sdram.sv" \
    "$root/tools/tests/sim/sdram_w9825_model.sv" \
    "${hdmi_srcs[@]}" > "$work/fast.log" 2>&1 || { tail -60 "$work/fast.log"; exit 1; }
(cd "$work/fast" && ./tb) | tee "$work/run_fast.log"
grep -q 'desktop plane: PASS' "$work/run_fast.log"
