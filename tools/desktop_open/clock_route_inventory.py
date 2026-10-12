#!/usr/bin/env python3
"""How was every clock net in this nextpnr log actually routed?

    tools/clock-route-inventory.py [--json] <log> [more.log ...]
    tools/clock-route-inventory.py --write-profile <out.json> <log>
    tools/clock-route-inventory.py --compare <profile.json> <log>

`tools/pnr-timing.py` answers "did anything violate a constraint?".  This tool
answers the other half: "did each clock reach its sinks over the dedicated
clock network, or was it quietly put somewhere else?"  On the GW5AST-138C the
second question matters, because the chipdb's clock model is incomplete, so
nextpnr can complete a build whose clocks are on general fabric -- or on
"global resources only", which carries no warning at all -- and report nothing.

A nextpnr log describes each clock net's routing in one of four ways:

    Info:     'NAME' net was routed.
        routed on the dedicated clock network -- the good case.

    Info:     'NAME' net was routed using global resources only.
        completed, but on the global-resource fabric, not the clock spine.
        There is NO warning for this; a gate that reads warnings misses it.

    Info:     'NAME' net was routed but not connected end to end; leaving it to
              the router.
        partially routed; a later "was routed." line may still appear, so a
        check that only looks for the good line is fooled.

    Warning: Failed to route net 'NAME' from A to B using dedicated routing.
        one line PER SINK.  This is what produced the "943 missing wires": 943
        sinks of the single net `clk`, not 943 nets.

## Why this is a *regression* check, not an all-dedicated check

On this die a PLL's clock reaches the clock plane through fabric and a logic
gate -- the fork's own measured note says so -- and no clock pip anywhere is
sourced from a PLL output wire.  So "every clock dedicated" is a bar **no build
on record meets**: the hardware-proven baseline has `clk` fabric for 943 sinks,
`hclk5` 4, `clk27` 1, and the 74.25 MHz pixel clock on global resources.  A
gate demanding all-dedicated refuses every build by construction, which is a
wall, not a check.

So the default is to *record* the topology, and `--compare` fails only when a
build is **worse than the recorded profile** -- more fallbacks, or a net that
was dedicated no longer is, or a clock that is not in the profile at all shows
up on non-dedicated resources.  That is what "explicit and recorded" means here.
It does NOT prove a clock works; only hardware does, and a topology that
matches the proven build exactly is exactly as unverified as that build is.

Exit code: 0 when nothing regressed (or no --compare was asked for and every
clock is dedicated), 1 on regression, 2 on an unreadable log or profile.

SPDX-License-Identifier: MIT
"""

import argparse
import json
import re
import sys
from collections import OrderedDict

ROUTED = re.compile(r"^Info:\s+'([^']+)' net was routed\.$")
GLOBAL = re.compile(r"^Info:\s+'([^']+)' net was routed using global resources only\.$")
PARTIAL = re.compile(
    r"^Info:\s+'([^']+)' net was routed but not connected end to end; leaving it to the router\.$")
FALLBACK = re.compile(
    r"^Warning: Failed to route net '([^']+)' from .* using dedicated routing\.$")
FMAX = re.compile(
    r"^Info: Max frequency for clock\s+'([^']+)':\s+([\d.]+) MHz \((PASS|FAIL) at ([\d.]+) MHz\)$")


def inventory(path):
    """Return {net: record} for one log, plus the log's total line count."""
    nets = OrderedDict()

    def rec(name):
        return nets.setdefault(name, {
            "routed": False, "global_only": False, "partial": False,
            "fallbacks": 0, "fmax": None, "constraint": None})

    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    for line in lines:
        m = FALLBACK.match(line)
        if m:
            rec(m.group(1))["fallbacks"] += 1
            continue
        m = PARTIAL.match(line)
        if m:
            rec(m.group(1))["partial"] = True
            continue
        m = GLOBAL.match(line)
        if m:
            rec(m.group(1))["global_only"] = True
            continue
        m = ROUTED.match(line)
        if m:
            rec(m.group(1))["routed"] = True
            continue
        m = FMAX.match(line)
        if m:
            # Placement prints an estimate and routing prints the final value;
            # the last one seen is the post-route number, which is what counts.
            r = rec(m.group(1))
            r["fmax"], r["constraint"] = float(m.group(2)), float(m.group(4))
            continue

    for r in nets.values():
        r["dedicated"] = r["routed"] and not r["global_only"] \
            and not r["partial"] and r["fallbacks"] == 0

    return nets, len(lines)


def verdict(record):
    if record["fallbacks"]:
        return "fabric (%d sink%s)" % (record["fallbacks"],
                                       "" if record["fallbacks"] == 1 else "s")
    if record["partial"]:
        return "partial (not connected end to end)"
    if record["global_only"]:
        return "global-resources-only"
    if record["routed"]:
        return "dedicated"
    return "not routed"


def report(path, nets, as_json):
    bad = sorted(n for n, r in nets.items() if not r["dedicated"])
    if as_json:
        print(json.dumps({
            "log": path,
            "nets": {n: {**r, "verdict": verdict(r)} for n, r in nets.items()},
            "non_dedicated": bad, "ok": not bad,
        }, indent=2))
        return
    print("=== %s ===" % path)
    width = max(len(n) for n in nets) if nets else 4
    print("  %-*s  %-9s  %8s  %11s  %s"
          % (width, "net", "binding", "fallbacks", "final MHz", "constraint MHz"))
    for name, r in nets.items():
        fmax = "%8.2f" % r["fmax"] if r["fmax"] is not None else "%8s" % "-"
        cons = "%11.2f" % r["constraint"] if r["constraint"] is not None \
            else "%11s" % "-"
        print("  %-*s  %-9s  %8d  %s  %s"
              % (width, name, "yes" if r["dedicated"] else "NO",
                 r["fallbacks"], fmax, cons))
    for name in nets:
        print("  %-*s  %s" % (width, name, verdict(nets[name])))
    if bad:
        print()
        print("  %d of %d clock net(s) do not ride the dedicated network: %s"
              % (len(bad), len(nets), ", ".join(bad)))


def profile_of(nets):
    """The comparable part of a run, for --write-profile / --compare."""
    return {n: {"dedicated": r["dedicated"], "fallbacks": r["fallbacks"],
                "global_only": r["global_only"], "partial": r["partial"]}
            for n, r in nets.items()}


def regressions(profile, nets):
    """Nets that are worse than the recorded profile."""
    bad = []
    for name, r in nets.items():
        base = profile.get(name)
        if base is None:
            if not r["dedicated"]:
                bad.append("%s: not in the recorded profile and not dedicated (%s)"
                           % (name, verdict(r)))
            continue
        why = []
        if r["fallbacks"] > base["fallbacks"]:
            why.append("fabric fallbacks %d -> %d" % (base["fallbacks"], r["fallbacks"]))
        if base["dedicated"] and not r["dedicated"]:
            why.append("was dedicated, now %s" % verdict(r))
        if base["global_only"] is False and r["global_only"]:
            why.append("now routed on global resources only")
        if base["partial"] is False and r["partial"]:
            why.append("now only partially connected")
        if why:
            bad.append("%s: %s" % (name, "; ".join(why)))
    return bad


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("log")
    p.add_argument("--json", action="store_true", help="emit JSON instead of a table")
    p.add_argument("--write-profile", metavar="OUT.json",
                   help="record this log's clock profile for later --compare")
    p.add_argument("--compare", metavar="PROFILE.json",
                   help="fail if this log is worse than the recorded profile")
    args = p.parse_args(argv[1:])

    try:
        nets, _lines = inventory(args.log)
    except OSError as e:
        print("%s: %s" % (args.log, e), file=sys.stderr)
        return 2
    if not nets:
        print("=== %s ===" % args.log)
        print("  no clock-net routing lines found -- wrong log?")
        return 2

    if args.write_profile:
        with open(args.write_profile, "w") as fh:
            json.dump({"source": args.log, "nets": profile_of(nets)}, fh, indent=2)
            fh.write("\n")
        print("wrote %s (%d clock nets)" % (args.write_profile, len(nets)))
        if not args.compare:
            return 0

    if args.compare:
        try:
            profile = json.load(open(args.compare))["nets"]
        except (OSError, KeyError, ValueError) as e:
            print("%s: bad profile: %s" % (args.compare, e), file=sys.stderr)
            return 2
        report(args.log, nets, args.json)
        bad = regressions(profile, nets)
        print()
        if bad:
            print("  CLOCK TOPOLOGY REGRESSED against %s:" % args.compare)
            for b in bad:
                print("    - %s" % b)
            return 1
        print("  no clock-topology regression against %s" % args.compare)
        print("  (this is not proof the clocks work -- the profile is the"
              " hardware-proven build's own unverified topology)")
        return 0

    report(args.log, nets, args.json)
    if args.json:
        return 0
    # With no profile to compare, the only honest pass is all-dedicated.
    return 0 if all(r["dedicated"] for r in nets.values()) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
