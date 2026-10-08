# TinyTang desktop core

`desktop.bin` hosts TinyDesk without an emulator. It keeps the existing
80×45 desktop layer, UART keyboard reports, injected input, front-port
keyboard link and F12 switching. HDMI and PmodVGA mirror the same compositor;
the background is black when the layer is disabled. Physical DS2 controllers
and the old LED outputs are removed to release the PMOD pins.

The source is reconstructed from nestang commit
`c2450818e1f0c858e13c5dd16746ee5221a5c760`, the seven common patches, the menu
patch and the desktop patch. `tools/build_desktop_core.sh` creates an isolated
checkout and never edits the supplied upstream checkout. It needs that commit
in `NESTANG_DIR` (default `../tangcore/nestang`), Gowin EDA 1.9.11.03 and Python.

```bash
tools/tests/test_desktop_core.sh
tools/build_desktop_core.sh 2
# build/desktop/place2/desktop.bin
tools/sweep_desktop_core.sh
# build/desktop/sweep/{place0,place1,place2,place3}/desktop.bin
# build/desktop/sweep/summary.md and summary.json
```

Placement accepts 0–4, default 2. `GOWIN_SH` overrides the tool executable and
`DESKTOP_BUILD_DIR` overrides the build directory. Reports and a source
fingerprint are preserved with each placement. The build gates the published
binary on zero setup/hold violations and nonnegative path slack. Constraints
target GW5AST-LV138PG484AC1/I0 revision C with a 50 MHz oscillator, 21.492 MHz
main clock and 74.25 MHz pixel clock. VGA uses the HDMI 1280×720 raster,
1650×750 total, positive 40-pixel HSync and 5-line VSync. RGB is the top four
bits of each compositor channel, forced black outside the visible region.
VGA blanking and sync are registered once, matching the compositor's RGB
register. Using raw raster timing with that RGB exposes a previous-line
blanking pixel at the left edge and drops the last visible pixel.

The HDL and carried upstream modifications are GPL-3.0-only. Build tools are
MIT; see `THIRD_PARTY.md`. The synchronizers and held-data handshake were
designed independently after reviewing colibri conventions.

## Configuration

TinyTang firmware recognizes UART core ID `0x54` and applies `/tang.ini`
after each successful `tangload`. The tested normal VGA seating is **J1 on
PMOD1 (nearest HDMI), J2 on PMOD0**, face up:

```ini
[tang]
pmod0 = vga_j2
pmod0_flip = no
pmod1 = vga_j1
pmod1_flip = no
```

This produces `0xc0 = 0x0230`. The first desktop ABI supports both sockets
released or a complete VGA pair, in either order, with independent row flips.
Unsupported module words are rejected without changing the active word.
Sockets start released after reset. OLED, encoder and I2S are currently
Phosphor features; they are not driven by this desktop core.

The boot script loads `/cores/console138k/desktop.bin`. Preserve the old
`nestang-menu.bin` and boot script on the card for rollback. Firmware must
include the `0x54` entry in `pmod_sockets.cpp` for automatic VGA declaration.

## Register ABI 1.0

The endpoint accepts version-1 extended TangCore command `0x10` with the
existing big-endian fields and CRC-16/CCITT-FALSE. Read32 and write32 are
supported; capabilities returns `3`. Replies use response `0x16`.

| Address | Access | Meaning |
|---|---|---|
| `0x00` | Read | Identity `0x00544453` (TDS) |
| `0x04` | Read | Desktop ABI `0x00010000` |
| `0x08` | Read | Personality bitmap `0x0000000d` (none, VGA J1, VGA J2) |
| `0xc0` | Read/write | Socket declaration |

The socket word uses personality nibbles `[7:4]` and `[11:8]`, and row flips
`[12]` and `[13]`. All other bits must be zero. Personalities are 0 (released),
2 (VGA J1) and 3 (VGA J2). A validated write commits at the beginning of
vertical blank; its reply waits for the pixel-domain acknowledgement. The
held word and synchronized toggles prevent a partial pin configuration.

Status values are implementation choices: 0 success, 1 unsupported version,
2 unsupported opcode, 3 bad CRC, 4 unknown address, 5 bad payload length and
6 unsupported socket word. No malformed request changes the sockets.

The regression exercises the real 2 Mbaud UART, reply CRCs through an
independent Python implementation, legacy keyboard/layer traffic, invalid
requests and acknowledgement latency. An independent PMOD simulation checks
row mapping and flips, released pins, frame-boundary commits, blanking and
sync counts across a full 720p frame. The legacy desktop decoder regression
also runs without `DESKTOP_CORE`. A streamed compositor regression independently
checks the physical VGA color/sync pins over a full raster and all HDMI visible
pixels against a reference glyph image, including line and frame wraps.
