# Menu-core patches

The changes that turn this project's patched nestang into an emulator-free
**menu core** — the board with the NES machine left out, so TinyDesk has a host
of its own rather than riding on an emulator.

These are separate from `third_party/patches/0001`–`0006`, which stay the
official patched NES core. Apply order:

1. `third_party/patches/0001`–`0006`, via `scripts/apply-nestang-patches.sh`
2. `menu/0001-menu-core.patch`, on top

`tools/build_menu_core.sh` does both and builds. The nestang checkout stays
pristine; nothing here is a fork.

## 0001-menu-core.patch

Four files, 89 insertions.

- `src/boards/console138k_menu.v` — the console138k board file plus one define,
  `MENU_CORE`. Everything the board needs to be a board is unchanged: the 50 MHz
  clock, HDMI, the BL616 UART, the two front USB pins carrying the keyboard
  link, and the DS2 controller path (iosys still reports a joypad word upward,
  and the keyboard's pointer mode is folded into it).
- `src/nestang_top.sv` — `ifndef MENU_CORE` around the NES cluster: the `NES`
  machine, the loader-write block, `sdram_nes`, `GameLoader`, the
  `int_audio`/`ext_audio` assigns, the `clkref`/`reset_nes` block, and the
  autofire/joypad shift registers. The `else` ties off the four signals the
  survivors still read — `color`, `scanline`, `cycle`, `sample`, all read by
  `nes2hdmi`. The BL616 link's ROM handshake lines are **not** tied off: iosys
  drives those, and it is the loader that consumed them that is gone.
- `src/boards/console138k_menu.sdc` — the constraints without `fclk`. That
  64.47 MHz SDRAM clock has no consumer once `sdram_nes` is gone, so synthesis
  removes it, and a constraint naming a removed net fails the timing stage
  outright. `clk` becomes a primary clock on its own net, where upstream
  declares it generated from `fclk`; the period is upstream's (15.51 ns ÷ 3).
- `build.tcl` — a third argument, `menu`, selecting the board file, the SDC and
  the artifact name. The emulator's sources stay in the file list and are pruned
  by synthesis because nothing instantiates them.

    gw_sh build.tcl console138k ds2 menu

## Built result

`impl/pnr/nestang_console138k_ds2_menu.bin` — **4,492,288 bytes**, MD5
`c747105422aa4462bffc67c555b2bda3`.

| | menu core | patched NES core |
|---|---|---|
| Logic (LUT/ALU) | 2,781 (3%) | ~12,400 (9%) |
| Register | 1,699 (2%) | ~5% |
| BSRAM | 13/340 (4%) | 43/340 (13%) |
| Bitstream | 4,492,288 B | 4,606,154 B |

The bitstream shrinks only ~2.5% while the logic drops by roughly four fifths:
Gowin's bitstream size is dominated by structure and routing frames, not by how
much logic a design uses. So this is a far smaller, cleaner host — **not**
meaningfully a faster load. That distinction is worth keeping, because the
opposite is the intuitive guess.

## Scope

This first patch leaves the emulator's *sources* in the build's file list; only
the instantiation is gated. Trimming the list can follow once the core is proven
on hardware. Nothing here ports nestang's streaming command path — ROM data,
floppy, PS/2 and the extended channel are absent by construction, not disabled.
