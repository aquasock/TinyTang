#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host checks for tools/tinytang_sync.py.

The two parts worth testing without a board are the ones that broke while the
tool was written: reading the shell's one-line `ls -l` format, and deciding
what differs between the board and the local mirror.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

from tinytang_sync import parse_ls, plan    # noqa: E402

CHECKS = 0
FAILED = 0


def check(ok, what):
    global CHECKS, FAILED
    CHECKS += 1
    if not ok:
        FAILED += 1
        print(f"  FAIL {what}", file=sys.stderr)


def main():
    # The shell lists a whole directory on one line, entries separated by two
    # spaces, with colour codes around directory names.
    real = ("d        0  \x1b[1;34mscripts\x1b[0m  d        0  cores  "
            "-   657232  tinytang-bl616.bin  -       16  gutenberg.pos")
    entries = parse_ls(real)
    check(len(entries) == 4, f"four entries from one line (got {entries})")
    check(entries[0] == ("d", 0, "scripts"), "a coloured directory name is clean")
    check(entries[2] == ("-", 657232, "tinytang-bl616.bin"), "a file and its size")
    check(entries[3] == ("-", 16, "gutenberg.pos"), "the last entry is not swallowed")

    spaced = "-     1234  Book Title With Spaces.zip  -     99  b.txt"
    entries = parse_ls(spaced)
    check(len(entries) == 2, f"two entries when a name has spaces ({entries})")
    check(entries[0][2] == "Book Title With Spaces.zip",
          "a name with spaces survives whole")

    check(parse_ls("") == [], "an empty listing yields nothing")
    check(parse_ls("d 0 nodigit") == [], "a malformed line yields nothing")

    board = {"a.txt": 10, "b.bin": 20, "same.txt": 5}
    local = {"b.bin": 25, "c.txt": 7, "same.txt": 5}
    only_board, only_local, differing = plan(board, local)
    check(only_board == ["a.txt"], f"board-only files ({only_board})")
    check(only_local == ["c.txt"], f"local-only files ({only_local})")
    check(differing == ["b.bin"], f"size-differing files ({differing})")
    check("same.txt" not in differing, "equal sizes are not a difference")
    check(plan({}, {}) == ([], [], []), "two empty sides have no differences")

    print(f"tinytang sync: {CHECKS} checks, {FAILED} failed")
    return 0 if FAILED == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
