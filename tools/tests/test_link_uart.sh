#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# The desktop link's UART (fpga/desktop/link_uart.sv) at 2, 5 and 10 Mbaud:
# skewed back-to-back receive, exact transmit bit times, framing errors,
# glitches and the 3 us fallback to 2 Mbaud (tools/tests/sim/tb_link_uart.sv).
#
#   tools/tests/test_link_uart.sh
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
command -v verilator >/dev/null 2>&1 || { echo "test_link_uart: verilator not found" >&2; exit 1; }
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
verilator --binary --timing -j 0 -Wno-fatal -Wno-DECLFILENAME -Wno-WIDTHEXPAND \
    -Wno-WIDTHTRUNC -Wno-UNUSEDSIGNAL -Wno-SYNCASYNCNET \
    --top-module tb_link_uart --Mdir "$work" -o tb \
    "$root/tools/tests/sim/tb_link_uart.sv" \
    "$root/fpga/desktop/link_uart.sv" > "$work/build.log" 2>&1 || { tail -60 "$work/build.log"; exit 1; }
"$work/tb" | tee "$work/run.log"
grep -q 'link_uart: PASS' "$work/run.log"
