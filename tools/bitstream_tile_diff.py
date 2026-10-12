#!/usr/bin/env python3
"""Compare two GW5AST-138C bitstreams tile by tile with the apicula fork's decoder.

    tools/bitstream_tile_diff.py A.fs B.fs [--tiles R82C182,R105C182 ...] [--all]

Without --tiles it lists every tile whose fuses differ, with the tile type and
the number of fuses set only in A and only in B.  Logic-cell tiles (types the
chip repeats thousands of times) are summarised as one count unless --all is
given, because a vendor bitstream sets default fuses in all of them.

With --tiles it decodes each named tile in both bitstreams with gowin_unpack's
own parse_tile_() and prints the cells and attributes, routing pips and clock
pips found only in A, only in B, and in both.

Run it under the interpreter and PYTHONPATH the open flow uses
(tools/desktop_open/build.sh): oss-cad-suite's python3 with the apicula fork
first on PYTHONPATH.

SPDX-License-Identifier: MIT
"""

import argparse
import collections
import importlib.resources
import re
import sys
import warnings

warnings.filterwarnings("ignore")

from apycula import chipdb, gowin_unpack  # noqa: E402
from apycula.bslib import read_bitstream  # noqa: E402

DEVICE = "GW5AST-138C"


def load(db, path):
    bitmap, _, _, _ = read_bitstream(path)
    return chipdb.tile_bitmap(db, bitmap)


def fuses(tile):
    return {(r, c) for r, row in enumerate(tile) for c, bit in enumerate(row) if bit}


def decode(db, bm, row, col, shape_from=None):
    tile = bm.get((row, col))
    if tile is None:
        # A tile with no fuses set can be absent from one bitmap.
        ref = shape_from[(row, col)]
        tile = [[0] * len(ref[0]) for _ in ref]
    bels, pips, clock_pips = gowin_unpack.parse_tile_(db, row, col, tile, bm, noiostd=False)
    out = set()
    for name, bel in bels.items():
        if isinstance(bel, dict):
            items = bel.items()
        else:
            items = [("", bel)]
        for key, val in items:
            out.add(f"bel {name} {key}={sorted(val) if isinstance(val, set) else val}")
    for dest, src in pips.items():
        out.add(f"pip {src} -> {dest}")
    for dest, src in clock_pips.items():
        out.add(f"clk {src} -> {dest}")
    return out


def parse_rc(text):
    m = re.fullmatch(r"R(\d+)C(\d+)", text.strip())
    if not m:
        raise SystemExit(f"bad tile name {text!r}; use R<row>C<col>, 1-based")
    return int(m.group(1)) - 1, int(m.group(2)) - 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--tiles", default="")
    ap.add_argument("--all", action="store_true")
    args = ap.parse_args()

    with importlib.resources.path("apycula", f"{DEVICE}.msgpack.xz") as path:
        db = chipdb.load_chipdb(path)
    gowin_unpack._device = DEVICE
    gowin_unpack._pinout = db.pinout[DEVICE][gowin_unpack._packages[DEVICE]]
    bm_a, bm_b = load(db, args.a), load(db, args.b)

    if args.tiles:
        for name in args.tiles.split(","):
            row, col = parse_rc(name)
            da, db_ = decode(db, bm_a, row, col, bm_b), decode(db, bm_b, row, col, bm_a)
            fa, fb = fuses(bm_a.get((row, col), [])), fuses(bm_b.get((row, col), []))
            print(f"=== R{row + 1}C{col + 1} ttyp {db[row, col].ttyp}: "
                  f"fuses only A {len(fa - fb)}, only B {len(fb - fa)}, both {len(fa & fb)}")
            for label, items in (("only A", da - db_), ("only B", db_ - da), ("both", da & db_)):
                for item in sorted(items):
                    print(f"  {label:6s} {item}")
        return

    keys = set(bm_a) | set(bm_b)
    ttyp_count = collections.Counter(db[r, c].ttyp for (r, c) in keys)
    common = {t for t, n in ttyp_count.items() if n > 500}
    folded = collections.Counter()
    # The same (only-A, only-B) fuse sets in many tiles of one type is a
    # chip-wide default, not a design difference: group those.
    shapes = collections.defaultdict(list)
    for (row, col) in sorted(keys):
        fa, fb = fuses(bm_a.get((row, col), [])), fuses(bm_b.get((row, col), []))
        if fa == fb:
            continue
        ttyp = db[row, col].ttyp
        if ttyp in common and not args.all:
            folded[ttyp] += 1
            continue
        shapes[(ttyp, frozenset(fa - fb), frozenset(fb - fa))].append((row, col))
    for (ttyp, only_a, only_b), tiles in sorted(shapes.items(), key=lambda kv: kv[1][0]):
        names = " ".join(f"R{r + 1}C{c + 1}" for r, c in tiles)
        if len(tiles) > 4 and not args.all:
            names = f"{len(tiles)} tiles, first {names.split()[0]}"
        print(f"ttyp {ttyp}: only A {len(only_a)}, only B {len(only_b)}: {names}")
    for ttyp, n in sorted(folded.items()):
        print(f"(ttyp {ttyp}: {n} differing tiles of a type repeated {ttyp_count[ttyp]} times, "
              f"not listed; --all lists them)")


if __name__ == "__main__":
    sys.exit(main())
