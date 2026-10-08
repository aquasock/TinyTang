#!/usr/bin/env python3
"""Reject a Gowin timing report with setup/hold violations or missing summaries."""
# SPDX-License-Identifier: MIT
import argparse
from html.parser import HTMLParser
from pathlib import Path


class Tables(HTMLParser):
    def __init__(self):
        super().__init__()
        self.tables = []
        self.table = None
        self.row = None
        self.cell = None

    def handle_starttag(self, tag, attrs):
        if tag == "table":
            self.table = []
        elif tag == "tr":
            self.row = []
        elif tag in ("td", "th"):
            self.cell = []

    def handle_data(self, data):
        if self.cell is not None:
            self.cell.append(data)

    def handle_endtag(self, tag):
        if tag in ("td", "th") and self.cell is not None:
            if self.row is not None:
                self.row.append(" ".join("".join(self.cell).split()))
            self.cell = None
        elif tag == "tr" and self.row is not None:
            if self.table is not None:
                self.table.append(self.row)
            self.row = None
        elif tag == "table" and self.table is not None:
            self.tables.append(self.table)
            self.table = None


def check(path):
    parser = Tables()
    parser.feed(path.read_text())
    summaries = [t for t in parser.tables if t and t[0] == [
        "Clock Name", "Analysis Type", "Endpoints TNS", "Number of Endpoints"]]
    if len(summaries) != 1 or len(summaries[0]) < 5:
        raise ValueError("missing or incomplete total-negative-slack summary")
    for row in summaries[0][1:]:
        if len(row) != 4 or row[1] not in ("Setup", "Hold"):
            raise ValueError(f"unrecognized timing row: {row}")
        if float(row[2]) != 0 or int(row[3]) != 0:
            raise ValueError(f"timing violation: {row}")
    paths = [t for t in parser.tables if t and t[0][:2] == ["Path Number", "Path Slack"]]
    if len(paths) < 2:
        raise ValueError("setup/hold path-slack tables missing")
    margins = []
    for table in paths[:2]:
        slacks = [float(row[1]) for row in table[1:] if len(row) >= 2 and row[0].isdigit()]
        if not slacks or min(slacks) < 0:
            raise ValueError("negative or missing path slack")
        margins.append(min(slacks))
    print(f"timing PASS: setup {margins[0]:+.3f} ns, hold {margins[1]:+.3f} ns; "
          f"{len(summaries[0])-1} clock/analysis rows have zero violations")
    return {"setup_ns": margins[0], "hold_ns": margins[1], "violations": 0}


if __name__ == "__main__":
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("report", type=Path, help="Gowin *_tr_content.html")
    try:
        check(args.parse_args().report)
    except (ValueError, OSError) as exc:
        raise SystemExit(f"timing FAIL: {exc}")
