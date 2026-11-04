#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare two desktop-core provenance files.

`tools/desktop_open/build.sh` writes a `provenance.txt` beside every bitstream
it produces: the tuple of inputs the build consumed, plus the digests of what
came out.  This tool says whether two of those tuples are the same build.

Why it exists: on 2026-11-03 a cycle had to hash four candidate cores off the
card and compare them with file mtimes to work out which one was current, and a
recorded bitstream could not be rebuilt from the tuple recorded for it.  The
flow itself is deterministic -- two builds from one tree and one toolchain give
identical bits -- so what was missing was not a clock or a seed but a record of
the inputs.  This is the check that makes the record worth having.

    tools/check_desktop_provenance.py <expected.txt> <actual.txt>

Exit status 0 when every comparable field matches, 1 when any differs, 2 when a
file cannot be read or is not a provenance file.

Two groups of lines are deliberately not compared:

  * `built_at` -- the only line that differs between two builds of one tuple.
  * `tinytang_head` and `tinytang_status_files` -- where this repository's HEAD
    happened to be.  Neither is an input to synthesis, place and route or
    packing: `source_sha256` covers the RTL that is.  Comparing them would make
    every unrelated commit invalidate a recorded core.

Every field that IS an input must match exactly, as must both output digests.
A field present in one file and absent from the other counts as a difference: a
build that cannot say what its chipdb was is not the same build as one that can.
"""

import sys


def load(path):
    """Return (fields, error).  fields is an ordered dict plus a list of keys
    seen more than once, which is itself a malformed file."""
    fields = {}
    duplicates = []
    try:
        with open(path, encoding="utf-8") as handle:
            for number, raw in enumerate(handle, 1):
                line = raw.rstrip("\n")
                if not line.strip() or line.lstrip().startswith("#"):
                    continue
                if "=" not in line:
                    return None, f"{path}:{number}: not a key=value line: {line!r}"
                key, _, value = line.partition("=")
                key = key.strip()
                if not key:
                    return None, f"{path}:{number}: empty key"
                if key in fields:
                    duplicates.append(key)
                fields[key] = value
    except OSError as exc:
        return None, f"{path}: {exc}"
    if not fields:
        return None, f"{path}: no fields; not a provenance file"
    if "desktop_bin_sha256" not in fields:
        return None, f"{path}: no desktop_bin_sha256; not a desktop-core provenance file"
    if duplicates:
        unique = sorted(set(duplicates))
        return None, f"{path}: repeated fields: {', '.join(unique)}"
    return fields, None


IGNORED = {
    "built_at",          # the one line two builds of a tuple differ in
    "tinytang_head",     # context, not an input; source_sha256 covers the RTL
    "tinytang_status_files",
}


def main(argv):
    if len(argv) != 3:
        print("usage: check_desktop_provenance.py <expected.txt> <actual.txt>",
              file=sys.stderr)
        return 2

    expected, error = load(argv[1])
    if error:
        print(error, file=sys.stderr)
        return 2
    actual, error = load(argv[2])
    if error:
        print(error, file=sys.stderr)
        return 2

    problems = []
    for key in sorted(set(expected) | set(actual)):
        if key in IGNORED:
            continue
        if key not in expected:
            problems.append(f"  {key}: only in actual ({actual[key]})")
        elif key not in actual:
            problems.append(f"  {key}: only in expected ({expected[key]})")
        elif expected[key] != actual[key]:
            problems.append(f"  {key}:\n    expected {expected[key]}\n    actual   {actual[key]}")

    if problems:
        print(f"provenance DIFFERS: {argv[1]} vs {argv[2]}")
        for line in problems:
            print(line)
        print(f"{len(problems)} field(s) differ")
        return 1

    compared = len([k for k in expected if k not in IGNORED])
    print(f"provenance matches on all {compared} compared fields")
    print(f"  desktop.bin {expected['desktop_bin_sha256']}")
    print(f"  desktop.fs  {expected['desktop_fs_sha256']}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
