#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Which port modules a host test actually compiles.

The qualification matrix used to answer this with

    grep -ql "$basename" tools/tests/*.{sh,c,cpp}

which counts a *mention*.  Two comments that merely name
`tang_fpga_uart.c` made that module look covered for several cycles while
nothing compiled it, so the number the matrix quoted was wrong.  A module is
covered here only when a test really builds it, which is one of:

  * a test shell script names the path `ports/bl616/<name>` -- that is how a
    module reaches a compiler command line;
  * a test source has an `#include` naming the module -- the include-the-
    module pattern the newer suites use so they can reach its statics.

Comments are excluded, because a comment is not a test.

    tools/check_test_coverage.py                 # list the uncovered modules
    tools/check_test_coverage.py --covered       # list the covered ones too
    tools/check_test_coverage.py --json          # machine-readable

Exit status is 0 always: this reports, it does not gate.  Making it a gate
would be wrong while most of the board's modules are legitimately untested.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


def module_dirs(root: Path):
    yield root / "ports" / "bl616"
    yield root / "ports" / "bl616" / "phosphor"


def port_modules(root: Path):
    """Every C/C++ module in the port, by path."""
    out = []
    for d in module_dirs(root):
        if not d.is_dir():
            continue
        for p in sorted(d.glob("*.c")) + sorted(d.glob("*.cpp")):
            out.append(p)
    return out


def _strip_c_comments(text: str) -> str:
    """Remove // and /* */ comments so a mention inside one cannot count."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def _strip_shell_comments(text: str) -> str:
    """Remove shell comments.  Good enough for these scripts: a '#' that is not
    at a word start or inside a string is rare in them, and over-stripping can
    only under-count coverage, which errs toward the honest direction."""
    out = []
    for line in text.splitlines():
        stripped = line.lstrip()
        if stripped.startswith("#"):
            continue
        out.append(line)
    return "\n".join(out)


def test_sources(root: Path):
    tdir = root / "tools" / "tests"
    for pattern in ("*.c", "*.cpp"):
        for p in sorted(tdir.glob(pattern)):
            yield p, _strip_c_comments(p.read_text(errors="replace"))
    sim = tdir / "sim"
    if sim.is_dir():
        for pattern in ("*.c", "*.cpp", "*.sv"):
            for p in sorted(sim.glob(pattern)):
                yield p, _strip_c_comments(p.read_text(errors="replace"))


def test_scripts(root: Path):
    tdir = root / "tools" / "tests"
    for p in sorted(tdir.glob("*.sh")):
        yield p, _strip_shell_comments(p.read_text(errors="replace"))


def coverage(root: Path):
    """Map module path -> list of tests that build it."""
    sources = list(test_sources(root))
    scripts = list(test_scripts(root))

    result = {}
    for mod in port_modules(root):
        rel = mod.relative_to(root).as_posix()
        name = mod.name
        hits = []

        for path, text in scripts:
            if rel in text:
                hits.append(path.relative_to(root).as_posix())

        inc = re.compile(r'^\s*#\s*include\s*["<][^">]*' + re.escape(name) + r'["<]',
                         re.M)
        for path, text in sources:
            if inc.search(text):
                hits.append(path.relative_to(root).as_posix())

        result[rel] = sorted(set(hits))
    return result


def main(argv):
    root = Path(__file__).resolve().parent.parent
    cov = coverage(root)

    show_covered = "--covered" in argv
    as_json = "--json" in argv

    uncovered = {k: v for k, v in cov.items() if not v}
    covered = {k: v for k, v in cov.items() if v}

    if as_json:
        print(json.dumps({"covered": covered, "uncovered": sorted(uncovered)},
                         indent=2, sort_keys=True))
        return 0

    if show_covered:
        for k in sorted(covered):
            print(f"COVERED   {Path(k).name:32s} {', '.join(Path(x).name for x in covered[k])}")
        print()

    for k in sorted(uncovered):
        print(f"untested  {Path(k).name}")

    print()
    print(f"{len(covered)} of {len(cov)} port modules are built by a host test")
    print(f"{len(uncovered)} are not")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
