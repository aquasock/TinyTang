#!/usr/bin/env python3
"""Run the apicula fork's gowin_pack with extra raw PLL attributes.

    tools/gowin_pack_pll_attr.py ATTR=VAL[,ATTR=VAL...] <gowin_pack arguments>

Diagnostic for core-log entry 36.  Decoding the Gowin build of the HDMI test
core shows every PLL carrying attribute 211 = 2, an id apycula/attrids.py does
not name (it sits between A_CLKFB_SEL 209 and A_GMC_SEL 212) and that the
fork's packer therefore never writes.  This wrapper adds the given
(attribute id, value id) pairs to every GW5AST-138C PLL's attribute set before
its fuses are looked up, so the vendor's own fuse table for that pair decides
which bits are set.  Nothing else about the pack changes.

SPDX-License-Identifier: MIT
"""

import sys

from apycula import chipdb, gowin_pack


def main():
    if len(sys.argv) < 2 or "=" not in sys.argv[1]:
        print(__doc__, file=sys.stderr)
        return 2
    extra = [tuple(int(x) for x in pair.split("=")) for pair in sys.argv[1].split(",")]
    del sys.argv[1]

    original = gowin_pack.GW5AST_138C.get_pll_attrvals

    def get_pll_attrvals(self, bel):
        av = original(self, bel)
        db = self.chipdb.db
        for attr, val in extra:
            if (attr, val) not in db.logicinfo["PLL"]:
                raise SystemExit(f"PLL attribute {attr}={val} is not in the chipdb's logicinfo")
            chipdb.add_attr_val(db, "PLL", av, attr, val)
        print(f"gowin_pack_pll_attr: {bel.cell.name}: added {extra}", file=sys.stderr)
        return av

    gowin_pack.GW5AST_138C.get_pll_attrvals = get_pll_attrvals
    return gowin_pack.main()


if __name__ == "__main__":
    sys.exit(main())
