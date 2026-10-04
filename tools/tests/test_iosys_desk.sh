#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Simulate the core's command decode for the desktop layer.
#
# The firmware side is tested on the host and the layer module in simulation;
# this covers what sits between them -- `iosys_bl616` turning bytes off UART1
# into writes on the layer's port, at the rate the link actually runs.  A
# mistake there writes the right cells in the wrong places, which on hardware
# looks like a subtly scrambled desktop and explains itself nowhere.
#
#   tools/tests/test_iosys_desk.sh
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

for f in src/iosys/iosys_bl616.v src/iosys/textdisp_wide.sv; do
    if [[ ! -f "$nestang/$f" ]]; then
        echo "test_iosys_desk: $nestang/$f not found; is the patch applied?" >&2
        exit 1
    fi
done

if ! command -v verilator >/dev/null 2>&1; then
    echo "test_iosys_desk: verilator not found" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

verilator --binary --timing -j 0 \
    -Wall -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-UNDRIVEN \
    -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC -Wno-BLKSEQ -Wno-VARHIDDEN \
    -Wno-CASEINCOMPLETE -Wno-LATCH -Wno-SYNCASYNCNET -Wno-PINNOTFOUND \
    -Wno-ASSIGNIN \
    --top-module tb_iosys_desk \
    +incdir+"$nestang/src/assets" \
    +incdir+"$nestang/src/iosys" \
    --Mdir "$work" \
    -o tb \
    "$root/tools/tests/sim/tb_iosys_desk.sv" \
    "$root/tools/tests/sim/dpb_model.sv" \
    "$nestang/src/iosys/iosys_bl616.v" \
    "$nestang/src/iosys/uart_fixed.v" \
    "$nestang/src/iosys/textdisp.v" \
    "$nestang/src/iosys/gowin_dpb_menu.v" \
    "$nestang/src/iosys/textdisp_wide.sv"

"$work/tb"
