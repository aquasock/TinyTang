# GW5AST-138C clock-tree inventory — 2026-10-18

Read-only inventory of the vendor clock data against the apicula clock model,
the first step of the clock-tree cycle (core-log entry 88).

## How to reproduce

```sh
GOWINHOME=/home/vash/tools/gowin-1.9.11.03 \
PYTHONPATH=/run/media/vash/GIT/apicula-mathieufro:/home/vash/tools/pydeps \
python3 tools/desktop_open/clock_inventory.py \
    --chipdb /run/media/vash/GIT/apicula-mathieufro/apycula/GW5AST-138C.msgpack.xz
```

Vendor side: Gowin IDE 1.9.11.03, `GW5AST-138C.fse` / `.dat` / `.tm` /
`.csr` / `.ini`.  Model side: apicula fork `epic/gw5ast138c`, chipdb
`GW5AST-138C.msgpack.xz`, `fse_clock_pips_138()` in `apycula/chipdb.py`.

## Vendor data that exists

`.dat` `gw5aStuff` clock keys:

- `PllIn` (216), `PllOut` (32), `PllInDlt`, `PllOutDlt`, the four corner PLL
  input/output tables;
- `CMuxTopInNodes` (189 rows), `CMuxBotInNodes` (189 rows) — the central clock
  mux input node lists;
- `CMuxTopIns` (189 rows × 3 inputs), `CMuxBotIns` (189 rows × 3) — the mux
  input selections (many `-1`, i.e. unused);
- `SpineColumn` (8 values, all zero — read as a lead: either the field is
  unused on this device or the parser offset is wrong).

`.fse` wire tables: `CLOCK_MUX` (38), `CLOCK_MUX_TOP` (90),
`CLOCK_MUX_BOTTOM` (91) for the central bridge tile types 80–85, plus the
`HCLK` table (48).  This is the raw clock connectivity the router needs.

`.tm` (3.1 MB) is the vendor timing model; the clock classes have not been
read yet.

## What the fork's model does with it

`fse_clock_pips_138()` filters the `.fse` rows:

- skips source ids in `P16A..P47D`, `164..236` (longwires), `105..128`
  (unknown wires), `253..268`;
- allows a `SPINE*` source only to `GT00`/`GT10`;
- for a `SPINE*` destination on a bridge tile, keeps the logic-to-clock gates
  (renamed `_TOP`/`_BOT`) or a `CBRIDGEOUT*` source only through the hardcoded
  permutation `{top: "02136574", bottom: "30214657"}`;
- drops the measured-dead bridge pips (`SPINE17 <= CBRIDGEOUT_BOTTOM0`,
  `SPINE18 <= CBRIDGEOUT_BOTTOM2`, `SPINE25 <= CBRIDGEOUT_BOTTOM0`);
- in `CLOCK_MUX` (38), passes only the measured HCLK escape wires; other
  longwires/unknowns are skipped.

The generated chipdb reports 484 clock pips, 171 HCLK pips, 2
`spine_select_wires`, 6 `io2hclk` and 6 `hclk_div2` entries.

## Raw versus kept

| tile | table | raw rows | kept pips (dests) |
|---|---|---|---|
| 80 | CLOCK_MUX 38 | 1672 | 807 (18) |
| 80 | CLOCK_MUX_BOTTOM 91 | 119 | — |
| 81 | CLOCK_MUX 38 | 479 | 275 (5) |
| 81 | CLOCK_MUX_BOTTOM 91 | 35 | — |
| 82 | CLOCK_MUX 38 | 40 | 40 (17) |
| 83 | — | 0 | 0 |
| 84 | CLOCK_MUX 38 | 388 | 264 (6) |
| 84 | CLOCK_MUX_TOP 90 | 40 | — |
| 84 | CLOCK_MUX_BOTTOM 91 | 5 | — |
| 85 | CLOCK_MUX 38 | 1259 | 775 (18) |
| 85 | CLOCK_MUX_TOP 90 | 136 | — |
| 85 | CLOCK_MUX_BOTTOM 91 | 18 | — |

The scanner's breakdown for tile 80's 1672 rows is 992 longwire (164–236),
136 unknown (105–128) and 544 "other"; the "other" bucket contains the
`TLPLL0CLK0` (81) rows, the logic gates that are renamed per half, and the
non-gate sources the model drops.  The next version of the scanner must
classify exact `(src, dst)` membership in the kept set, including the
`_TOP`/`_BOT` rename, so the delta is a list rather than a bucket.

## What this points at

The vendor has the full central-mux node and selection tables.  The fork
replaces part of that with a hardcoded spine permutation, drops whole
longwire/unknown ranges, and keeps only the clock arcs it has measured.  That
is consistent with the symptom: the router has fewer dedicated clock arcs
than the silicon, so it falls back to fabric for the pixel clock (28–201
sinks), and the `clk` route can take a centre spine the model mislabels
(SPINE17).  The hardcoded permutation and the skipped ranges are the two
places to prove or replace against the vendor data.

## Result — permutation experiment (2026-10-18)

The fork's hardcoded `is_allowed_spine_input()` was removed on the non-bridge
clock cells, the measured-dead list was kept, the chipdb was regenerated
(850,644 -> 851,232 bytes) and the nextpnr `.bba` rebuilt (93,897,666 bytes).
On the TinyTang desktop netlist, **all 8 placement seeds pass timing with 0
pixel-clock fabric fallbacks** (was 28-201), at 82.7-90.1 MHz.  The vendor rows
were the missing arcs.

The pixel clock now routes on **global resources only**, which the topology
gate flags against the profile's 21 fabric sinks.  That is a new topology, not
a proven bad one; it needs a board test before the profile is re-recorded.
`clk` still takes SPINE17 (7-15 pips), so step 2d still refuses: the
control-clock route is the remaining blocker.  The bridge-tile golden counts
are unchanged (the change is on tile types 196-251); the measured dead/good pip
tests and the inventory test all pass (21 tests).

Fork commit: `65cf206` on `aquasock/apicula` `epic/gw5ast138c`.  The generated
chipdb and `.bba` are build products; regenerate with the Makefile target.

## Next change

1. `clk`: decode the vendor `PllIn` / `PllOut` / `CMux` rows for the `pll_nes`
   (`TRPLL0CLK0`) route and correct the global-route model; target no SPINE17.
2. Board-test the new pixel-clock topology (global resources only) with an
   experimental core carrying both overrides; if HDMI and the OLED pass, the
   profile can be re-recorded and the topology gate updated.
3. The I2C/RTC bring-up resumes after a clock-clean normal core passes the
   board test.
