#!/usr/bin/env python3
"""Report resource use and timing from desktop placement variants 0–3."""
# SPDX-License-Identifier: MIT
import argparse
import contextlib
import hashlib
import io
import json
import re
from pathlib import Path

from check_gowin_timing import Tables, check


def variant(directory, placement):
    report = directory / "desktop_tr_content.html"
    with contextlib.redirect_stdout(io.StringIO()):
        timing = check(report)
    tables = Tables()
    tables.feed(report.read_text())
    clocks = next(t for t in tables.tables if t and t[0][:4] == [
        "NO.", "Clock Name", "Constraint", "Actual Fmax"])
    pixel = next(row for row in clocks[1:] if row[2] == "74.250(MHz)")
    timing["pixel_fmax_mhz"] = float(pixel[3].split("(")[0])
    text = (directory / "desktop.rpt.txt").read_text()

    def resource(name):
        match = re.search(r"^\s*" + re.escape(name) + r"\s*\|\s*(\d+(?:\.\d+)?)/(\d+)", text, re.M)
        if not match:
            raise ValueError(f"missing resource {name}")
        return {"used": float(match[1]) if "." in match[1] else int(match[1]), "total": int(match[2])}

    logic = re.search(r"\((\d+) LUT, (\d+) ALU, (\d+) ROM16\)", text)
    if not logic:
        raise ValueError("missing LUT/ALU breakdown")
    binary = directory / "desktop.bin"
    return {"placement": placement, "timing": timing,
            "lut": int(logic[1]), "alu": int(logic[2]),
            "logic": resource("Logic"), "ff": resource("--Logic Register as FF"),
            "bsram": resource("BSRAM"), "dsp": resource("DSP"),
            "pll": resource("PLL"), "bytes": binary.stat().st_size,
            "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
            "source": (directory / "source.sha256").read_text().split()[0]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    results = []
    failures = []
    for placement in range(4):
        try:
            results.append(variant(args.directory / f"place{placement}", placement))
        except (OSError, ValueError, StopIteration) as error:
            failures.append(f"placement {placement}: {error}")
    if failures:
        raise SystemExit("sweep FAIL: " + "; ".join(failures))
    if len({r["source"] for r in results}) != 1:
        raise SystemExit("sweep FAIL: variants were built from different sources")
    # Maximize worst setup slack among variants that already passed hold/TNS.
    selected = max(results, key=lambda r: r["timing"]["setup_ns"])
    summary = {"variants": results, "selected": selected["placement"]}
    (args.directory / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    lines = ["| Placement | LUT | ALU | FF | BSRAM | DSP | Pixel Fmax MHz | Setup ns | Hold ns | Violations |",
             "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for r in results:
        t = r["timing"]
        lines.append(f"| {r['placement']} | {r['lut']} | {r['alu']} | {r['ff']['used']} | "
                     f"{r['bsram']['used']} | {r['dsp']['used']} | {t['pixel_fmax_mhz']:.3f} | "
                     f"{t['setup_ns']:+.3f} | {t['hold_ns']:+.3f} | {t['violations']} |")
    lines += ["", f"Selected placement {selected['placement']} for greatest setup margin.",
              f"SHA-256: {selected['sha256']}"]
    markdown = "\n".join(lines) + "\n"
    (args.directory / "summary.md").write_text(markdown)
    print(markdown, end="")


if __name__ == "__main__":
    main()
