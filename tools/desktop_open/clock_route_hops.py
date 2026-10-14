#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Warn when a clock's route uses a hop suspected of not existing in silicon.

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
That is one dead core against two working ones, a lead and not a finding, so
this check warns and never stops the build.  A route it flags should be taken
to the board knowing it is the shape of the one dead core on record.

Exit status is 0 whether or not a suspect hop is found; 2 means the file
could not be read.
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
              f'{", ".join(wires)}, the hop of the one dead core on record:')
        for p in hits:
            print(f'    {p}')
    if found:
        print('  This placement is the shape of desktop ABI 1.2 seed 6, which met timing and')
        print('  loaded with clk dead (core-log entry 67, TOOL-017).  It is a lead, not a rule:')
        print('  packing anyway; check that the core answers `fpga` before trusting it.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
