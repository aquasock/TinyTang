#!/usr/bin/env python3
"""Read a nextpnr log and report its timing: what passed, what violated.

    tools/pnr-timing.py build/pnr.log [more.log ...]
        --require-clock desktop_sockets.pixel_clk=74.25

Exits 0 when every clock passed and there is no setup or hold violation, 1 when
anything violated, 2 when the log could not be read at all.  That exit code is
the point: the flow currently passes `--timing-allow-fail`, so nextpnr reports a
violation and then writes the bitstream anyway.  A build that ships with a hold
violation is a build that works by luck, so the acceptance criterion for a
desktop-core bitstream is this tool's exit code rather than nextpnr's.

Why the log and not the JSON report: `--report` writes `fmax`, `critical_paths`
and `utilization`, and `critical_paths` holds the worst *setup* paths only.
Hold violations appear in no structured field -- they are text in the log -- and
hold is the failure this core actually has. Required-clock checks also reject a
missing domain or a clock inadvertently checked at the 12 MHz default.

This tool sees setup/hold violations and the clocks it is asked about. It does
NOT prove a clock reached the dedicated network, and on this device that is the
whole question -- `tools/clock-route-inventory.py` answers it for every clock at
once and is the topology half of the gate.

A violation block looks like this, one per violated path:

    Warning: Hold/min time violation for clock 'posedge clk':
    Info:       type curr  total name
    Info:   clk-skew -1.76 -1.76 Net clk (150,64) -> (150,52)
    Info:                          Sink sys_inst.desktop_endpoint.response_DFFRE_Q_23.CLK
    Info:   clk-to-q  0.25 -1.51 Source sys_inst.desktop_endpoint.state_DFF_Q_2.Q
    Info:    routing  0.92 -0.58 Net ...
    Info:                          Sink ...

The last row's `total` is the path's slack, negative when it violates; the rows'
`curr` values are the per-term contributions, and on a clocked path the
`clk-skew` row is usually the largest -- which is the tell for a clock on
general fabric rather than the dedicated network.

SPDX-License-Identifier: MIT
"""

import argparse
import math
import re
import sys
from collections import defaultdict

VIO = re.compile(r"^Warning: (Setup|Hold)/min time violation for clock '([^']+)':")
ROW = re.compile(r"^Info:\s+(\S+)\s+(-?\d+\.\d+)\s+(-?\d+\.\d+)\s+(.*)$")
FMAX = re.compile(r"^Info: Max frequency for clock\s+'([^']+)':\s+([\d.]+) MHz \((PASS|FAIL) at ([\d.]+) MHz\)")


def analyse(path):
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    viols = defaultdict(lambda: {"setup": 0, "hold": 0, "worst": 0.0, "skew": 0.0})
    fmax = {}

    for i, line in enumerate(lines):
        m = FMAX.match(line)
        if m:
            name, achieved, verdict, constraint = m.groups()
            fmax[name] = (float(achieved), float(constraint), verdict)
            continue

        m = VIO.match(line)
        if not m:
            continue

        kind, clock = m.groups()
        v = viols[clock]
        v["hold" if kind == "Hold" else "setup"] += 1

        # Walk the block's rows: the last numeric `total` is the path slack and
        # negative means it violated; `clk-skew` is the tell for a clock that
        # ended up on general fabric.
        last_total = None
        j = i + 1
        while j < len(lines):
            nxt = lines[j]
            if VIO.match(nxt) or FMAX.match(nxt):
                break
            if not nxt.startswith("Info:"):
                break
            row = ROW.match(nxt)
            if row:
                term, _curr, total, _rest = row.groups()
                last_total = float(total)
                if term == "clk-skew":
                    v["skew"] = min(v["skew"], float(total))
            j += 1
        if last_total is not None:
            v["worst"] = min(v["worst"], last_total)

    return fmax, viols


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+")
    parser.add_argument("--require-clock", action="append", default=[],
                        metavar="NAME=MHZ", help="require a reported clock at this frequency")
    parser.add_argument("--require-dedicated-clock", action="append", default=[],
                        metavar="NAME",
                        help="require a completed dedicated route with no fabric fallback "
                             "and no partial connection (see tools/clock-route-inventory.py "
                             "to check every clock at once)")
    args = parser.parse_args(argv[1:])
    required = {}
    for item in args.require_clock:
        try:
            name, mhz = item.rsplit("=", 1)
            mhz = float(mhz)
            if not name or not math.isfinite(mhz) or mhz <= 0:
                raise ValueError()
            required[name] = mhz
        except ValueError:
            parser.error("--require-clock must be NAME=positive-MHz")

    failed = False
    for path in args.logs:
        file_failed = False
        try:
            fmax, viols = analyse(path)
        except OSError as e:
            print(f"{path}: {e}", file=sys.stderr)
            return 2

        print(f"=== {path} ===")
        if not fmax and not viols:
            print("  no timing sections found -- wrong log?")
            return 2

        for name in sorted(fmax):
            achieved, constraint, verdict = fmax[name]
            if verdict == "FAIL":
                file_failed = True
            print(f"  {verdict:4} {name:32} {achieved:8.2f} MHz  (constraint {constraint:g} MHz)")

        for name, mhz in required.items():
            if name not in fmax or abs(fmax[name][1] - mhz) > 0.02:
                file_failed = True
                actual = f"{fmax[name][1]:g} MHz" if name in fmax else "missing"
                print(f"  FAIL required clock {name}: expected {mhz:g} MHz, got {actual}")

        log = open(path, encoding="utf-8", errors="replace").read()
        for name in args.require_dedicated_clock:
            clock = re.escape(name)
            routed = re.search(rf"^Info:\s+'{clock}' net was routed\.$",
                               log, re.MULTILINE)
            fallback = re.search(rf"^Warning: Failed to route net '{clock}' .* using dedicated routing\.$",
                                 log, re.MULTILINE)
            # A clock that is only partly connected can still print a later
            # "was routed." line, so require the absence of the partial line as
            # well: presence of the good line alone does not prove the route.
            partial = re.search(
                rf"^Info:\s+'{clock}' net was routed but not connected end to end;",
                log, re.MULTILINE)
            passed = bool(routed) and not fallback and not partial
            file_failed |= not passed
            print(f"  {'PASS' if passed else 'FAIL'} dedicated clock {name}")

        total = 0
        n_setup = 0
        n_hold = 0
        worst_slack = 0.0
        worst_skew = 0.0
        for name in sorted(viols):
            v = viols[name]
            n = v["setup"] + v["hold"]
            total += n
            n_setup += v["setup"]
            n_hold += v["hold"]
            worst_slack = min(worst_slack, v["worst"])
            worst_skew = min(worst_skew, v["skew"])
            if n:
                file_failed = True
            print(f"  {'FAIL' if n else 'PASS'} {name:32} setup {v['setup']:3d}  hold {v['hold']:3d}"
                  f"  worst {v['worst']:+.3f} ns  clk-skew {v['skew']:+.3f} ns")

        if not viols:
            print("  (no setup/hold violation blocks in this log)")

        # One machine-readable line, because a clock name may contain a space
        # (`posedge clk`) and parsing the rows above by column is fragile.
        failed |= file_failed
        verdict = "fail" if file_failed else "pass"
        print(f"  summary: {verdict} setup={n_setup} hold={n_hold}"
              f" worst_slack={worst_slack:.3f} worst_skew={worst_skew:.3f}")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
