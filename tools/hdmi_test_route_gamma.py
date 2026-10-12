#!/usr/bin/env python3
"""Re-route the HDMI test core's bit clock the way Gowin's build does, by hand.

    tools/hdmi_test_route_gamma.py in-pnr.json out-pnr.json [--keep-block1]

Diagnostic for core-log entry 36.  In the open flow's routed netlist the bit
clock `hclk5` leaves pll_hdmi's MPLLCLKOUT0 on general fabric and enters the
HCLK blocks through their fabric entries (`L2HCLK30` at X181Y81, and a second
branch to `L2HCLK12` at X181Y27 for the serialiser at X181Y57).  Gowin's build
of the same design, with pll_hdmi at PLL_R[2], uses only the X181Y81 block and
selects `HCLK_MUX_GAMMA30 <- HCLK_UNK581`, which no modelled pip drives.

This rewrites `hclk5`'s ROUTING to that shape and nothing else:

  * every fabric pip of `hclk5` is dropped (the PLL output wire stays as the
    net's source);
  * the X181Y81 `L2HCLK30 -> BETA30 -> BUF_BI30` entry is dropped and
    `HCLK_MUX_GAMMA30 <- HCLK_UNK581` is added, keeping
    `ALPHA30 <- BUF_BO30`, `HCLK30 <- ALPHA30`, `CLKDIV_I30 <- ALPHA30` and
    each serialiser's own `FCLKA` lane select;
  * unless --keep-block1, the X181Y27 block's entry and muxes are dropped too,
    as Gowin leaves that block unconfigured.

The result is packed with the apicula fork's gowin_pack as usual.  It is an
experiment, not a build: the route is asserted, not found by the router.

SPDX-License-Identifier: MIT
"""

import argparse
import json
import sys


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--keep-block1", action="store_true")
    args = ap.parse_args()

    doc = json.load(open(args.src))
    top = doc["modules"]["top"]
    net = top["netnames"]["hclk5"]
    parts = net["attributes"]["ROUTING"].split(";")
    triples = [parts[i:i + 3] for i in range(0, len(parts) - 2, 3)]

    keep, dropped = [], []
    for wire, pip, strength in triples:
        dst_wire = wire.split("/", 1)[1]
        if not pip:                                   # the net's source wire
            keep.append((wire, pip, strength))
            continue
        tile = pip.split("/", 1)[0]
        hclk = "HCLK" in dst_wire or dst_wire in ("FCLKA",) or dst_wire.startswith("CLKDIV_I")
        if not hclk:
            dropped.append(pip)                       # fabric segment
        elif "BETA" in pip or "BUF_BI" in dst_wire:
            dropped.append(pip)                       # fabric entry into the block
        elif tile == "X181Y27" and not args.keep_block1 and "FCLKA" not in dst_wire:
            dropped.append(pip)                       # second block, unused by Gowin
        elif (not args.keep_block1 and dst_wire in ("HCLK_BUF_BO12", "HCLK_MUX_ALPHA12", "HCLK12")):
            dropped.append(pip)
        else:
            keep.append((wire, pip, strength))
    keep.append(("X181Y81/HCLK_MUX_GAMMA30", "X181Y81/HCLK_MUX_GAMMA30/HCLK_UNK581", "1"))

    for pip in dropped:
        print(f"drop {pip}")
    for wire, pip, _ in keep:
        print(f"keep {wire} {pip}")
    net["attributes"]["ROUTING"] = "".join(f"{w};{p};{s};" for w, p, s in keep)
    json.dump(doc, open(args.dst, "w"))


if __name__ == "__main__":
    sys.exit(main())
