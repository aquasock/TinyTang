# A video-path monitor for the desktop core

## Goal

Give the desktop core the instrument TinyNES's `DIAG=1` player is: per-frame
evidence of what the video path actually emitted, readable from the host while
the user watches the picture. It exists so a future HDMI fault can be
*localised* rather than only seen.

It is not needed to close the fault of TinyTang core-log entry 52, which the
user accepted on a hot-boot pass. Recorded here as a plan; do not build it
without the user asking.

## Facts the design rests on

- The desktop core already carries a host-readable instrument.
  `fpga/desktop/clock_monitor.sv` counts cycles in three domains, is
  instantiated in `desktop_regs.sv`, and answers read32 at `0x120`..`0x134`.
  The shell's `fmon` reads it and `fpgar <addr>` reads any desktop register,
  both over UART1 through the console. Measured on this board on 2026-10-13
  with the entry-52 image loaded and the desk layer up: `clk` 21,503,075 and
  21,505,365 Hz against a 21.492 MHz design, `pix` 74,249,911 and 74,250,208
  Hz against 74.25 MHz -- two probes agreeing to 0.01 percent -- and
  `fpgar 0x134` returning the stamp `0x4d4f4e31`. **The readout path is
  already built and proven; no host tool is needed for this plan.**
- A rate counter cannot see this fault. On TinyNES the vertical-blank period,
  the pixel count and the bit pattern of every frame stayed correct through
  the fault, which is what placed it after the pixel stage (TinyNES
  core-reference DEV-018). The desktop needs the same split, so the new
  instrument has to watch the pixel data and not the clocks.
- `clock_monitor`'s own header states the limit that shapes the design: a
  counter proves a *rate*, not a *phase*.
- The desktop's picture is generated in fabric by `textdisp_wide` and the
  `hdmi/` stack. There is no CPU in that core, so there is no firmware-side
  counter the way TinyNES had one.

## Design

A new leaf, `fpga/desktop/video_monitor.sv`, clocked by `hclk` (74.25 MHz) and
fed the pixel stream at the input of the TMDS encoder in `desktop_hdmi.sv` --
the same point TinyNES's `PIXELS` and `BITS` observed, and for the same reason.

Three values, latched at each frame end and held until the next:

- the frame count, on vertical-blank edges;
- the frame period in `clk` cycles (21.492 MHz), so the pair is an exact
  measurement against a known reference, as TinyNES's `mcycle` comparison was;
- a rolling checksum of the emitted RGB together with a count of pixels seen
  non-zero -- the pair that tells a pixel lost to zero from a pixel that took a
  different non-zero value, which a count alone cannot.

The latched values cross into `clk` as gray code, as `clock_monitor` already
does, and `desktop_regs` answers them at the next free read32 addresses --
`0x138` onward, ahead of the `0x200` cell block.

Nothing else changes. The visible picture, the desk layer, the register map up
to `0x138` and the UART contract are untouched; the monitor is a leaf.

## Readout

Already built. `fmon` reads the clock monitor and `fpgar <addr>` reads one
register, over UART1 through the console with the desk layer running. A pass is
a polling loop of `fpgar` across the new address range, and its numbers can be
read while the user watches the screen, which is what makes it usable in a heat
test.

## Tests

- A testbench that streams a known raster and checks the counters frame by
  frame, in the manner of `tools/tb_textdisp_wide.sv`: a full frame's period in
  `clk` cycles, the checksum of a raster with one pixel flipped, and the
  non-zero count against a raster with a known black count.
- A build that passes the setup/hold and clock-topology gates, and a seed sweep
  of the desktop core's own flow.
- On the board, a hot-boot run with `fpgar` polled, compared against the user's
  eye.

## Risks

- The monitor runs on the pixel clock, the tightest timing in the design, and
  must not become the fault it is meant to find. Its own paths have to be kept
  short, and it must be counted in whatever look-ahead the wide layer needs.
- A checksum proves a difference, not its location: it will say a frame
  changed, not which stage changed it. The pixel-stage split still comes from
  the counter pair, as it did on TinyNES.
- Polling registers competes with the desk layer for the core UART. `fpgar`
  was measured working with the layer up, but a long poll loop should be
  checked against the layer's frames before it is trusted in a soak.
- The fault is currently cured. Building this is not a claim that it is
  present, only that the next one should not have to be found by eye alone.
