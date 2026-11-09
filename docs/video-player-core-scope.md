# A basic video player core — scope note

Recorded 2026-11-09 from a survey of the code on this machine. It is a scope,
not a commitment: do not build from it without the user asking.

## The question

Can a basic video player core be put on the Tang Console 138K in the open
flow, and is there HDL already written that can be ported to it?

Short answer: yes, and most of what a player needs is already proven on this
exact part — but the decoder is the part that is not, and the choice of decoder
decides the size of the project.

## What the board already gives you

All of the following is in this tree or a sibling, and all of it is proven on
`GW5AST-LV138PG484AC1/I0` revision C:

- **Open-flow build** — `tools/build_desktop_core_open.sh` and
  `tools/desktop_open/`: yosys `read_slang`, the GW5AST-138C nextpnr-himbaechel
  fork, apicula `gowin_pack`, with a provenance check that reproduces a
  bitstream. Gowin EDA appears only to generate the device database.
- **HDMI** — `fpga/desktop/hdmi/` plus `desktop_hdmi.sv` (2,331 lines with the
  link): 1280x720, 74.25 MHz pixel clock, TMDS through four `OSER10`s.
- **The BL616 link** — `fpga/desktop/desktop_link.v`, the TangCore frame
  protocol, with the extended-protocol file stream the firmware already
  serves (`fpga_file_stream`, frame type `0x11`) and a per-session baud
  switch to 5 Mbaud.
- **A CPU and memory** — `../TinyPhosphor/src/ae350/` (2,907 lines): the AE350
  hard core at 750 MHz on a 75 MHz bus, 32 KiB boot ROM, 64 KiB BRAM at
  `0x60000000`, and the dock's 32 MiB SDRAM at `0x40000000`, plus the stream
  loader and a Rockbox host that already plays eleven audio codec families
  while streaming from the card through a 4 MiB SDRAM cache. Built and
  qualified by the same open flow.
- **A framebuffer path to SDRAM** — `../TinyMaps/fpga/tinymaps/sdram_ctrl.sv`
  (360 lines, MIT, vended from TinyNES's `ahb_sdram.sv`) drives one Winbond
  W9825G6KH-6, 32 MiB, 16-bit, from the bus clock. Its A25 port is shared with
  the CPU and its video port delivers one 256-byte line per `vstart`, about
  140 clocks. It is running on hardware. `maps_video_rd.sv` (122 lines) is the
  line-store side of a 720p scanout that reads that port, and `maps_hdmi.sv`
  is its 720p output.
- **A register/ABI pattern** — `desktop_regs.sv` and the firmware's `phosphor`
  command show how a core is driven: read32/write32 over the same link, with
  a file server beside it.

## The three ceilings

1. **The link is 431 kB/s.** Measured on this board by TinyMaps at 5 Mbaud
   (4 MiB in 9.7 s); 2 Mbaud gives 190 kB/s. A 12 Mbaud attempt failed timing
   in TinyMaps (189 MHz against the 371 MHz the serializer needs), so 5 Mbaud
   is the top of the proven range. Everything that must cross the link in real
   time — compressed bitstream, not raw frames — shares that 3.4 Mbit/s.
2. **There is no open-flow DDR3.** The only hardware-proven DDR3 in this
   project family is Gowin's vendor IP, used by Tang-PSX and built wholly by
   Gowin EDA. TinyVCMI's `docs/ddr3-open-flow.md` records the open attempts:
   the LiteDRAM GW5 PHY failed setup timing in every placement and the `DDRDLL`
   primitive reports `step = 0` in this flow at 350 MHz and at 400 MHz. Treat
   the 32 MiB SDR SDRAM as the only usable framebuffer memory.
3. **Placement is a lottery.** AE350 images pass one or two seeds in eight, and
   a large logic change can pass none. A full sweep costs hours. Plan work in
   seed-sized increments, not one big drop.

Device resources, for sizing: 138,240 LUT4, 340 BSRAM (6.27 Mbit),
298 `MULTALU27X18`, 12 PLLs. TinyPhosphor's AE350 player uses 21,721 LUT4 and
212 BSRAM.

## What HDL exists on this machine

### Already proven on this part — reuse, do not port

TinyTang's HDMI stack, TinyPhosphor's AE350 subsystem and stream loader,
TinyMaps' SDRAM engine and 720p scanout. Nothing here needs rewriting; it needs
to be assembled into one top and, for the SDRAM, given a second client.

### Small modules that port almost as they are

In `../MiSTer-Raster` (GPL-2.0-or-later, so combinable with this tree's
GPL-3.0-only HDL through the "or later" option):

- `rtl/mpeg2_new/mpeg2_ycbcr_to_rgb_bt601.sv` — 88 lines, pure arithmetic,
  BT.601 and BT.709 with limited-range offsets and final clipping. No vendor
  primitives. This is the colour converter a raw-frame or software-decoded
  player needs in the pixel path.
- `rtl/mpeg2_video_720x480p.sv` — 71 lines, a counter-only 480p raster
  generator with a 50/59.94 Hz selection. Useful only if 480p is wanted
  instead of the existing 720p raster.

### The one large asset

`../MiSTer-Raster/rtl/mpeg2_new/` is 18,283 lines of SystemVerilog implementing
a real H.262/MPEG-2 decoder: bitreader, VLC, inverse quantisation, IDCT,
intra reconstruction, P/B macroblock engines, motion compensation, picture
reordering, plus `rtl/audio/` (604 lines) for MP2 audio. It decodes
progressive 4:2:0 up to 720x480 with I, P and B pictures, entirely in fabric,
and is qualified on Cyclone V — 32,930 of 41,910 ALMs, 4,158,522 block-memory
bits, 60 DSP blocks.

Two things make this a port rather than a copy:

- The reference pictures and the presentation buffer live in MiSTer DDR3
  through `mpeg2_h262_ddram_store_420p.sv` and `mpeg2_h262_ddram_arbiter.sv`
  (about ten files touch the 64-bit native DDR port). Replacing that with the
  16-bit SDRAM line channel is the real work; the line channel is 256-byte
  granular and shared with the CPU, where the DDR port is a wide burst port.
- The top is MiSTer's: `Raster.sv` plus `sys/` (52 files), HPS I/O, OSD, SD
  access over ioctl, Quartus PLLs, and three `altsyncram` instances in
  `mpeg2_luma_framebuffer.sv`. All of that is replaced by the Tang top above.

Fitting is plausible but not established: the design's block memory is
4.16 Mbit against 6.27 Mbit available, and ALM count is not comparable to
LUT4 count, so synthesis through yosys may land well above or below. And the
Gowin flow's known defect — two-clock block RAM reads back wrong — has to be
checked against this design's clock crossings. This is an experiment before it
is a plan.

### The software asset

`../Rockbox_Video_Player` is MPEGPlayer NX: a Rockbox video player, already a
plugin on Rockbox 4.0, decoding H.264 Constrained Baseline (h264bsd, Apache-2.0)
up to 320x240 and AAC-LC (FAAD) from an MP4 file, with audio-master
synchronisation, bounded memory (446 KiB peak), drop and underrun accounting.
Measured on its MIPS target: 10.0/17.6 ms average/worst video decode per frame
over a 60 s 24 fps run, 480 of 480 frames with no drops.

That is a decoder whose performance contract is already written down, on a
32-bit embedded CPU. TinyPhosphor already runs Rockbox on this board's AE350
with the same file-streaming model. The gap is the CPU's speed and caches, not
the codec's existence.

## Three ways to build it

### A. Frame player — raw frames over the link

BL616 sends pre-decoded frames from the card; the FPGA stores them in SDRAM and
scans them out. HDL is TinyMaps' SDRAM engine plus a sequential line-fetch
scanout with integer upscaling; host side is ffmpeg writing raw frames.

Bandwidth, at 431 kB/s, with a 720p output upscaled in fabric so only source
lines are read from SDRAM:

- 160x120 RGB565, 38.4 kB per frame — 11 fps
- 160x120 YUV420, 28.8 kB — 15 fps
- 320x240 RGB565, 153.6 kB — 2.8 fps
- 320x240 YUV420, 115.2 kB — 3.7 fps

Estimate: one to two weeks. It is a slideshow, not a player, but it is a strict
subset of B and proves the scanout on hardware first.

### B. AE350 software H.264 — recommended

The AE350 decodes with MPEGPlayer NX's h264bsd, writes RGB565 (or YUV) into
SDRAM, and the fabric scans it out; audio goes out the existing I2S2 and HDMI
audio paths, or AAC is decoded by the same player. The file crosses the link
once at 5 Mbaud, which a 320x240 24 fps stream at ~600 kbit/s leaves 90 percent
of the link free.

What is new: a CPU-writable framebuffer window with a tear-free flip at
vertical blank, a sequential scanout with integer scaling (A's HDL), and the
port of the player's output stage from Rockbox's LCD blitter to that
framebuffer. Everything else exists.

The risk to measure first: h264bsd needs ~50 fps of decode headroom at 320x240
on a 1 GHz-class core. At 750 MHz with 32 KiB caches over a 32-byte-line SDRAM
at roughly 140 clocks a line, throughput and cache behaviour are the question,
not the algorithm. A decode-only benchmark on the existing AE350 image answers
it in a day, before any HDL is written.

Estimate: four to eight weeks.

### C. Fabric MPEG-2 — the real port

Port the H.262 decoder, retarget the DDR store to SDRAM, replace the MiSTer
top, fit and close timing. No CPU needed, best picture, no link in the decode
path once the file is cached — but the effort is months and the fit is not
proven.

Estimate: three to six months, with the first month spent only on whether
`read_slang` elaborates the 18k lines, whether it fits 340 BSRAMs, and whether
the two-clock BRAM gate can be met. If any of those three fail, the option
should stop there.

## Recommendation

Build B, with A as its first milestone:

1. **Probe, one day each, before writing HDL.** (i) Run yosys `read_slang` over
   `MiSTer-Raster/rtl/mpeg2_new` and report elaboration errors and a size
   estimate — this decides whether C is ever viable. (ii) Build h264bsd for
   RV32 and time a 320x240 stream on the AE350 image — this decides B's frame
   size and rate. (iii) Confirm the link's 5 Mbaud path works with a new core
   ID from this tree's firmware, not only TinyMaps'.
2. **Milestone A.** SDRAM framebuffer, 720p scanout, host tool, moving picture
   on screen from raw frames. Proves the only new HDL.
3. **Milestone B.** Port the player output stage; play one MP4 at 320x240 with
   audio.

Do not start with C, and do not start with the fabric until the probes say the
decoder elaborates and fits.

## Rules that must not be re-litigated

- No DDR3 in the open flow. SDR SDRAM only.
- The link is the streaming ceiling; the decode cannot live on the far side of
  it.
- Two-clock block RAM is broken in this flow; keep each BRAM single-clock.
- An AE350 image is not qualified until it passes its gates on a seed, and a
  large change may pass none — budget the sweep.
- Reuse the proven HDMI, SDRAM and AE350 blocks rather than writing new ones;
  the new work is the framebuffer client and the scanout.

## Verification

- Simulation: a scanout testbench in the manner of
  `../TinyMaps/sim/tb_maps_line_store.sv`, checking one frame's pixels against
  a reference image; a framebuffer flip test that a write during active video
  never tears.
- Build: the existing gates (`ae350_gates.py`, `check_netlist.py`) plus a
  seed sweep.
- On the board: a known clip, the register frame counter advancing, a
  decoded-frame checksum compared against a host decode of the same file, and
  the user's eye on motion and A/V sync. A still frame proves the path; only
  motion proves the cadence.

## What this note does not decide

Frame size and rate, container and codec profile, whether audio is I2S2 or
HDMI only, and whether the player is a standalone core or a TinyDesk app.
Each is a user decision once the probes are in.
