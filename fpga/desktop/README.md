# TinyTang desktop core

`desktop.bin` hosts TinyDesk without an emulator. It keeps the existing
80×45 desktop layer, injected input and F12 switching, and carries the I2C
bus for the desktop clock's RTC on the USB-A port. HDMI and PmodVGA mirror the
same compositor; the background is black when the layer is disabled. Physical
DS2 controllers, the old LED outputs and, since ABI 1.5, the wired keyboard
link are removed; the keyboard and mouse are Bluetooth, on the BL616.

Every source is in this directory: `desktop_top.sv`, the UART link
(`desktop_link.v`), the compositor and HDMI output (`desktop_hdmi.sv`, `hdmi/`),
the 80×45 layer (`textdisp_wide.sv`), the register endpoint, PMOD sockets and
the I2C engine (`desktop_pmod.sv`, `i2c_master.sv`), OLED terminal and PLL
wrappers. The top, link and compositor
began as nestang's `nestang_top.sv`, `iosys_bl616.v` and `nes2hdmi.sv`
(nand2mario, GPL-3.0); the NES machine, cartridge loader, controllers, frame
buffer and scaler are gone. No nestang checkout or patch is involved.

The open flow (`tools/build_desktop_core_open.sh`) needs only yosys, the
nextpnr and apicula forks and Python. The Gowin reference build below needs
Gowin EDA 1.9.11.03.

```bash
tools/tests/test_desktop_core.sh
tools/build_desktop_core_open.sh
# build/desktop/open-s<seed>/desktop.bin
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
after each successful `tangload`. The file names one of two layouts, each
module seated right side up. The VGA seating is **J1 on PMOD1 (nearest
HDMI), J2 on PMOD0**:

```ini
[tang]
layout = vga
```

This produces `0xc0 = 0x0230`. `layout = i2s2_oled` seats the I2S2 in PMOD1
and the OLEDrgb in PMOD0; this core has no I2S2, so the firmware sends it the
OLED alone, `0xc0 = 0x0010`, rather than a word it would refuse. The first desktop ABI supports both sockets
released or a complete VGA pair, in either order, with independent row flips.
Unsupported module words are rejected without changing the active word.
Sockets start released after reset. ABI 1.1 additionally supports OLEDrgb
on either socket, including two panels sharing the same terminal. ABI 1.3 and
1.4 put the I2C engine on a socket (`i2c`, then `i2c_uart` with an 8N1 UART
for a LoRa module); ABI 1.5 removed both personalities and the UART and moved
the engine to the USB-A port, below. Encoder and I2S remain Phosphor
features. The OLED build is deployed and was accepted by the user on hardware
(core-log entry 65).

## The I2C bus on the USB-A port

From ABI 1.5 the I2C engine is always on, on the data pair of the populated
USB-A port, the only pair that reaches the FPGA on a stock dock: SDA on D+
(H13) and SCL on D- (G13), open-drain, as TinyNES and TinyGBC wire their
first Wii controller. Adafruit's Wii Nunchuck Breakout Adapter on that port
carries the bus's pull-ups, and the DS3231 RTC sits on the same STEMMA QT
chain at 0x68, beside the Wii controller at 0x52. The bus needs no
`/tang.ini` declaration. The firmware's `tangi2c` command and the desktop
clock use it while this core is loaded; a game core that replaces this one
drives the same pair as its own controller bus.

The engine reads SCL back as well as driving it. Once it releases SCL it waits
for the line to read high before timing the high half of the bit, as
TinyNES's Wii master does, because a Wii controller stretches the clock: an
engine that runs the clock blind samples the controller's ACK before it is
given, and a scan then finds the DS3231 alone. SCL held low for 1 ms ends the
transfer, or the whole scan, with a NACK and both lines released, so an empty
port, whose lines the dock's 15K pull-downs hold low, or a stuck device cannot
hang the engine.

A read writes the register, sends STOP, waits 200 us and reads in a
transaction of its own; it never uses a repeated START. A Wii controller ACKs
a read after a repeated START but sends only 0xff, and needs the pause to load
its bytes (TinyNES waits the same); the DS3231 keeps its register pointer
across the STOP. A scan probes each address from 0x08 to 0x77 with a one-byte
read that it NACKs, so a probe never writes to a device.

The boot script loads `/root/cores/console138k/tinytang.bin`; removing or
renaming the script leaves the board at a plain prompt. Firmware must include
the `0x54` entry in `pmod_sockets.cpp` for automatic VGA declaration.

## Register ABI 1.5

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
| `0x04` | Read | Desktop ABI `0x00010005` |
| `0x08` | Read | Personality bitmap `0x0000000f` (none, OLEDrgb, VGA J1, VGA J2) |
| `0xc0` | Read/write | Socket declaration |
| `0x100` | Read | Geometry of the active font set: `0x00180010` (24×16), `0x000c0008` (12×8) or `0x00100008` (16×8) |
| `0x108` | Read | Completed OLED frame count |
| `0x10c` | Read | CRC32 of the last emitted RGB565 frame, high byte first |
| `0x114` | Read/write | Cursor visible bit 16, row bits 15:8, column bits 7:0 |
| `0x118` | Read/write | OLED font set: 0 Graphics (reset), 1 Standard, 2 Compact |
| `0x140` | Read | I2C status: valid, busy, nack, scan done, sequence, scan address and count |
| `0x144`–`0x148` | Read | I2C read window, 8 bytes |
| `0x154` | Read/write | I2C command: op `[1:0]`, address `[14:8]`, register `[23:16]`, length `[28:24]`, start `[31]` |
| `0x158` | Read/write | I2C automatic read: address `[6:0]`, register `[15:8]`, length `[20:16]`, enable `[31]` |
| `0x160`–`0x164` | Read/write | I2C write window, 8 bytes |
| `0x200`–`0x7fc` | Write | 384 row-major cells with a stride of 24, word-aligned |

The socket word uses personality nibbles `[7:4]` and `[11:8]`, and row flips
`[12]` and `[13]`. All other bits must be zero. Personalities are 0 (released),
1 (OLEDrgb), 2 (VGA J1), 3 (VGA J2). A validated
write commits at the beginning of
vertical blank; its reply waits for the pixel-domain acknowledgement. The
held word and synchronized toggles prevent a partial pin configuration.

Status values are implementation choices: 0 success, 1 unsupported version,
2 unsupported opcode, 3 bad CRC, 4 unknown address, 5 bad payload length and
6 unsupported value. No malformed request changes sockets or text cells.

The regression exercises the real 2 Mbaud UART, reply CRCs through an
independent Python implementation, legacy layer traffic with no unprompted frames, invalid
requests and acknowledgement latency. An independent PMOD simulation checks
row mapping and flips, released pins, frame-boundary commits, blanking and
sync counts across a full 720p frame. The link regression checks the layer
commands (enable, cursor, cell runs, row wrap). The compositor regression
checks that the layer covers the raster only with the overlay up and the
output is black otherwise. A streamed compositor regression independently
checks the physical VGA color/sync pins over a full raster and all HDMI visible
pixels against a reference glyph image, including line and frame wraps.

## OLED terminal

The OLED is a separate shell display filling its 96×64 pixels in one of three
font sets, chosen by register `0x118` (desktop ABI 1.2):

| Set | Value | Cell | Grid | Glyphs |
|---|---|---|---|---|
| Graphics | 0 | 4×4 | 24×16 | original TinyTang glyphs (`oled_font.vh`) |
| Standard | 1 | 8×8 | 12×8 | the desktop's own 8×8 font (`oled_font_standard.vh`, from `font.vh`) |
| Compact | 2 | 6×8 | 16×8 | original TinyTang glyphs (`oled_font_compact.vh`) |

The core resets to Graphics, which is the only set an ABI 1.1 firmware knows,
and a value above 2 is refused with status 6. The cell store keeps a stride of
24 in every set, so a cell's address is `0x200 + 4 × (row × 24 + column)`
whichever grid is showing, and cells outside the grid are kept but not drawn.
A set change is applied with the cursor and socket word at vertical blank.
ASCII 32–126 is supported in every set; unsupported printable Unicode uses
one boxed fallback cell (DEL). The display conversion does
not change command input or file contents. Each 16-bit cell stores character
7:0, foreground 11:8 and background 15:12 in a 16-colour palette. Upper bits
of a write32 value must be zero. Block writes validate the whole request
before applying 1–64 consecutive cells; a rejected block applies none.
Cursor updates use the same vertical-blank handshake as socket declarations.
The panel clocks SPI at a 161.6 ns period and latches both pixel bytes before
sending them. Its frame signature covers the serialized pixels.

The BL616 app **OLED Terminal** owns an independent TinyDesk Shell session
and compact terminal emulator, sized to the font set's grid, Standard by
default at every boot; `oledterm font standard|compact|graphics` changes it
at once, keeping the text and the cursor's line. The link poller sends the
set to `0x118` before any cell, and on a desktop core older than ABI 1.2 it
holds the terminal at Graphics, the only set that core can draw. The graphics
demos (`tools/make_oled_demo.py`) select Graphics while they play and return
to Standard.
Keyboard and paste input go there only when its window has focus; output
continues while unfocused or closed. The shell task and script workers use
a task-local terminal route, separate from TinyConsole and the existing
Terminal app. Starting it allocates a session, a 16 KB task stack and the
emulator, input ring and link buffers, about 31 KB of heap on hardware and
nothing before it starts; it is held until reboot, which leaves too little
for a desktop script launch alongside it. `crash tasks` lists every task's
stack margin. The background desktop poll task sends changed cell blocks and
cursor state. After each core load it asks the core's ID once with the
legacy command and sends extended frames only to this desktop core, so a
game core is never sent them. It pauses for core replacement and checks
desktop identity, ABI and OLED configuration once a second. Starting
another desktop, or `tangput`, from the OLED shell is refused, and a second
`tangload` is refused while one is programming. Both root sessions share
`~/.tdsh_history`, so commands entered in both at the same instant can lose
a history line.

Diagnostic shell commands are `oledterm start`, `oledterm status`,
`oledterm font [standard|compact|graphics]` and `oledterm run "echo hello"`.
The last submits a command to the independent session. Run
`tools/make_oled_font.py` to regenerate the three font files and a native
96×64 specimen of each set under `build/oled-terminal/`. Additional regressions:

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
TinyDesk's window manager. The card's `i2s2_oled` layout gives this core the
OLED in PMOD0 with PMOD1 released, `0xc0 = 0x0010`.
