# Carried patches

Changes this project makes to third-party sources it does not own live here as
git-format patch files, not as commits in the third-party checkout itself. The
checkout stays pristine and in sync with upstream, and every local change is
one reviewable file with a recorded reference commit.

A patch is applied by `scripts/apply-nestang-patches.sh`, which is idempotent
(`git submodule update` or a fresh clone reverts it, so the script runs before
any build), and every patch is recorded in `THIRD_PARTY.md` beside the other
third-party obligations.

All three target `nand2mario/nestang` at reference commit
`c2450818e1f0c858e13c5dd16746ee5221a5c760`, the revision `tangcore` pins, built
for `console138k` at device revision B.

## 0001-controller-gate.patch

While the OSD is asserted the physical pads — the SNES/DS2 port and nestang's
own FPGA USB host — are gated away from the game, so the hidden game does not
move while the desktop owns the controller. `iosys` is still fed the raw pad, so
the host sees every button and can drive the pointer; the BL616's own injection
(`hid`) is left ungated, because whether that drives the game or the desktop is
the firmware's decision, not the gate's. The legacy 32x28 page, nestang's menu
and every stock host are untouched.

## 0002-desktop-text-layer.patch

Adds `src/iosys/textdisp_wide.sv`: the 80x45 cell layer with per-cell 15-bit
BGR5 foreground and background, read on the pixel clock and written on the main
logic clock. Verified by simulation rather than by build —
`tools/test_textdisp_wide.sh` checks every output pixel of four 16x16 cell
blocks, including the corner cells and the write-port guard that keeps a column
past the grid from wrapping into the next row. A build cannot tell a correct
address decode from a plausible one; that test can.

The colour path is three pixels deep. As first carried, the layer addressed
the store with the raster's own `cx` and so landed three output pixels late,
which this README called invisible. It was not: see `0007`.

## 0003-desktop-layer-wiring.patch

Wires the layer into the core: the three additive `iosys` commands (`0x13` move
the layer cursor, `0x14` write a run of 5-byte cells advancing across the grid,
`0x15` enable), the write port and enable through `nestang_top` to `nes2hdmi`,
the composite after the scaler at output resolution, and the source file in
`build.tcl`.

The layer is shown only while the **overlay** is asserted as well as enabled.
That is deliberate: `nesload` already clears the overlay before releasing a
core, which is what hands the screen to the game, so tying the layer to the same
byte means the desktop is hidden by machinery that already exists and has
nothing new to keep in step.

When it is up it replaces the output entirely — bars included — which is what
full screen means for a desktop.

## 0007-wide-layer-alignment.patch

Aligns the layer with the raster. Pixels 0-2 of every line were computed at the
end of the previous line, in blanking, where the store's address falls back to
cell 0 -- so cell (0,0)'s glyph column was painted down the whole left edge in
that cell's colours, and the marks changed with whatever character sat in the
top-left corner. `textdisp_wide` now takes `frame_width`/`frame_height` from
`hdmi` and addresses the store three pixels ahead of the raster, wrapping into
the next line and frame as `hdmi.sv`'s counters do, so `color` is the pixel for
the coordinate being presented now and matches the stock one-register `rgb`
path. `tools/tb_textdisp_wide.sv` was rewritten to stream a free-running
1650x750 raster and check every visible pixel of a frame; the old testbench
held `cx` still for four clocks per check and could not see any latency.

## Built result

`4,637,898 bytes`, MD5 `926182f43e454194788073375d6ae09b`, with the cell store
inferred as block RAM (**BSRAM 31 → 43 of 340**, LUTs up ~150) and **TNS 0.000
with no setup or hold violations** on every clock.

The artifact size moves with placement and is not a correctness signal: three
revisions of this design have produced 4,608,916, 4,637,898 and 4,608,916 bytes
again, for changes of a condition, a clamp and a comparator. The resource
report is the reliable indicator that the layer is present, and it is (BSRAM 43
against 31 without it). Recorded so the numbers are not mistaken for each
other later.

## Pending

Slice 2 and after: the firmware that drives these commands does not exist yet.
Nothing in this project sends `0x13`/`0x14`/`0x15` today, so the layer is in the
bitstream and never enabled. The transport constants are defined in
`ports/bl616/tang_fpga_link.h`.

## Conventions

- One patch per coherent change, `NNNN-short-slug.patch`, numbered in the order
  they apply.
- Mark patched regions in the source with a `TinyTang:` comment, so a reader
  landing in the file sees that it is not upstream's text.
- Build for the revision `.ai/core-reference.md` names for this board; see
  `DEV-001`/`DEV-002` and the note in `THIRD_PARTY.md`.
