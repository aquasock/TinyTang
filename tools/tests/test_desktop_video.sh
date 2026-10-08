#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
source_dir="${NESTANG_DIR:?Set NESTANG_DIR to a reconstructed desktop source tree}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
hdmi_srcs=()
for source in "$source_dir"/src/hdmi2/*.sv; do
    [[ "$(basename "$source")" == audio_clock_regeneration_packet.sv ]] && continue
    hdmi_srcs+=("$source")
done
verilator --binary --timing -j 4 -Wno-fatal -Wno-DECLFILENAME \
    -Wno-PINNOTFOUND -Wno-PINMISSING -Wno-BLKANDNBLK -Wno-MULTIDRIVEN \
    -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC -Wno-UNOPTFLAT \
    -DDESKTOP_CORE -DMENU_CORE --top-module tb_desktop_video \
    +incdir+"$source_dir/src/assets" +incdir+"$source_dir/src/iosys" \
    --Mdir "$work" -o tb \
    "$root/tools/tests/sim/tb_desktop_video.sv" \
    "$root/tools/tests/sim/acr_packet_stub.sv" \
    "$root/tools/tests/sim/elvds_obuf_stub.sv" \
    "$source_dir/src/nes2hdmi.sv" "$source_dir/src/iosys/textdisp_wide.sv" \
    "$root/fpga/desktop/desktop_pmod.sv" "${hdmi_srcs[@]}" > "$work/build.log" 2>&1 || \
    { tail -80 "$work/build.log"; exit 1; }
cp "$source_dir/src/assets/background.txt" "$work/"
(cd "$work" && ./tb)
