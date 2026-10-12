#!/usr/bin/env python3
"""Append missing desktop clock constraints using primitive pins, not aliases.

The board clock after its IBUF and the CLKDIV pixel output otherwise fall back
to nextpnr's 12 MHz default. Resolve pins against the actual input netlist and
refuse missing primitives instead of silently constraining a nonexistent net.

    tools/desktop-clock-constraints.py pnr-input.json desktop.sdc

SPDX-License-Identifier: MIT
"""

import argparse
import json
from pathlib import Path


CLOCKS = (
    ("board_clock", "pll_27.PLL_inst", "PLL", "CLKIN", 50.0),
    ("video_reference", "pll_27.PLL_inst", "PLL", "CLKOUT0", 27.0),
    ("video_pixel", "div5", "CLKDIV", "CLKOUT", 74.25),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("netlist", type=Path)
    parser.add_argument("sdc", type=Path)
    args = parser.parse_args()
    doc = json.loads(args.netlist.read_text())
    module = doc["modules"].get("nestang_top") or doc["modules"].get("top")
    if module is None:
        parser.error("missing flattened desktop top module")
    # These constraints describe this desktop clock chain. Refuse a changed
    # divider recipe rather than assigning its old frequency to a new clock.
    expected = {
        "pll_27.PLL_inst": {"IDIV_SEL": 1, "FBDIV_SEL": 1, "MDIV_SEL": 27, "ODIV0_SEL": 50,
                            "MDIV_FRAC_SEL": 0, "ODIV0_FRAC_SEL": 0},
        "pll_hdmi.PLL_inst": {"IDIV_SEL": 1, "FBDIV_SEL": 1, "MDIV_SEL": 55, "ODIV0_SEL": 4,
                              "MDIV_FRAC_SEL": 0, "ODIV0_FRAC_SEL": 0},
        "div5": {"DIV_MODE": 5},
    }
    for name, params in expected.items():
        for key, value in params.items():
            raw = module["cells"].get(name, {}).get("parameters", {}).get(key)
            try:
                actual = int(raw, 2) if isinstance(raw, str) and len(raw) > 8 else int(raw)
            except (TypeError, ValueError):
                parser.error(f"unrecognized clock parameter {name}.{key}: {raw}")
            if actual != value:
                parser.error(f"changed clock recipe {name}.{key}: {actual}, expected {value}")
    for name, fref in (("pll_27.PLL_inst", 50.0), ("pll_hdmi.PLL_inst", 27.0)):
        if module["cells"][name]["parameters"].get("FCLKIN") != str(int(fref)):
            parser.error(f"changed or unrecognized reference clock in {name}")
    additions = ["\n# Additional desktop clocks resolved at their primitive pins.\n"]
    for clock, name, kind, pin, mhz in CLOCKS:
        cell = module["cells"].get(name, {})
        if cell.get("type") != kind or len(cell.get("connections", {}).get(pin, [])) != 1:
            parser.error(f"missing {kind} clock pin {name}/{pin}")
        additions.append(f"create_clock -name {clock} -period {1000 / mhz:.9f} "
                         f"[get_pins {{{name}/{pin}}}]\n")
    args.sdc.write_text(args.sdc.read_text() + "".join(additions))


if __name__ == "__main__":
    main()
