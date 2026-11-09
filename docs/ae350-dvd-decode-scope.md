# Can the AE350 decode DVD-quality video?

Recorded 2026-11-09. A scope, not a commitment. Every number below is either
measured on this board or derived from a published figure with the derivation
shown, so a reader can disagree with the arithmetic rather than with an opinion.

## The question

TinyPhosphor runs Rockbox on the AE350 and plays eleven audio codec families.
Can the same core decode DVD-quality video — 720x480 at 29.97 fps, or 720x576
at 25 — given that data arrives fast enough?

Short answer: **compute and memory are each marginal, and together they put one
frame's work within a few percent of the frame budget either side of passing.**
It is not a comfortable yes, and the three variables that decide it are all
cheap to measure. It is also, before any of that, a question that the link has
already partly answered — see *The ceiling nobody asked about*, below.

## What is already in the tree

- **`third_party/rockbox/apps/plugins/mpegplayer/libmpeg2`** — libmpeg2 from
  mpeg2dec 0.4.0b, GPL-2.0, 6,229 lines. Its own README states the profile:
  "main profile" for MPEG-2, which is exactly DVD's Main Profile at Main Level,
  and it has no resolution cap (`header.c` checks only the MPEG-2 syntax flag
  `vertical_position_extension`). So the decoder that DVD needs is already here.
- **Fed to Rockbox, not to a plugin host.** `software/rbhost` builds the
  `rbcodec` libraries for rv32imafdc and embeds them in AE350 programs; it does
  not build `apps/plugins`. mpegplayer is a plugin, so using libmpeg2 means
  driving `mpeg2_*` from an rbhost-style program, not building the plugin.
- **No RISC-V kernels.** libmpeg2's fast paths select on `CPU_ARM_CLASSIC` and
  `CPU_COLDFIRE` (`idct_arm.S`, `idct_armv6.S`, `motion_comp_arm_s.S`,
  Coldfire equivalents). RISC-V falls through to `idct.c`'s integer butterfly
  and `motion_comp_c.c`'s byte-at-a-time saturating copy. **This is the single
  largest unknown in the estimate below.**
- **AC-3 is already qualified on this board.** Rockbox's `a52` codec is in the
  rbhost codec set, and TinyPhosphor's twelve-format corpus played AC-3 with
  480,002 samples and zero underruns (core-log entry 9). DVD audio needs no new
  work.
- **A benchmark harness that takes a codec and an input and times it**
  (`make bench BENCH_CODEC=... BENCH_INPUT=...`). That is the vehicle for the
  measurement this note recommends, and it needs no HDL.

## What the board measures

From TinyPhosphor and Tang-PSX `.ai/core-reference.md`, all hardware-measured:

- **Core clock 750 MHz.** A dependent `addi` chain ran at 0.94 to 0.99 IPC
  (AE350-007); 2^30 counted cycles completed in 1.48 s. That is the ceiling for
  a chain with no loads, no branches and no misses — not an estimate of real
  code.
- **L1: 32 KiB instruction and 32 KiB data**, 4-way, 32-byte lines
  (`micm_cfg = mdcm_cfg = 0x00439ada`, AE350-004). Write-back D-cache.
- **No hardware prefetch.** `mcache_ctl` bits 9 and 10 read back zero on this
  part (AE350-008). The core cannot hide a miss it did not ask for.
- **A D-cache miss to DDR3 costs ~570 core cycles and misses do not overlap**
  (AE350-009). A hit costs ~3.
- **A miss served by Tang-PSX's 128 KiB fabric L2 costs ~240 core cycles**
  (AE350-010).
- **Streaming bandwidth, cached, sequential:** 1 MiB read in 9.34 M cycles
  (84 MB/s), 1 MiB write in 7.60 M cycles (103 MB/s). Those are Tang-PSX's
  DDR3 numbers *through the fabric L2*.
- **TinyPhosphor has no L2.** Its memory is the dock's 32 MiB SDR SDRAM
  (`sdram_ctrl.sv`), whose peak is 16 bits at the bus clock — 150 MB/s — and it
  gives video lines priority over the CPU. Tang-PSX's 163-line Migen
  `gateware/l2_cache.py` is the mitigation and is not in this design.

## The arithmetic

Nothing here is a measurement of libmpeg2 on this CPU; it is a bracket, built
from libmpeg2's own published figure and the measured core facts, and it is
meant to be replaced by the benchmark in *The one-day probe*.

libmpeg2's README: "about 110 fps on a PIII/666" for typical DVD streams, "less
than 20 cycles per output pixel" — 6.05 M cycles for a 720x480 frame, i.e.
17.5 cycles/pixel, **with MMX IDCT and motion compensation**.

RISC-V here has no SIMD and no asm kernels, so the two kernels that MMX
accelerates run in generic C. The IDCT (`idct.c`) is a per-element integer
butterfly with 16-bit loads and stores: about 600-900 instructions per 8x8
block against MMX's ~100, so ~10-14 instructions per pixel. Motion
compensation in `motion_comp_c.c` is a per-byte saturating add: ~8-20
instructions per predicted pixel against MMX's ~3. Slice parsing scales least,
perhaps 1.3x. Weighted, that is roughly **1.8-2.5x the PIII/MMX cost →
30-50 cycles per pixel**, and real-code IPC below the 0.95 of a dependent
chain.

Frame budget at 29.97 fps is 33.37 ms. 720x480 is 345,600 pixels.

- 30 cycles/px → 13.8 ms/frame (72 fps ceiling)
- 40 cycles/px → 18.4 ms/frame (54 fps)
- 50 cycles/px → 23.0 ms/frame (43 fps)
- 60 cycles/px → 27.7 ms/frame (36 fps)

Compute alone therefore looks survivable. The problem is that it is not alone.

### Memory, which does not overlap with compute

Per frame in 4:2:0, a frame is 518,400 bytes:

- write the output frame: 0.52 MB
- read one reference (P): 0.52 MB; two (B): 1.04 MB
- average over a 2B-picture GOP: 0.41 MB read + 0.52 MB written ≈ 10 ms of bus

Motion compensation is worse than a stream, because it is line-granular. A
16x16 luma block touches ~16 separate 32-byte lines (rows of 16 bytes, 720 bytes
apart), plus chroma. At 1,350 macroblocks a frame and 570 cycles a missed line,
that is 12-25 ms/frame if every fetch misses; at the L2's 240 cycles it is
5-11 ms. **The L2 is the difference between viable and not, and only Tang-PSX
has it.**

The display adds its own traffic: scanning 720x480 4:2:0 out of SDRAM once per
refresh is 31 MB/s, 37 percent of the measured 84 MB/s streaming rate; RGB565
would be 41 MB/s. Writing RGB565 from the CPU instead of YUV420 would move that
cost into the decoder's write traffic.

Adding the three terms serially — which is the honest model, because this core
has no prefetcher, no miss overlap and one issue stream:

| case | compute | memory | scanout | total vs 33.37 ms |
|---|---|---|---|---|
| tuned 30 cyc/px, L2 hits | 13.8 | 6.4 | 6.2 | **26.4 ms, 21% under** |
| mid 40 cyc/px, mixed L2 | 18.4 | 9.9 | 6.2 | **34.5 ms, 3% over** |
| untuned 50 cyc/px, MC misses | 23.0 | 19.0 | 6.2 | **48.2 ms, 44% over (≈20 fps)** |

**So: 720x480 at 29.97 fps is on the line, and the naive port lands on the wrong
side of it.** The realistic software envelope without further work is 720x480
at 12-20 fps, or 480x360 at 30 fps, or 720x480 at 30 with tuned kernels and an
L2.

## The ceiling nobody asked about

"Assuming it can get the data fast enough" — it cannot, quite. The BL616 serves
files over the UART link, measured at 431 kB/s at 5 Mbaud (TinyMaps, 4 MiB in
9.7 s). That is **3.45 Mbit/s**, against a DVD's 10.08 Mbit/s peak and its
typical 4-6 Mbit/s.

So a real DVD stream cannot be delivered in real time whatever the decoder
does, and the 32 MiB SDRAM holds only ~75 s of a 3.5 Mbit/s stream, so
pre-loading a feature is not an option either. What *is* reachable is a
**720x480 MPEG-2 encode at 2.5-3 Mbit/s** — soft, but genuinely 480-line DVD
resolution — streamed live. Conveniently, a lower bitrate costs *less* to
decode (more skipped blocks, fewer coefficients), so the link ceiling and the
CPU ceiling push the same way.

If full-rate or full-bitrate DVD is the requirement, the link has to change
first, and the only proven lever on this board's UART PHY is 5 Mbaud.

## What would move it

1. **RISC-V kernels for IDCT and MC** — the highest-value work, and the one
   least proven. ARM's `idct_arm.S` and `motion_comp_arm_s.S` are the model;
   RISC-V has no packed byte arithmetic, so expect SWAR on 4 pixels per word,
   not parity with MMX. Best guess 40 → 25-30 cycles/px, which is the
   difference between 3% over and 20% under.
2. **The fabric L2** — Tang-PSX's `l2_cache.py` is 163 lines of Migen. Porting
   it means re-expressing it in the TinyPhosphor flow's terms, not copying it.
   Worth 5-14 ms/frame.
3. **Decode YUV and convert in fabric** — reuse MiSTer-Raster's 88-line
   `mpeg2_ycbcr_to_rgb_bt601.sv` (BT.601/709, limited range, final clip, no
   vendor primitives) at the scanout, and never touch RGB in software. Removes
   a per-pixel software conversion and its writes.
4. **Half-resolution decode with a 2x fabric upscale** — 1/4 the compute and
   1/4 the memory, comfortably inside budget at 30 fps, but the picture is
   VCD-grade, not DVD. This is the fallback if the bench disappoints.
5. **Frame dropping** — decode every other frame. Full resolution, 15 fps
   judder, and two-thirds of the budget back.

## The one-day probe

Before any HDL: add libmpeg2 to the rbhost bench and time a 720x480 clip on the
AE350. It needs no new gateware, no L2, and it replaces the whole bracket above
with a measurement:

```sh
make bench BENCH_CODEC=mpeg2 BENCH_INPUT=clip720x480.m2v BENCH_NAME=mpeg2dvd
tools/ae350_run.py upload build/bench/mpeg2dvd.tpi
tools/ae350_run.py run mpeg2dvd.tpi        # decode-only, no display
```

Read three numbers off it: **cycles per pixel**, **cycles attributable to
D-cache misses** (a cycle counter around the MC path, or a run with the L2
absent versus the SDRAM line channel), and **frames per second at 720x480**.
A pass at ≥35 fps decode-only means DVD resolution is reachable with margin; a
result near 30 fps means the kernels and the L2 are both mandatory; anything
under 25 fps means software is not the answer and the fabric decoder is.

## If the answer has to be full-rate DVD

Then the fabric decoder is the path, and the fit looks better than the earlier
note in this tree implied. MiSTer-Raster's H.262 decoder is qualified on
Cyclone V `5CSEBA6U23I7` at 32,930 of 41,910 ALMs, 545 of 553 RAM blocks
(4.16 Mbit of block memory) and 60 DSP blocks. A Cyclone V ALM is a fracturable
8-input LUT, roughly 1.6 4-LUT equivalents, so that is ~52,000 4-LUTs against
the GW5AST's **138,240 LUT4** — about 38 percent. Block memory is 4.16 Mbit
against **6.27 Mbit** (66 percent), and 60 small multipliers against **298
MULTALU27X18**. Nothing in that comparison is a guarantee — ALM and LUT4 are not
interchangeable, and Yosys is not Quartus — but it is no longer a reason to
expect the design not to fit.

What still blocks it: the reference frames live behind MiSTer's 64-bit DDR3
port, and the open flow has no DDR3. The 16-bit SDR SDRAM at 150 MB/s peak can
carry 40 MB/s of reference traffic, so the port is a retarget of
`mpeg2_h262_ddram_store_420p.sv` and its arbiter to the line channel in
`sdram_ctrl.sv` — the same work the earlier scope note identified, now with a
more favourable fit estimate behind it.


## Measured, 2026-11-09

The bracket above was replaced by a measurement. `software/mpeg2probe` in
TinyPhosphor compiles libmpeg2 from the Rockbox submodule unmodified for
rv32imafdc and times a 720x480 29.97 fps 4 Mbit/s closed-GOP stream (150
pictures: 11 I, 40 P, 99 B) embedded in the image. Correctness first: the
decoded luma matches ffmpeg to a maximum absolute difference of 1-2 on 0.18
percent of samples on the host, and the AE350 returns the host build's luma
checksums exactly (`0x59ee2e4c`, `0x7c5e122e`).

- **84.32 cycles per pixel**, 18.80 instructions per pixel, **IPC 0.223**
- **38.86 ms per frame, 25.74 fps decode-only** against a 33.37 ms budget:
  **1.16x over**, before any scanout or audio shares the same memory
- Core clock confirmed at 749.9 MHz by two independent clocks; a second run
  moved cycles by 2.5 percent with instructions bit-identical

So the guess landed between the "mid" and "pessimistic" cases in the table
above: not a comfortable yes, and not viable as it stands.

The important part is *why*. A 352x240 clip measured 90.59 cycles per pixel
at the same IPC, which looks like evidence against memory -- but motion
compensation's traffic per pixel is constant with frame size, so a smaller
clip never tests cache pressure. The decomposition instead used three
independently measured constants and no fitted parameters (570 cycles a miss
with no overlap; 0.95 IPC on a dependent chain; 33 cache lines per macroblock
from geometry) and reproduced both runs within +10.6 and +5.2 percent:
**23.5 percent compute, 87.1 percent non-overlapping misses.**

That makes the conclusion sharper than "software decoding is marginal":

- the decode is **stall bound, not instruction bound**; RISC-V IDCT and
  motion-compensation kernels would buy little on their own
- **the fabric L2 is the first and largest lever** -- at 240 cycles a hit the
  model gives 23.4 ms per frame, **42.8 fps**, which is real headroom for the
  first time
- TinyPhosphor has no L2, and its DDR3-bridge counters at `0xe80000a0` read
  zero, so this attribution is modelled rather than counted, and that register
  block does not exist in the SDRAM variant

Runtime: the probe ran through TinyTang's `phosphor run`, after `ae350run`
proved to be a TinyNES path this core does not answer. Rebuild and re-run with
`make -C /run/media/vash/GIT/TinyPhosphor/software/mpeg2probe ae350` and
`phosphor run /root/cores/mpeg2probe.tpi`; the results are read from the AE350
register block with `phosphor peek`.

## Recommendation

The probe answered the question it was built for, and the answer is that the
decoder is not the problem -- the memory path is:

1. **Port the 128 KiB fabric L2** (`gateware/l2_cache.py` in Tang-PSX, 163
   lines of Migen, re-expressed for this flow). The model says this alone takes
   38.86 ms to 23.4 ms per frame, 25.7 fps to 42.8 fps, which is the first
   configuration with real headroom for a 720x480 scanout and audio.
2. **Re-measure with the L2 in place.** The model predicts; a second run of the
   same probe measures. If it lands near 42 fps the software player is viable
   and the remaining work is the framebuffer and scanout from the previous
   scope note. If it does not, the stall term was not the L2's to fix.
3. **Only then consider RISC-V kernels.** They address the 23.5 percent compute
   term, so on their own they buy a fraction of what the L2 does, and libmpeg2's
   ARM and Coldfire assembly cannot be transliterated -- RISC-V has no packed
   byte arithmetic.
4. **Re-target the bitrate to the link.** At 431 kB/s the link carries
   3.45 Mbit/s, so the target is a 720x480 encode at 2.5-3 Mbit/s. This costs
   nothing in HDL and removes the one ceiling no decoder work touches.

Building the player before the L2 would produce one that cannot hold 480p, which
is the outcome the measurement was meant to avoid. The fabric H.262 decoder
stays in reserve: it is the answer if step 2 disappoints, and the fit estimate
above is more favourable than the first note in this tree implied.

Either way the AE350 is not wasted. It is the only thing on the board that can
demux, serve, synchronise and run AC-3, and it already does four of those five.

## Open questions this note does not settle

- Real-world IPC for libmpeg2 on the A25: measured, 0.223 overall, and the
  decomposition above accounts for it. What is still modelled rather than
  counted is the miss cost specifically, because this image has no bridge
  counters.
- Whether the SDR SDRAM's 16-bit line channel, shared with a CPU cache-miss
  stream, an L2 fill stream and a video scanout, sustains the assumed split.
  Still unmeasured: the probe decoded without displaying, so scanout has not
  yet competed with the decoder for the same memory.
- Whether a 2.5-3 Mbit/s 720x480 encode is what the user means by "DVD
  quality" — this is the one question that changes the target, and it is the
  user's to answer, not this note's.
