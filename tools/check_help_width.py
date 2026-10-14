#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Fail when a registered command's help text cannot fit on one `help` line.

The shell renders the command list with

    printf("  " C_CYAN "%-10s" C_RESET " %s\\n", commands[i].name, commands[i].help);

so one line costs two columns of indent, a ten-column name field and one space
before the help text, and the rest is the help text's:

    columns(help) <= TERMINAL_COLS - 2 - 10 - 1

At the 80 columns the USB console and the desktop terminal use, that leaves 67.
A longer help text wraps, and the wrapped line reads as though it belonged to
the command under it -- which is why this is a gate and not a style note.

    tools/check_help_width.py              # every tree that registers commands
    tools/check_help_width.py --cols 100   # a wider terminal
    tools/check_help_width.py -v           # list every entry, not just failures

Exit status 0 when every entry fits, 1 when one does not, so it can gate a
commit.  Read the tables rather than the compiled image: a command registered
with a long string is the defect, wherever the string ends up.
"""

import argparse
import pathlib
import re
import sys

# Where command tables live: this port and the two vendored projects whose
# portable cores this firmware compiles against.
DEFAULT_TREES = [
    "ports",
    "third_party/tinydesk",
    "third_party/tinydesk-shell",
]

# { "name", "usage", "help", fn, flags },
ENTRY = re.compile(
    r'\{\s*"([^"]{1,40})"\s*,\s*'
    r'"((?:[^"\\]|\\.){0,200})"\s*,\s*'
    r'"((?:[^"\\]|\\.){0,300})"\s*,',
    re.S,
)

INDENT = 2
NAME_FIELD = 10
SEPARATOR = 1


def entries(root):
    """Yield (name, help, path, line) for every command table entry found."""
    for path in sorted(pathlib.Path(root).rglob("*.c")):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for match in ENTRY.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            yield match.group(1), match.group(3), path, line


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cols", type=int, default=80,
                    help="terminal width the help output must fit (default 80)")
    ap.add_argument("--trees", nargs="*", default=DEFAULT_TREES,
                    help="roots to scan, relative to the repository root")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="list every entry, not only those that overflow")
    args = ap.parse_args(argv[1:])

    root = pathlib.Path(__file__).resolve().parent.parent
    budget = args.cols - INDENT - NAME_FIELD - SEPARATOR
    if budget < 1:
        sys.exit("check_help_width: %d columns leaves no room for help text" % args.cols)

    seen = 0
    over = []
    for tree in args.trees:
        base = root / tree
        if not base.is_dir():
            continue
        for name, help_text, path, line in entries(base):
            seen += 1
            width = INDENT + max(NAME_FIELD, len(name)) + SEPARATOR + len(help_text)
            where = "%s:%d" % (path.relative_to(root), line)
            if width > args.cols:
                over.append((width, name, help_text, where))
            elif args.verbose:
                print("  %3d  %-11s %s" % (width, name, help_text))

    if over:
        print("help text that will wrap at %d columns (budget %d):\n"
              % (args.cols, budget))
        for width, name, help_text, where in sorted(over, reverse=True):
            print("  %-11s %3d cols (over by %d): %s" % (name, width, width - args.cols,
                                                         help_text))
            print("              %s" % where)
        print("\n%d of %d entries overflow." % (len(over), seen))
        return 1

    print("check_help_width: %d entries, none over %d columns" % (seen, args.cols))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
