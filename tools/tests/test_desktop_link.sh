#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Simulate the core's command decode for the desktop layer.
#
# The firmware side is tested on the host and the layer module in simulation;
# this covers what sits between them -- `desktop_link` turning bytes off UART1
# into writes on the layer's port, at the rate the link actually runs.  A
# mistake there writes the right cells in the wrong places, which on hardware
# looks like a subtly scrambled desktop and explains itself nowhere.
#
#   tools/tests/test_desktop_link.sh
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"

desktop="$root/fpga/desktop"

if ! command -v verilator >/dev/null 2>&1; then
    echo "test_desktop_link: verilator not found" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

verilator --binary --timing -j 0 \
    -Wall -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-UNDRIVEN \
    -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC -Wno-BLKSEQ -Wno-VARHIDDEN \
    -Wno-CASEINCOMPLETE -Wno-LATCH -Wno-SYNCASYNCNET -Wno-PINNOTFOUND \
    -Wno-ASSIGNIN \
    --top-module tb_desktop_link \
    +incdir+"$desktop" \
    --Mdir "$work" \
    -o tb \
    "$root/tools/tests/sim/tb_desktop_link.sv" \
    "$desktop/desktop_link.v" \
    "$desktop/uart_fixed.v" \
    "$desktop/desktop_regs.sv" \
    "$desktop/clock_monitor.sv"

"$work/tb"
