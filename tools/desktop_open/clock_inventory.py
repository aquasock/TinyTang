#!/usr/bin/env python3
"""Inventory the GW5AST-138C clock data: vendor tables vs. the apicula model.

Read-only.  Loads the vendor .fse/.dat (GOWINHOME) and the apicula chipdb
(GW5AST-138C.msgpack.xz), then reports:

  * the raw CLOCK_MUX / CLOCK_MUX_TOP / CLOCK_MUX_BOTTOM tables for the
    central bridge tile types;
  * the pips the fork's fse_clock_pips_138() keeps from them;
  * the .dat SpineColumn and the clock-related gw5aStuff keys;
  * the chipdb's clock/hclk resource counts.

The script is the inventory instrument for the clock-tree cycle.  It does not
change the fork or the chipdb; it names the raw entries the model filters so
the next step can prove or replace each filter against the vendor data.

Usage:
    GOWINHOME=/path/to/gowin-1.9.11.03 \\
    PYTHONPATH=/path/to/apicula-fork:/path/to/pydeps \\
    python3 tools/desktop_open/clock_inventory.py [--out FILE]

SPDX-License-Identifier: MIT
"""

import argparse
import os
import sys
from collections import Counter
from pathlib import Path

from apycula import fse_parser, wirenames as wnames, chipdb

DEVICE = "GW5AST-138C"
BRIDGE_TILES = (80, 81, 82, 83, 84, 85)
TABLES = {
    38: "CLOCK_MUX",
    90: "CLOCK_MUX_TOP",
    91: "CLOCK_MUX_BOTTOM",
}
SKIP_RANGES = (
    ("P16A..P47D", lambda i: wnames.clknumbers["P16A"] <= i <= wnames.clknumbers["P47D"]),
    ("longwire 164..236", lambda i: 164 <= i <= 236),
    ("unknown 105..128", lambda i: 105 <= i <= 128),
    ("253..268", lambda i: 253 <= i <= 268),
)


def name(i):
    return wnames.clknames.get(i, f"UNK{i}")


def classify(srcid, destid, ttyp, kept):
    """Why a raw entry is or is not in the fork's kept pips."""
    src, dest = name(srcid), name(destid)
    for label, test in SKIP_RANGES:
        if test(srcid):
            return label
    if srcid == 277:
        return "VCC"
    if src.startswith("SPINE") and destid not in (291, 292):
        return "spine source, non-GT sink"
    if dest.startswith("SPINE") and ttyp not in BRIDGE_TILES:
        if not src.startswith("CBRIDGEOUT"):
            return "non-bridge source"
        if (dest, src) in chipdb._gw5ast138c_dead_bridge_pips:
            return "measured-dead bridge pip"
        return "bridge source, filter/permutation"
    if (dest, src) in kept:
        return "kept"
    return "other"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--gowinhome", default=os.environ.get("GOWINHOME"))
    ap.add_argument("--chipdb",
                    default=os.path.join(os.path.dirname(__file__),
                                         "../../../apicula/GW5AST-138C.msgpack.xz"))
    args = ap.parse_args()
    if not args.gowinhome:
        sys.exit("GOWINHOME is not set")

    out = open(args.out, "w") if args.out else sys.stdout
    def p(*a):
        print(*a, file=out)

    fse_path = os.path.join(args.gowinhome, "IDE/share/device", DEVICE,
                            DEVICE + ".fse")
    dat_path = os.path.join(args.gowinhome, "IDE/share/device", DEVICE,
                            DEVICE + ".dat")
    wnames.select_wires(DEVICE)

    p("== vendor data ==")
    p("GOWINHOME", args.gowinhome)
    p("fse", fse_path)
    p("dat", dat_path)
    with open(fse_path, "rb") as f:
        fse = fse_parser.read_fse(f, DEVICE)
    p("tile types in fse", len(fse))

    # ---------------------------------------------------- clock mux tables --
    for ttyp in BRIDGE_TILES:
        if ttyp not in fse:
            continue
        kept = chipdb.fse_clock_pips_138(fse, ttyp, DEVICE)
        p(f"\n== tile type {ttyp} ==")
        p("kept pips", sum(len(v) for v in kept.values()), "over", len(kept), "dests")
        for table, tname in TABLES.items():
            rows = fse[ttyp].get("wire", {}).get(table, [])
            if not rows:
                continue
            reasons = Counter()
            samples = {}
            for row in rows:
                srcid, destid = row[0], row[1]
                why = classify(srcid, destid, ttyp, kept)
                reasons[why] += 1
                samples.setdefault(why, []).append(
                    f"{name(srcid)} -> {name(destid)} (src {srcid}, dst {destid})")
            p(f"  {tname} ({table}): {len(rows)} raw rows")
            for why, n in reasons.most_common():
                p(f"    {n:5d} {why}")
                for s in samples[why][:3]:
                    p(f"          e.g. {s}")

    # ------------------------------------------------------------ vendor dat --
    from apycula import dat_parser
    dat = dat_parser.Datfile(Path(dat_path))
    p("\n== vendor .dat ==")
    keys = [k for k in getattr(dat, "gw5aStuff", {}) if
            any(w in k.lower() for w in ("spine", "hclk", "clock", "mux", "pll"))]
    p("gw5aStuff clock keys", keys)
    stuff = getattr(dat, "gw5aStuff", {})
    for k in ("CMuxTopInNodes", "CMuxBotInNodes", "CMuxTopIns", "CMuxBotIns",
              "SpineColumn", "PllIn", "PllOut"):
        v = stuff.get(k)
        if v is None:
            continue
        if hasattr(v, "__len__"):
            p(k, "len", len(v), "sample", list(v)[:16])
        else:
            p(k, v)

    # -------------------------------------------------------------- chipdb --
    if os.path.exists(args.chipdb):
        db = chipdb.load_chipdb(args.chipdb)
        p("\n== apicula chipdb ==")
        p("tile types", len(db.tiles),
          "clock_pips", sum(len(getattr(t, "clock_pips", {})) for t in db.tiles.values()))
        for attr in ("hclk_pips", "spine_select_wires", "io2hclk", "hclk_div2"):
            v = getattr(db, attr, None)
            p(attr, "absent" if v is None else (len(v) if hasattr(v, "__len__") else v))
    else:
        p("\n(chipdb not found at %s)" % args.chipdb)

    if args.out:
        out.close()


if __name__ == "__main__":
    main()
