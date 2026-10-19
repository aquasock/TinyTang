#!/usr/bin/env python3
"""Decode the clock-network pips a GW5AST-138C bitstream sets, from the vendor tables.

    clock_route_decode.py LABEL=FILE.fs [LABEL=FILE.fs ...] [--out FILE.tsv]

Read-only.  For every tile the bitstream sets fuses in, every row of the
vendor's own clock tables -- CLOCK_MUX (38), CLOCK_MUX_TOP (90) and
CLOCK_MUX_BOTTOM (91) in GOWINHOME's GW5AST-138C.fse -- is tested against the
tile's fuses, unfiltered.  apicula's chipdb is not consulted for the pips:
fse_clock_pips_138() drops whole ranges and rewrites the bridge pairs, and a
decode through it can only name what the model already admits, so a vendor
route the model mislabels would come back in the model's own words.

A row is on when every fuse it names is set.  Rows that share a destination
can share fuses (a mux is encoded, not one-hot), so for each destination only
the matching rows whose fuse set is not a strict subset of another matching
row's are reported.  Rows with no fuse (an always-on connection) are skipped.

With --selects it reports instead, for each bitstream, the spine select ports
(the CLK and A ports of the logic tiles at rows 27 and 81, columns 92 to 94
that nextpnr's enable_spines() ties to a constant) and the routing rows that
drive them, from the vendor's GENERAL table (2).  A port with no row set is
unprogrammed; Gowin drives the ports of a spine it uses from VSS (core-log
entry 94).

Output is one TSV line per pip per bitstream:
    label  row  col  ttyp  table  dest  src  srcid  destid  fuses
with 0-based row and col (X = col, Y = row in nextpnr's names).  Run it under
the open flow's interpreter and PYTHONPATH (tools/desktop_open/build.sh).

SPDX-License-Identifier: MIT
"""

import argparse
import os
import sys

from apycula import fse_parser, wirenames as wnames
from apycula.bslib import read_bitstream
from apycula.chipdb import unpad

DEVICE = "GW5AST-138C"
TABLES = {38: "CLOCK_MUX", 90: "CLOCK_MUX_TOP", 91: "CLOCK_MUX_BOTTOM"}


def clock_rows(fse):
    """{ttyp: [(table, srcid, destid, frozenset(fuses))]} from the vendor tables."""
    rows = {}
    for ttyp, td in fse.items():
        if not isinstance(ttyp, int):
            continue
        wires = td.get("wire", {})
        for table in TABLES:
            for srcid, destid, *fuses in wires.get(table, []):
                if srcid < 0:
                    continue
                fs = frozenset(fse_parser.fuse_lookup(fse, ttyp, f, DEVICE)
                               for f in unpad(fuses))
                if fs:
                    rows.setdefault(ttyp, []).append((table, srcid, destid, fs))
    return rows


def decode(fse, rows, path):
    bitmap, _, _, _ = read_bitstream(path)
    tiles = fse_parser.tile_bitmap(fse, bitmap)
    found = []
    for (row, col, ttyp), tile in tiles.items():
        if ttyp not in rows:
            continue
        on = {(r, c) for r, line in enumerate(tile) for c, bit in enumerate(line) if bit}
        matched = {}
        for table, srcid, destid, fs in rows[ttyp]:
            if fs <= on:
                matched.setdefault((table, destid), []).append((srcid, fs))
        for (table, destid), cands in matched.items():
            for srcid, fs in cands:
                if any(fs < other for _, other in cands):
                    continue
                found.append((row, col, ttyp, table, destid, srcid, fs))
    return found


SELECT_TILES = [(r, c) for r in (27, 81) for c in (92, 93, 94)]
SELECT_PORTS = ("CLK0", "CLK1", "CLK2", "A6", "A7")


def select_drivers(fse, path):
    """{(row, col): {port: [sources]}} for the spine select ports."""
    bitmap, _, _, _ = read_bitstream(path)
    tiles = fse_parser.tile_bitmap(fse, bitmap)
    grid = fse["header"]["grid"][61]
    out = {}
    for row, col in SELECT_TILES:
        ttyp = grid[row][col]
        tile = tiles.get((row, col, ttyp))
        on = set() if tile is None else {
            (r, c) for r, line in enumerate(tile) for c, bit in enumerate(line) if bit}
        cands = {}
        for srcid, destid, *fuses in fse[ttyp]["wire"].get(2, []):
            dest = wnames.wirenames.get(destid)
            if dest not in SELECT_PORTS:
                continue
            fs = frozenset(fse_parser.fuse_lookup(fse, ttyp, f, DEVICE) for f in unpad(fuses))
            if fs and fs <= on:
                cands.setdefault(dest, []).append((wnames.wirenames.get(abs(srcid), str(srcid)), fs))
        out[row, col] = {port: [src for src, fs in c if not any(fs < o for _, o in c)]
                         for port, c in cands.items()}
    return out


def name(i):
    return wnames.clknames.get(i, f"UNK{i}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bitstreams", nargs="+", help="LABEL=path.fs")
    ap.add_argument("--gowinhome", default=os.environ.get("GOWINHOME",
                                                          "/home/vash/tools/gowin-1.9.11.03"))
    ap.add_argument("--out")
    ap.add_argument("--selects", action="store_true",
                    help="report the spine select ports instead of the clock pips")
    args = ap.parse_args()

    # read_fse checks the IDE version through GOWINHOME.
    os.environ["GOWINHOME"] = args.gowinhome
    wnames.select_wires(DEVICE)
    fse_path = os.path.join(args.gowinhome, "IDE/share/device", DEVICE, DEVICE + ".fse")
    with open(fse_path, "rb") as f:
        fse = fse_parser.read_fse(f, DEVICE)
    rows = clock_rows(fse)

    out = open(args.out, "w") if args.out else sys.stdout
    if args.selects:
        print("label\trow\tcol\tport\tdrivers", file=out)
        for spec in args.bitstreams:
            label, _, path = spec.partition("=")
            for (row, col), ports in select_drivers(fse, path).items():
                for port in SELECT_PORTS:
                    drivers = ports.get(port)
                    print(label, row, col, port, ",".join(drivers) if drivers else "-",
                          sep="\t", file=out)
        if args.out:
            out.close()
        return
    print("label\trow\tcol\tttyp\ttable\tdest\tsrc\tsrcid\tdestid\tfuses", file=out)
    for spec in args.bitstreams:
        label, _, path = spec.partition("=")
        if not path:
            sys.exit(f"bad argument {spec!r}; use LABEL=path.fs")
        for row, col, ttyp, table, destid, srcid, fs in sorted(decode(fse, rows, path)):
            print(label, row, col, ttyp, TABLES[table], name(destid), name(srcid),
                  srcid, destid, " ".join(f"{r},{c}" for r, c in sorted(fs)),
                  sep="\t", file=out)
    if args.out:
        out.close()


if __name__ == "__main__":
    main()
