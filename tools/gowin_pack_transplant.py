#!/usr/bin/env python3
"""Transplant a donor bitstream's unmodelled fuses into a gowin_pack run.

    tools/gowin_pack_transplant.py --donor DONOR.fs --list [--select SPEC]
    tools/gowin_pack_transplant.py --donor DONOR.fs [--select SPEC]
        [--pll-attr ATTR=VAL,...] -- <gowin_pack arguments>

Diagnostic for core-log entry 37.  A fuse is *unmodelled* when it is set in the
donor (a Gowin build) and appears in none of the chipdb's tables for its tile:
no pip, clock pip, alone-node, shortval, longval, longfuse or hclk pip of that
tile names it.  The fork's packer can never set such a fuse, so these are the
candidates for what a Gowin build carries and an open build cannot.

--list prints, per selected tile, how many unmodelled donor fuses it has.
Without --list the open build is packed by the fork's gowin_pack with the
selected tiles' unmodelled donor fuses OR-ed into its tilemap just before the
bitmap is written, so gowin_pack still computes every frame CRC and the footer
checksum itself.  Modelled fuses are never copied: the open build's own
routing and cell configuration are left exactly as the router made them.

SPEC selects tiles, comma-separated, any of:
    all              every tile with unmodelled donor fuses
    R<row>C<col>     one tile (1-based, as the decoders print)
    row=<r>[-<r2>]   whole rows (1-based)
    col=<c>[-<c2>]   whole columns (1-based)
    ttyp=<t>         one tile type
    !ttyp=<t>        exclude a tile type (applied last)
    !R<row>C<col>    exclude a tile (applied last)

--copy-empty copies ALL of the donor's fuses, modelled ones included, into
every tile the open build leaves with no fuse set: a tile the open design does
not use carries no signal of the open design, so the donor's routing and cell
settings there can only add what the donor had.  --copy-superset widens that
to every tile whose open fuses are a subset of the donor's, so nothing the open
design set is lost.  --copy-empty-ttyp limits it
to the given tile types (comma-separated); --copy-empty-skip-ttyp excludes some.

--pip R<row>C<col>:DEST<-SRC (repeatable) sets the fuses of one modelled pip
or clock pip, looked up in the chipdb, in a tile the open build does use.

--fuse R<row>C<col>:<r>,<c> (repeatable) sets one raw fuse, tile-relative, as
tools/bitstream_tile_diff.py and the decoders number them; --clear-fuse clears
one the same way.

--pll-attr adds raw PLL (attribute id, value id) pairs as
tools/gowin_pack_pll_attr.py does.

SPDX-License-Identifier: MIT
"""

import argparse
import importlib.resources
import re
import sys
import warnings

warnings.filterwarnings("ignore")

from apycula import chipdb, gowin_pack  # noqa: E402
from apycula.bslib import read_bitstream  # noqa: E402

DEVICE = "GW5AST-138C"


def coords(obj, out):
    """Collect every (row, col) fuse coordinate found in the values of obj."""
    if isinstance(obj, dict):
        for v in obj.values():
            coords(v, out)
    elif isinstance(obj, (set, frozenset, list, tuple)):
        items = list(obj)
        if items and all(isinstance(e, tuple) and len(e) == 2 and all(isinstance(x, int) for x in e)
                         for e in items):
            out.update(items)
        else:
            for e in items:
                coords(e, out)


def modelled(db, row, col, cache):
    td = db[row, col]
    key = (td.ttyp, (row, col) if (row, col) in db.hclk_pips else None)
    if key not in cache:
        out = set()
        for table in (td.pips, td.clock_pips, td.alonenode, getattr(td, "alonenode_6", {}),
                      db.shortval.get(td.ttyp, {}), (db.longval or {}).get(td.ttyp, {}),
                      (db.longfuses or {}).get(td.ttyp, {}), db.hclk_pips.get((row, col), {})):
            coords(table, out)
        cache[key] = out
    return cache[key]


def unmodelled(db, donor_bm):
    cache, res = {}, {}
    for (row, col), tile in donor_bm.items():
        known = modelled(db, row, col, cache)
        bits = {(r, c) for r, line in enumerate(tile) for c, b in enumerate(line) if b} - known
        if bits:
            res[(row, col)] = bits
    return res


def select(db, cand, spec):
    chosen, drop_t, drop_x = set(), set(), set()
    for term in [t.strip() for t in spec.split(",") if t.strip()]:
        neg = term.startswith("!")
        term = term.lstrip("!")
        m_rc = re.fullmatch(r"R(\d+)C(\d+)", term)
        m_rng = re.fullmatch(r"(row|col)=(\d+)(?:-(\d+))?", term)
        m_t = re.fullmatch(r"ttyp=(\d+)", term)
        if term == "all":
            hit = set(cand)
        elif m_rc:
            hit = {(int(m_rc.group(1)) - 1, int(m_rc.group(2)) - 1)} & set(cand)
        elif m_rng:
            lo = int(m_rng.group(2)) - 1
            hi = int(m_rng.group(3) or m_rng.group(2)) - 1
            axis = 0 if m_rng.group(1) == "row" else 1
            hit = {k for k in cand if lo <= k[axis] <= hi}
        elif m_t:
            hit = {k for k in cand if db[k].ttyp == int(m_t.group(1))}
        else:
            raise SystemExit(f"bad selector {term!r}")
        if neg:
            (drop_t if m_t else drop_x).update(hit)
        else:
            chosen |= hit
    return chosen - drop_t - drop_x


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--donor", required=True)
    ap.add_argument("--select", default="all")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--pll-attr", default="")
    ap.add_argument("--copy-empty", action="store_true")
    ap.add_argument("--copy-superset", action="store_true")
    ap.add_argument("--copy-empty-ttyp", default="")
    ap.add_argument("--copy-empty-skip-ttyp", default="")
    ap.add_argument("--pip", action="append", default=[])
    ap.add_argument("--fuse", action="append", default=[])
    ap.add_argument("--clear-fuse", action="append", default=[])
    ap.add_argument("pack_args", nargs=argparse.REMAINDER)
    args = ap.parse_args()

    with importlib.resources.path("apycula", f"{DEVICE}.msgpack.xz") as path:
        db = chipdb.load_chipdb(path)
    donor_bm = chipdb.tile_bitmap(db, read_bitstream(args.donor)[0])
    cand = unmodelled(db, donor_bm)
    chosen = select(db, cand, args.select)
    total = sum(len(cand[k]) for k in chosen)
    print(f"transplant: {len(chosen)} tiles, {total} unmodelled donor fuses "
          f"(of {len(cand)} tiles, {sum(len(v) for v in cand.values())} fuses)", file=sys.stderr)

    if args.list:
        by_ttyp = {}
        for k in sorted(chosen):
            by_ttyp.setdefault(db[k].ttyp, []).append(k)
        for ttyp, tiles in sorted(by_ttyp.items(), key=lambda kv: kv[1][0]):
            n = sum(len(cand[k]) for k in tiles)
            names = " ".join(f"R{r + 1}C{c + 1}:{len(cand[(r, c)])}" for r, c in tiles[:12])
            more = f" ... +{len(tiles) - 12}" if len(tiles) > 12 else ""
            print(f"ttyp {ttyp:4d} {len(tiles):5d} tiles {n:7d} fuses  {names}{more}")
        return 0

    extra = [tuple(int(x) for x in p.split("=")) for p in args.pll_attr.split(",") if p]
    if extra:
        original_av = gowin_pack.GW5AST_138C.get_pll_attrvals

        def get_pll_attrvals(self, bel):
            av = original_av(self, bel)
            for attr, val in extra:
                chipdb.add_attr_val(self.chipdb.db, "PLL", av, attr, val)
            return av
        gowin_pack.GW5AST_138C.get_pll_attrvals = get_pll_attrvals

    pip_bits = []
    for spec in args.pip:
        m = re.fullmatch(r"R(\d+)C(\d+):(\w+)<-(\w+)", spec)
        if not m:
            raise SystemExit(f"bad --pip {spec!r}; use R<row>C<col>:DEST<-SRC")
        row, col, dest, src = int(m.group(1)) - 1, int(m.group(2)) - 1, m.group(3), m.group(4)
        td = db[row, col]
        bits = td.pips.get(dest, {}).get(src) or td.clock_pips.get(dest, {}).get(src)
        if not bits:
            raise SystemExit(f"--pip {spec}: no such pip or clock pip in the chipdb")
        pip_bits.append(((row, col), set(bits), spec))

    raw_fuses = []
    for spec in args.fuse + args.clear_fuse:
        m = re.fullmatch(r"R(\d+)C(\d+):(\d+),(\d+)", spec)
        if not m:
            raise SystemExit(f"bad --fuse {spec!r}; use R<row>C<col>:<r>,<c>")
        raw_fuses.append(((int(m.group(1)) - 1, int(m.group(2)) - 1),
                          (int(m.group(3)), int(m.group(4))), spec,
                          0 if spec in args.clear_fuse else 1))

    only_t = {int(t) for t in args.copy_empty_ttyp.split(",") if t}
    skip_t = {int(t) for t in args.copy_empty_skip_ttyp.split(",") if t}
    original_write = gowin_pack.Bitstream_GW5A.write

    def write(self):
        if args.copy_empty or args.copy_superset:
            ntiles = nbits = 0
            for (row, col), dtile in donor_bm.items():
                ttyp = db[row, col].ttyp
                if (only_t and ttyp not in only_t) or ttyp in skip_t:
                    continue
                tile = self.main_tilemap[row, col]
                own = {(r, c) for r, line in enumerate(tile) for c, b in enumerate(line) if b}
                if own and not (args.copy_superset and
                                own <= {(r, c) for r, line in enumerate(dtile)
                                        for c, b in enumerate(line) if b}):
                    continue
                for r, line in enumerate(dtile):
                    for c, b in enumerate(line):
                        if b:
                            tile[r][c] = 1
                            nbits += 1
                ntiles += 1
            print(f"transplant: copied {nbits} donor fuses into {ntiles} "
                  f"{'empty or subset' if args.copy_superset else 'empty'} tiles", file=sys.stderr)
        for (row, col) in chosen:
            tile = self.main_tilemap[row, col]
            for r, c in cand[(row, col)]:
                tile[r][c] = 1
        print(f"transplant: OR-ed {total} unmodelled fuses into {len(chosen)} tiles", file=sys.stderr)
        for (row, col), bits, spec in pip_bits:
            tile = self.main_tilemap[row, col]
            for r, c in bits:
                tile[r][c] = 1
            print(f"transplant: set pip {spec} ({len(bits)} fuses)", file=sys.stderr)
        for (row, col), (r, c), spec, val in raw_fuses:
            self.main_tilemap[row, col][r][c] = val
            print(f"transplant: {'set' if val else 'cleared'} fuse {spec}", file=sys.stderr)
        return original_write(self)
    gowin_pack.Bitstream_GW5A.write = write

    pack_args = args.pack_args[1:] if args.pack_args[:1] == ["--"] else args.pack_args
    sys.argv = ["gowin_pack"] + pack_args
    return gowin_pack.main()


if __name__ == "__main__":
    sys.exit(main())
