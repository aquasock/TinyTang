#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# The desktop core's timing gate (tools/desktop_open/pnr_timing.py) must judge a
# build by its routed result.  nextpnr reports every clock twice -- an estimate
# after placement, then the routed figure -- and prints a failing clock as
# `Warning:`, not `Info:`.  The gate once matched `Info:` alone, so a routed
# failure left the placement estimate standing and every setup failure passed
# (core-log entry 46).  Three logs pin that down; the third has nextpnr call a
# slow clock PASS, which the gate must not take on trust.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
gate="$root/tools/desktop_open/pnr_timing.py"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

clocks() {   # $1 = the routed pixel-clock line
    cat <<LOG
Info: Max frequency for clock 'desktop_sockets.pixel_clk': 90.20 MHz (PASS at 74.25 MHz)
Info: Max frequency for clock                       'clk': 80.00 MHz (PASS at 21.49 MHz)
Info: Routing complete.
$1
Info: Max frequency for clock                       'clk': 76.13 MHz (PASS at 21.49 MHz)
Info: Program finished normally.
LOG
}
clocks "Warning: Max frequency for clock 'desktop_sockets.pixel_clk': 72.65 MHz (FAIL at 74.25 MHz)" > "$work/routed-fail.log"
clocks "Info: Max frequency for clock 'desktop_sockets.pixel_clk': 75.91 MHz (PASS at 74.25 MHz)" > "$work/routed-pass.log"
clocks "Info: Max frequency for clock 'desktop_sockets.pixel_clk': 73.00 MHz (PASS at 74.25 MHz)" > "$work/routed-slow.log"

fails=0
check() {    # $1 = log, $2 = the exit code the gate must give, $3 = the figure it must report
    local out rc=0
    out="$(python3 "$gate" --require-clock desktop_sockets.pixel_clk=74.25 "$work/$1" 2>&1)" || rc=$?
    if [[ $rc -ne $2 ]] || ! grep -q "pixel_clk *$3" <<<"$out"; then
        echo "FAIL: $1: exit $rc (want $2), report:"; echo "$out" | sed 's/^/    /'
        fails=$((fails + 1))
    else
        echo "ok: $1: exit $rc, pixel clock $3 MHz"
    fi
}
check routed-fail.log 1 72.65
check routed-pass.log 0 75.91
check routed-slow.log 1 73.00
[[ $fails -eq 0 ]] && echo "pnr timing gate: PASS" || { echo "pnr timing gate: FAIL"; exit 1; }
