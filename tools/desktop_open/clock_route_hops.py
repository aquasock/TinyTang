#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Refuse a clock route that uses a hop suspected of not existing in silicon.

    tools/desktop_open/clock_route_hops.py <pnr.json>

The topology check before this one reads nextpnr's log and gives each clock a
coarse verdict -- dedicated, fabric, global resources only -- and timing
analysis trusts the device database's model of every connection.  Neither can
see a connection the database models but the chip lacks, and the GW5AST-138C
database is known to mis-model the centre spines and quadrant bridges
(.ai/core-reference.md TOOL-017).  A route over such a hop passes every check
and loads with that clock dead.

This reads the route nextpnr wrote into pnr.json (each net's ROUTING attribute,
wire;pip;strength triples, a pip named tile/destination/source) and reports
every pip on a watched net that touches a suspect wire.

The one suspect so far is SPINE17 on `clk`.  Desktop ABI 1.2's seed 6 met
setup and hold and loaded with HDMI synced and black and the UART silent; its
`clk` route differed from the two working cores' only in SPINE17 <-
CBRIDGEOUT_TOP2 at X94Y81 and six GT00 <- SPINE17 taps (core-log entry 67).
Seed 16, chosen for that same hop, was tested on the board on 2026-10-14 and
failed the same way -- HDMI synced at 720p and black with the UART silent --
while seed 26 answered (core-log entry 70).  Two dead cores, both of them
through SPINE17, against two working ones that avoid it, so a route through
SPINE17 is refused rather than reported.

Exit status is 0 when no suspect hop is found, 1 when one is (the build gate),
and 2 when the file could not be read.
"""
import json
import sys

# net -> wires whose use on that net matches a core that loaded dead.
SUSPECT = {
    'clk': ('SPINE17',),
}


def main(argv):
    if len(argv) != 2:
        print(__doc__.strip().splitlines()[2], file=sys.stderr)
        return 2
    try:
        design = json.load(open(argv[1]))
    except (OSError, ValueError) as e:
        print(f'{argv[1]}: {e}', file=sys.stderr)
        return 2
    module = next(iter(design['modules'].values()))
    found = 0
    for net, wires in SUSPECT.items():
        entry = module['netnames'].get(net)
        if entry is None:
            print(f'  {net}: not in this design')
            continue
        fields = entry.get('attributes', {}).get('ROUTING', '').split(';')
        pips = [fields[i + 1] for i in range(0, len(fields) - 2, 3) if fields[i + 1]]
        hits = [p for p in pips if any(w in p.split('/')[1:] for w in wires)]
        if not hits:
            print(f'  {net}: {len(pips)} pips, none through {", ".join(wires)}')
            continue
        found += len(hits)
        print(f'  WARNING {net}: {len(hits)} of {len(pips)} pips go through '
              f'{", ".join(wires)}, the hop of the timing-clean cores that loaded dead:')
        for p in hits:
            print(f'    {p}')
    if found:
        print('  This placement routes a watched clock through a suspect hop.  Cores of this')
        print('  shape met timing and loaded with that clock dead: desktop ABI 1.2 seeds 6')
        print('  and 16, whose clk took SPINE17 (core-log entries 67 and 70, TOOL-017).')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
