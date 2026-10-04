#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Simulate the composite: the desktop layer against the scaler and the picture.
#
# Verifies the one requirement no other test covers -- that the layer fills the
# whole output rather than sitting inside bars the way the legacy page does.
#
#   tools/tests/test_composite.sh
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"

nestang="${NESTANG_DIR:-}"
if [[ -z "$nestang" ]]; then
    for candidate in "$root/../tangcore/nestang" "/run/media/vash/GIT/tangcore/nestang"; do
        if [[ -d "$candidate" ]]; then
            nestang="$(cd -- "$candidate" && pwd)"
            break
        fi
    done
fi

if [[ ! -f "$nestang/src/nes2hdmi.sv" || ! -f "$nestang/src/iosys/textdisp_wide.sv" ]]; then
    echo "test_composite: sources not found under $nestang; is the patch applied?" >&2
    exit 1
fi

if ! command -v verilator >/dev/null 2>&1; then
    echo "test_composite: verilator not found" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Everything under hdmi2 except the audio clock regeneration packet, which
# Verilator cannot fold; tools/tests/sim/acr_packet_stub.sv stands in for it.
hdmi_srcs=()
for f in "$nestang"/src/hdmi2/*.sv; do
    [[ "$(basename "$f")" == "audio_clock_regeneration_packet.sv" ]] && continue
    hdmi_srcs+=("$f")
done

verilator --binary --timing -j 0 \
    -Wall -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-UNDRIVEN \
    -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC -Wno-BLKSEQ -Wno-VARHIDDEN \
    -Wno-CASEINCOMPLETE -Wno-LATCH -Wno-SYNCASYNCNET -Wno-PINNOTFOUND \
    -Wno-UNOPTFLAT -Wno-MULTIDRIVEN -Wno-BLKANDNBLK \
    --top-module tb_composite \
    +incdir+"$nestang/src/assets" \
    +incdir+"$nestang/src/iosys" \
    --Mdir "$work" \
    -o tb \
    "$root/tools/tests/sim/tb_composite.sv" \
    "$root/tools/tests/sim/acr_packet_stub.sv" \
    "$root/tools/tests/sim/elvds_obuf_stub.sv" \
    "$nestang/src/nes2hdmi.sv" \
    "$nestang/src/iosys/textdisp_wide.sv" \
    "${hdmi_srcs[@]}"

# nes2hdmi loads its frame buffer with $readmemb from the working directory.
cp "$nestang/src/assets/background.txt" "$work/"

cd "$work"
./tb
