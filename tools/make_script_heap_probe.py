#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate a Castlevania launcher with heap readouts inside its script worker.

Run from the repository root:
    python3 tools/make_script_heap_probe.py /tmp/castlevania-heap.tdsh
    python3 tools/tinytang_put.py /tmp/castlevania-heap.tdsh /scripts/castlevania-heap.tdsh

With music playing and the Phosphor window open, run the generated launcher
from TinyDesk's Terminal. Its ble readouts include the live session copy,
worker stack and interpreter. Existing command-result checks are preserved.
"""

import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    script = (root / "scripts/castlevania.tdsh").read_text()
    stages = {
        'tangload "$CORE"': "before core load",
        'nesload "$ROM"': "before ROM load",
        'echo "castlevania: up"': "after ROM load",
    }
    found = set()
    lines = []
    for line in script.splitlines():
        if line in stages:
            found.add(line)
            lines.extend([f'echo "heap probe: {stages[line]}"', "ble"])
        lines.append(line)
    if found != set(stages):
        raise SystemExit("Castlevania launcher changed; review probe insertion points")
    args.output.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
