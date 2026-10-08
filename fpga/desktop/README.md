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
Sockets start released after reset. ABI 1.1 additionally supports OLEDrgb
on either socket, including two panels sharing the same terminal. Encoder
and I2S remain Phosphor features. The OLED build is deployed and was
accepted by the user on hardware (core-log entry 65).

The boot script loads `/cores/console138k/desktop.bin`. Preserve the old
`nestang-menu.bin` and boot script on the card for rollback. Firmware must
include the `0x54` entry in `pmod_sockets.cpp` for automatic VGA declaration.

## Register ABI 1.1

The endpoint accepts version-1 extended TangCore command `0x10` with the
existing big-endian fields and CRC-16/CCITT-FALSE. Read32 and write32 are
supported; validated block writes use command `0x12` and capabilities
returns `0x13`. Replies are `0x10` frames. A block write follows EXTCTL-002:
version, opcode `0x04`, sequence, first address, a one-byte count, the
words and the CRC, a frame length of 12 plus four per word; the reply's
data is the count. ABI 1.1's first build read a 32-bit count instead and
refused every block the firmware sent; the UART regression now replays
frames made by the firmware's own encoder.

| Address | Access | Meaning |
|---|---|---|
| `0x00` | Read | Identity `0x00544453` (TDS) |
| `0x04` | Read | Desktop ABI `0x00010001` |
| `0x08` | Read | Personality bitmap `0x0000000f` (none, OLEDrgb, VGA J1, VGA J2) |
| `0xc0` | Read/write | Socket declaration |
| `0x100` | Read | Geometry `0x00180010`: 24 columns, 16 rows |
| `0x108` | Read | Completed OLED frame count |
| `0x10c` | Read | CRC32 of the last emitted RGB565 frame, high byte first |
| `0x114` | Read/write | Cursor visible bit 16, row bits 15:8, column bits 7:0 |
| `0x200`–`0x7fc` | Write | 384 row-major cells, word-aligned |

The socket word uses personality nibbles `[7:4]` and `[11:8]`, and row flips
`[12]` and `[13]`. All other bits must be zero. Personalities are 0 (released),
1 (OLEDrgb), 2 (VGA J1) and 3 (VGA J2). A validated write commits at the beginning of
vertical blank; its reply waits for the pixel-domain acknowledgement. The
held word and synchronized toggles prevent a partial pin configuration.

Status values are implementation choices: 0 success, 1 unsupported version,
2 unsupported opcode, 3 bad CRC, 4 unknown address, 5 bad payload length and
6 unsupported value. No malformed request changes sockets or text cells.

The regression exercises the real 2 Mbaud UART, reply CRCs through an
independent Python implementation, legacy keyboard/layer traffic, invalid
requests and acknowledgement latency. An independent PMOD simulation checks
row mapping and flips, released pins, frame-boundary commits, blanking and
sync counts across a full 720p frame. The legacy desktop decoder regression
also runs without `DESKTOP_CORE`. A streamed compositor regression independently
checks the physical VGA color/sync pins over a full raster and all HDMI visible
pixels against a reference glyph image, including line and frame wraps.

## OLED terminal

The OLED is a separate shell display with a fixed 24×16 grid, filling its
96×64 pixels with original 4×4 glyphs. ASCII 32–126 is supported; unsupported
printable Unicode uses one boxed fallback cell. The display conversion does
not change command input or file contents. Each 16-bit cell stores character
7:0, foreground 11:8 and background 15:12 in a 16-colour palette. Upper bits
of a write32 value must be zero. Block writes validate the whole request
before applying 1–64 consecutive cells; a rejected block applies none.
Cursor updates use the same vertical-blank handshake as socket declarations.
The panel clocks SPI at a 161.6 ns period and latches both pixel bytes before
sending them. Its frame signature covers the serialized pixels.

The BL616 app **OLED Terminal** owns an independent TinyDesk Shell session
and compact terminal emulator, fixed at 24 columns for the line editor.
Keyboard and paste input go there only when its window has focus; output
continues while unfocused or closed. The shell task and script workers use
a task-local terminal route, separate from TinyConsole and the existing
Terminal app. Opening the app allocates a session and a 16 KB task stack,
about 25 KB of heap on hardware; `crash tasks` lists every task's stack
margin. The background desktop poll task sends changed cell blocks and
cursor state. After each core load it asks the core's ID once with the
legacy command and sends extended frames only to this desktop core, so a
game core is never sent them. It pauses for core replacement and checks
desktop identity, ABI and OLED configuration once a second. Starting
another desktop, or `tangput`, from the OLED shell is refused, and a second
`tangload` is refused while one is programming. Both root sessions share
`~/.tdsh_history`, so commands entered in both at the same instant can lose
a history line.

Diagnostic shell commands are `oledterm start`, `oledterm status`, and
`oledterm run "echo hello"`. The last submits a command to the independent
session. Run `tools/make_oled_font.py` to regenerate the font and native
proof sheet under `build/oled-terminal/`. Additional regressions:

```bash
bash tools/tests/test_oled_vterm.sh
bash tools/tests/test_desktop_oled.sh
bash tools/tests/test_osd_desk.sh
bash tools/tests/test_oled_link.sh
bash tools/tests/test_oled_session.sh
bash tools/tests/test_tang_flash.sh
```

`test_oled_session.sh` runs the real app, terminal routes, script workers and
printf layer on POSIX threads (`tools/tests/stubs/rtos_threads/`) inside
TinyDesk's window manager. The OLED-and-encoder card layout uses
`pmod0 = oledrgb` with PMOD1 released, giving `0xc0 = 0x0010`.
