#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Where the BL616's RAM goes, from a build's link map.

Static RAM -- .data, .bss and .noinit -- is what the heap does not get: the
heap is whatever is left of OCRAM after them (and a 32 KiB WRAM heap beside
it, BL6-010).  This reads the input sections out of the GNU ld map the SDK
writes next to the image and reports them by object file and by symbol, so a
change in RAM is measured, not guessed.

    tools/ram_report.py build                      # one build
    tools/ram_report.py build build-btclassic      # the second against the first
    tools/ram_report.py build --top 40 --min 1024

A build is a build directory (its build_out/*.map) or a .map file.
"""

import argparse
import collections
import glob
import os
import re
import sys

RAM_SECTIONS = (".bss", ".sbss", ".data", ".sdata", ".noinit", ".ram_noinit", "COMMON")
# Output sections that are RAM in the BL616 linker script.
RAM_OUTPUTS = ("ram_data", "ram_bss", "ram_noinit", "tcm_data", "ram_nocache_data",
               "ram_nocache_noinit")


def map_path(arg):
    if arg.endswith(".map"):
        return arg
    found = glob.glob(os.path.join(arg, "build_out", "*.map"))
    if len(found) != 1:
        sys.exit(f"{arg}: expected one build_out/*.map, found {found or 'none'}")
    return found[0]


def read_map(path):
    """[(output section, input section name, size, object)] for RAM sections."""
    out = []
    current = None
    lines = open(path, errors="replace").read().splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(r"^(\S+)\s+0x[0-9a-f]+\s+0x[0-9a-f]+", line)
        if m:
            current = m.group(1)
        elif re.match(r"^\S", line) and not line.startswith(" "):
            word = line.split()[0] if line.split() else ""
            if word in RAM_OUTPUTS or word.startswith("."):
                current = word
        s = line.strip()
        if current in RAM_OUTPUTS and s.startswith(RAM_SECTIONS):
            parts = s.split()
            name = parts[0]
            rest = parts[1:]
            if len(rest) < 3 and i + 1 < len(lines):
                rest = lines[i + 1].split()
                i += 1
            if len(rest) >= 3 and rest[0].startswith("0x") and rest[1].startswith("0x"):
                size = int(rest[1], 16)
                if size:
                    obj = rest[2].split("/")[-1]
                    out.append((current, name, size, obj))
        i += 1
    return out


def symbol(name):
    for prefix in (".bss.", ".sbss.", ".data.", ".sdata.", ".noinit.", ".ram_noinit."):
        if name.startswith(prefix):
            return name[len(prefix):]
    return name


def summarise(entries):
    by_obj = collections.Counter()
    by_sym = collections.Counter()
    by_out = collections.Counter()
    for out, name, size, obj in entries:
        by_obj[obj] += size
        by_sym[(symbol(name), obj)] += size
        by_out[out] += size
    return by_out, by_obj, by_sym


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("build", help="build directory or .map")
    ap.add_argument("other", nargs="?", help="a second build, reported against the first")
    ap.add_argument("--top", type=int, default=25, help="rows per table (default 25)")
    ap.add_argument("--min", type=int, default=512, help="smallest symbol listed (default 512)")
    args = ap.parse_args()

    a_out, a_obj, a_sym = summarise(read_map(map_path(args.build)))
    if not args.other:
        total = sum(a_out.values())
        print(f"static RAM {total} bytes: " +
              ", ".join(f"{k} {v}" for k, v in sorted(a_out.items())))
        print(f"\nby object file (top {args.top})")
        for obj, size in a_obj.most_common(args.top):
            print(f"  {size:7d}  {obj}")
        print(f"\nby symbol, {args.min} bytes or more (top {args.top})")
        rows = [(s, k) for k, s in a_sym.items() if s >= args.min]
        for size, (sym, obj) in sorted(rows, reverse=True)[:args.top]:
            print(f"  {size:7d}  {sym:40s} {obj}")
        return

    b_out, b_obj, b_sym = summarise(read_map(map_path(args.other)))
    ta, tb = sum(a_out.values()), sum(b_out.values())
    print(f"static RAM {ta} -> {tb} bytes ({tb - ta:+d})")
    diff = collections.Counter()
    for k in set(a_obj) | set(b_obj):
        d = b_obj.get(k, 0) - a_obj.get(k, 0)
        if d:
            diff[k] = d
    print(f"\nby object file, largest changes (top {args.top})")
    for obj, d in sorted(diff.items(), key=lambda kv: -abs(kv[1]))[:args.top]:
        print(f"  {d:+8d}  {obj}  ({a_obj.get(obj, 0)} -> {b_obj.get(obj, 0)})")
    sdiff = collections.Counter()
    for k in set(a_sym) | set(b_sym):
        d = b_sym.get(k, 0) - a_sym.get(k, 0)
        if abs(d) >= args.min:
            sdiff[k] = d
    print(f"\nby symbol, changes of {args.min} bytes or more")
    for (sym, obj), d in sorted(sdiff.items(), key=lambda kv: -abs(kv[1]))[:args.top]:
        print(f"  {d:+8d}  {sym:40s} {obj}")


if __name__ == "__main__":
    main()
