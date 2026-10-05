#!/usr/bin/env python3
"""Check .ai/core-log.md against the rules .ai/core-syntax.md states.

    tools/check_core_log.py [.ai/core-log.md]

Exit status 0 when every entry conforms, 1 otherwise, so it can gate a commit.

Written because the ad-hoc shell version of this check reported the *latest*
entry as having zero sections while it plainly had six -- its awk consumed the
block before grep ever saw it.  A checker that fails silently toward "fine" is
worse than no checker, so the rules are worth a real one.  It checks structure,
not prose: section order, numbering, types, the terminator, the Status table and
the 100-entry cap.
"""
import pathlib
import re
import sys

SECTIONS = [
    "#### Coming From:",
    "#### Purpose:",
    "#### Outcome:",
    "#### Next Steps:",
    "#### Files Modified:",
    "#### Status:",
]
TYPES = ("COMMIT", "VERSION")
STATUS_KEYS = ("- Build: ", "- Deployment: ", "- User Test: ")
STATUS_VALUES = ("PASS", "FAIL", "NOT RUN", "N/A")
CAP = 100


def main(argv):
    path = pathlib.Path(argv[1] if len(argv) > 1 else ".ai/core-log.md")
    lines = path.read_text(encoding="utf-8").splitlines()

    starts = [i for i, line in enumerate(lines) if line.startswith("## ")]
    problems = []
    if not starts:
        print("FAIL: no entries found")
        return 1
    if len(starts) > CAP:
        problems.append("the active log holds %d entries; the limit is %d" % (len(starts), CAP))

    for idx, start in enumerate(starts):
        end = starts[idx + 1] if idx + 1 < len(starts) else len(lines)
        block = lines[start:end]
        n = idx + 1

        m = re.match(r"^## (\d+) (\S+) (\S+) (\S+)$", block[0])
        if not m:
            problems.append("entry %d: header is not '## <num> <type> <version> <timestamp>': %r"
                            % (n, block[0]))
        else:
            num, typ = int(m.group(1)), m.group(2)
            if num != n:
                problems.append("entry %d: numbered %d" % (n, num))
            if typ not in TYPES:
                problems.append("entry %d: type %r is neither COMMIT nor VERSION" % (n, typ))

        found = [line for line in block if line.startswith("#### ")]
        if found != SECTIONS:
            problems.append("entry %d: sections are %r" % (n, found))

        body = [line for line in block if line.strip()]
        if body[-1] != "---":
            problems.append("entry %d: does not end with '---' (ends %r)" % (n, body[-1]))

        # Only the bullets under Status, not the ones under Files Modified.
        status = ([line for line in block[block.index("#### Status:"):] if line.startswith("- ")]
                  if "#### Status:" in block else [])
        if len(status) != 3:
            problems.append("entry %d: Status holds %d lines, not 3" % (n, len(status)))
        else:
            for line, key in zip(status, STATUS_KEYS):
                if not line.startswith(key):
                    problems.append("entry %d: Status line %r is not %r" % (n, line, key))
                elif line[len(key):].strip() not in STATUS_VALUES:
                    problems.append("entry %d: %s%r is not one of %s"
                                    % (n, key, line[len(key):].strip(), ", ".join(STATUS_VALUES)))

    if problems:
        for p in problems:
            print("FAIL: " + p)
        return 1
    print("core-log: %d entries, all conforming" % len(starts))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
