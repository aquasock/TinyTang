# TinyTang

A Tang Console 138K with a real desktop on it: **TinyDesk Shell** and the
**TinyDesk** desktop running on the board's BL616, shown on the board's own
HDMI output, typed at from a keyboard plugged into the board, with the SD card
as the filesystem and the FPGA booting cores on command. F12 switches the
screen between TinyDesk and whatever core is running, the way MiSTer's menu
key does.

The BL616 does the computing and the FPGA does the display. TinyDesk renders
ANSI as it always does; on the board that stream goes through a terminal
emulator into an 80x45 text layer the FPGA composites over its video, so the
HDMI output shows the desktop. The same stream also goes down the USB CDC, so a
terminal on a computer can show and drive the same session.

This is a from-scratch firmware. It does not inherit from TangCore; the only
thing taken from the previous firmware is board knowledge, and every such fact
is called out in the source where it is used.

## What it does

- **Boots to a console on the HDMI output.** `/scripts/boot.tdsh` loads the
  menu core, turns the desktop layer on, and stops at the shell prompt
  (`root@tinytang:~#`) drawn on the screen. The board has the shell's built-in
  commands, redirection and scripts, plus the Tang commands below.
- **`desktop`** runs TinyDesk on that console: overlapping draggable text-mode
  windows, a taskbar and a start menu, with Terminal, Files, Editor, System
  Monitor, Task Manager, Log, Settings, Counter, Phosphor and About. Quitting returns to
  the prompt. The network apps (Network, MQTT, Modbus, OTA) are left out because
  they are built on POSIX sockets and mbedTLS.
- **The Terminal window runs the shell.** It is the same session as the
  console, reached through a bridge over two ring buffers, so
  `tdsh run /scripts/boot-cart.tdsh` typed into a desktop window boots a core
  and starts a cartridge without leaving the desktop. One user and one session
  by design.
- **F12 switches between TinyDesk and the core.** It flips the overlay: the
  core keeps running underneath, and while TinyDesk is up the controllers are
  gated away from the game. It works at the console, in the desktop and during
  a game. L on controller 1 does the same. On the menu core the core's side is
  an empty frame; on the NES core it is the game.
- **A keyboard.** A Bluetooth LE keyboard connected with `blekbd` types at
  the console and in the desktop; the BL616 receives it directly, so it works
  on any core that carries the desktop layer. A Keychron K2 HE running this
  project's QMK patches (`third_party/patches/qmk/`) also plugs into either
  front USB-A port and does the same. It is not USB: the keyboard drives its
  own D+ line as a UART and the core receives it (see *Keyboard* below). Both
  can be used at once.
- **A pointer in the desktop.** A Bluetooth LE mouse connected with
  `blemouse`, the D-pad on controller 1, or the arrows with left-alt held,
  moves it; the mouse's buttons, A or Enter are the left button, B or Esc the
  right, and holding the left button while moving drags. The mouse's middle
  button and wheel are passed on too. All of them drive the same pointer. The pointer exists only while
  the desktop runs: at the bare console nothing is drawn and nothing moves.
- **The SD card** mounted read/write over FatFS, so `ls`, `cat`, `echo >`,
  `rm`, `mkdir`, `cp`, `mv` all work on the card. FatFS is built reentrant
  (`FF_FS_REENTRANT`), so the shell, the desktop and the playback task can use
  the card at the same time.
- **`tangload <path>`** programs the FPGA with a core image from the SD over
  the board's JTAG. A core loads in a second or two.
- **`fpga`** asks the running core for its ID over the BL616's UART link: the
  link's liveness proof. This project's cores answer 1; the core the FPGA comes
  up with on its own answers 0.
- **`nesload <path>`** streams an iNES ROM into the NES core and starts it.
- **`phosphor caps | peek | poke | play | pause | resume | status | stop | stats`**
  drives a loaded
  Tang-Phosphor core over its extended protocol (register access on frame
  type `0x10`, files streamed on `0x11` at 5 Mbaud). `phosphor play <file>`
  plays any of the twelve formats Phosphor supports -- WAV, FLAC, MP2, MP3,
  Ogg Vorbis, Opus, AAC, ALAC, WavPack, WMA, AC-3 and TTA -- by loading the
  resident Rockbox player `/ae350/resident.tpi` onto the core's AE350 and
  streaming the file to it. A playback task owns the track, so the shell is
  free: `nowait` returns at once and the track plays in the background, while
  plain `play` waits for it and Ctrl-C stops the track itself, not just the
  wait. `phosphor status` shows the state (idle, loading, playing, ended,
  stopped, failed), the file, elapsed time, samples, rate and underruns;
  `phosphor pause` holds the track where it is (even while it is still
  loading) and `phosphor resume` carries on, without the pause counting as a
  stall; `phosphor stop` silences the track at once and restarts the AE350. A new
  `play` replaces the current track, and `tangload` stops it before it
  reprograms the FPGA. The core,
  `/cores/console138k/phosphortang.bin`, carries the desktop layer, the
  keyboard link and F12 the way the NES core does, so it runs under TinyDesk
  with the layer left on. The merged core has no WAV or FLAC decoder in the
  fabric: its FPGA player is a raw-PCM sink fed by the AE350, so the AE350 is
  the only way to play a file. `scripts/phosphor.tdsh` loads the core with the
  desktop up, probes it, hands it the screen and starts `/music/test.mp3` (or
  `$FILE`) without waiting; F12 then switches between the player and TinyDesk,
  during the track and after it.
- **The Phosphor app** in the desktop's start menu is a player for `/music`:
  a sorted list of the playable files, the title and status of the current
  track, a progress bar (WAV and FLAC, whose length is read from the file's
  header; the others show elapsed time only), Prev, Play, Pause/Resume, Stop
  and Next, and an auto-advance checkbox that plays the folder through. It
  drives the same playback task as the `phosphor` command, so closing the
  window leaves the track playing and reopening it picks the track back up.
  Without the Phosphor core loaded it says so and its buttons are disabled.
- **`osd desk on | off | status`** controls the desktop layer directly. `on`
  shows the console on the layer, `off` stops it and hands the screen back to
  the core, `status` reports cells and rows sent and any refused.
- **`osd on | off | clear | at | menu`** draw on the NES core's own 32x28 text
  page, the one nand2mario's menu uses. The menu core does not build that page.
- **`blescan [seconds]`** starts the BL616's own Bluetooth LE radio and lists
  the devices advertising nearby, strongest first, with address, address type,
  signal (dBm), kind (`kbd` or `mouse` from the advertised appearance, `hid`
  for the HID service alone) and name. The radio starts at boot when a device
  is paired, otherwise on the first Bluetooth command, and stays up until
  reset.
- **`blekbd pair [name]`** pairs a Bluetooth LE keyboard (HID over GATT) as an
  input: it listens for 8 s, picks the strongest device advertising as a
  keyboard (or whose name contains `name`), pairs with Just Works and switches
  it to boot protocol -- the same 8-byte report the wired keyboard link
  carries. The keyboard types at the console and in the desktop exactly as the
  wired one does, with F12 and left-alt pointer mode (see *A keyboard* above).
  **The pairing is saved to the card** (`/sd/ble/bonds.bin`), so after a
  reset, or when the keyboard wanders out of range and back, it reconnects by
  itself as soon as it is woken; pairing mode is needed only the first time.
  `blekbd off` and `blekbd on` stop and resume reconnecting, `blekbd forget`
  deletes the pairing, `blekbd watch [seconds]` prints the reports,
  `blekbd <address> [pub|rand] [seconds]` pairs a given address, and `blekbd`
  alone reports the state. Proven with a Logitech K950 (from the MK955 set) in
  Bluetooth mode. Only Bluetooth LE devices work: the SDK has no Bluetooth
  Classic HID host. The keys are stored unencrypted on the card.
- **`blemouse pair [name]`** does the same for a Bluetooth LE mouse, on its own
  connection beside the keyboard: boot protocol, whose report is buttons, X, Y
  and (if the mouse adds it) the wheel, and the mouse then drives the
  desktop's pointer. It has the same `off`, `on`, `forget`, `watch`, address
  and status forms. Proven with a Logitech M750. Pair one device at a time: a
  second is refused until the first is ready.
- **`ble`** reports the radio, the heap (free now, and what the stack took when
  it started), the pairings file and both devices.
- **`usbstat` / `usbwatch` / `usbrole`** read the USB OTG block and switch the
  OTG connector's role. They exist to establish facts 11 and 12 below.
- **`tangflash <path>` / `tangput <size> <path>`** reflash the BL616 itself
  from a file on the SD, and put a file on the card over the console, with no
  BOOT button.
- **`platform`** reports the firmware's build identity, as does the startup
  banner: the commit it was built from, and a hash of any uncommitted changes
  (`318b23c`, or `318b23c-dirty.4f1a9c2`). It is regenerated on every build by
  `cmake/tinytang_build_id.cmake`, so an install can be confirmed from the host.

## Quick start

```bash
# Build the BL616 firmware (needs the Bouffalo SDK and the RISC-V toolchain)
make CHIP=bl616 BOARD=bl616dk

# Install over the ROM bootloader (BOOT button, once):
make flash CHIP=bl616 BOARD=bl616dk COMX=/dev/ttyACM0

# From then on, reflash over USB with no BOOT button:
tools/tinytang_flash.py build/build_out/tinytang_bl616.bin
#   ... then power-cycle the board (see fact 6), and confirm with `platform`:
#   it prints the build identity, which must match build/tinytang/tinytang_build_id.h

# Build the two FPGA cores (needs Gowin EDA 1.9.11.03 and a nestang checkout
# at ../tangcore/nestang, or NESTANG_DIR)
tools/build_nestang_core.sh     # the NES core, with the desktop layer
tools/build_menu_core.sh        # the menu core: the same design, no NES machine

# Put the cores and the scripts on the card (at a plain shell prompt -- fact 15)
tools/tinytang_put.py <nestang>/impl/pnr/nestang_console138k_ds2.bin      /cores/console138k/nestang-desk.bin
tools/tinytang_put.py <nestang>/impl/pnr/nestang_console138k_ds2_menu.bin /cores/console138k/nestang-menu.bin
tools/tinytang_put.py scripts/boot.tdsh      /scripts/boot.tdsh
tools/tinytang_put.py scripts/boot-cart.tdsh /scripts/boot-cart.tdsh

# Talk to it from a computer as well
screen /dev/ttyACM0 115200
```

Power-cycle the board and it comes up at the console on the HDMI output. Type
`desktop` for TinyDesk.

### Cartridges

Booting a cartridge is three commands, in this order:

```
tangload /cores/console138k/nestang-desk.bin  # put the NES core in
fpga                                          # confirm it answers (expect: core 1)
nesload /roms/castlevania.nes                 # stream the ROM; the core starts
```

`scripts/boot-cart.tdsh` runs all three with a check between each, from the
console or from the desktop's Terminal:

```
tdsh run /scripts/boot-cart.tdsh
```

It defaults to the NES core and `/roms/castlevania.nes`. Set `CORE` or `ROM` in
the session first to use another pair; `tdsh run` passes no arguments, but the
script inherits the session's variables:

```
ROM=/roms/other.nes; tdsh run /scripts/boot-cart.tdsh
```

The check that matters is the probe between the two loads. A ROM streamed into
a core that is not listening disappears with no error anywhere, and the
symptom is a black screen that looks like a video fault; the script stops with
a non-zero status instead. Once the game is running, F12 brings TinyDesk back
over it and F12 again returns to the game. `scripts/castlevania.tdsh` is the
same cycle under a Castlevania-specific name.

The NES core here is the patched one, `nestang-desk.bin`. nand2mario's stock
`nestang.bin` carries neither the desktop layer nor the keyboard link, so with
it loaded the keyboard and TinyDesk are gone until the next boot.

### What the board boots

`/scripts/boot.tdsh` runs at power-up. It loads the core named by its `CORE`
line -- the menu core, `nestang-menu.bin` -- probes it, enables the desktop
layer and stops at the prompt. Pointing `CORE` at `nestang-desk.bin` boots the
NES core instead. Removing or renaming the file boots to a plain prompt with no
core loaded; the FPGA then stays on the core it configured itself with, and
the screen shows a TangCore splash (fact 14).

## The FPGA cores

Both cores are nand2mario's nestang, carried as patches against upstream rather
than as a fork. The checkout stays pristine at commit `c2450818`, and
`scripts/apply-nestang-patches.sh` applies `third_party/patches/0001` through
`0007` before a build:

| Patch | What it adds |
|---|---|
| 0001 | Gates the physical pads away from the game while the overlay is up |
| 0002 | `textdisp_wide`: the 80x45 cell layer, per-cell 15-bit colour |
| 0003 | Wires the layer into iosys (commands `0x13`-`0x15`) and the HDMI mixer |
| 0004 | The keyboard link receiver on both front USB D+ pins |
| 0005 | The keyboard report sent up to the BL616 as response `0x08` |
| 0006 | Left-alt pointer mode: arrows, Enter and Esc become pad bits |
| 0007 | Aligns the layer with the raster (fact 16) |

`third_party/patches/menu/0001-menu-core.patch` applies on top to make the
menu core: the NES machine, its loader and SDRAM are not instantiated, and the
legacy text page is removed. Both builds are reproducible byte for byte, and a
fresh clone plus the series reproduces the working tree; the patches'
`README.md` files have the detail.

## Keyboard

The keyboard is a Keychron K2 HE built from Keychron's QMK fork with
`third_party/patches/qmk/` applied by `scripts/apply-qmk-tang-patches.sh`:

```
make keychron/k2_he/ansi:tang
```

That firmware is **not a USB keyboard**. It bit-bangs `A5 LEN payload SUM`
frames carrying the 8-byte HID boot report on PA12 (D+) at 281250 baud, which
the core's `keylink_rx` receives on either front port. The core relays the
report to the BL616, which turns it into the byte stream an ANSI terminal
sends: letters and punctuation as themselves, Shift applied, Ctrl as
`0x01`-`0x1A`, and the navigation keys as CSI sequences, with auto-repeat
generated on the board. Function keys are not translated yet, so the
desktop's F10 (start menu), F6 (switch windows) and F11 (full screen) work from
a computer's terminal but not from the board's keyboard. F12 is reserved for
switching the screen and is never typed. To use the keyboard on a PC again,
flash the stock firmware back, or hold the BOOTMAGIC key at plug-in for the
STM32's own DFU bootloader.

## How it is put together

- `main.cpp` — board bring-up, then the shell task.
- `ports/bl616/` — everything board-specific:
  - `tdsh_platform_bl616.c` — the platform API (timer, sleep, entropy, worker
    tasks), the Tang commands, and the console's input sources: the desktop's
    Terminal when it owns the console, otherwise the layer's input ring and
    then the CDC.
  - `usb_cdc_bl616.c` — the console over USB CDC-ACM.
  - `tdsh_fs_bl616.c` — the standard C file calls the shell uses, over FatFS,
    plus the console's stdin/stdout.
  - `tdsh_console_stdio_bl616.c`, `tdsh_stdio_redirect.h` — the shell's
    `printf` family, redirected to the console.
  - `tdsh_tang_flash.c` — `tangput`, `tangflash` and `tangload`.
  - `tang_fpga_link.h`, `tang_fpga_uart.c`, `fpga_frames.c` — the UART link to
    the core: the frame transport, the cache of unprompted frames (joypad,
    keyboard), and `fpga` and `nesload` over it.
  - `tang_osd_desk.c` — the desktop layer: a terminal emulator fed by the
    console tap, a shadow of what the core holds, the diff that sends cells,
    the pointer, and the F12 / L switch.
  - `tang_key.c` — keyboard reports to terminal input bytes, with auto-repeat.
  - `tang_pad.c` — the controller as the desktop's pointer, as xterm SGR mouse
    reports.
  - `tang_osd.c` — the NES core's 32x28 text page, and `osd`.
  - `td_desktop_bl616.c` — the TinyDesk port and the `desktop` command.
  - `td_bridge_bl616.c` — the shell inside TinyDesk's Terminal window.
  - `phosphor/` — the Tang-Phosphor host side, ported from Tang-Control: the
    extended-protocol transport, the resident AE350 player loader, the
    background playback task (`phosphor_player.cpp`, with its end-of-track rule
    in `phosphor_track.h`), the `phosphor` command, and the desktop's
    Phosphor app (`td_phosphor_app.cpp`, with its header parsing and time
    helpers in `phosphor_media.h`).
  - `tang_ble.c` — Bluetooth LE: `blescan`, `ble`, and `blekbd` and
    `blemouse`, the HID-over-GATT client with one keyboard slot and one mouse
    slot, which saves pairings to the card and reconnects them through the
    controller's whitelist; `tang_ble_bonds.c` is the pairing file's format.
    `tang_osd_desk.c` types the keyboard's boot report like the wired link's,
    with left-alt pointer mode applied by `tang_key_pointer()`, and hands the
    mouse's movement, buttons and wheel to `tang_pad_pointer()`.
  - `tang_usbstat.c`, `tang_usb_role.c` — `usbstat`, `usbwatch` and `usbrole`.
  - `tang_jtag_programmer.c`, `tang_jtag_glue.h` — the JTAG programmer.
- `cmake/tinytang_build_id.cmake` — writes the build identity header on every
  build.
- `third_party/tinydesk-shell`, `third_party/tinydesk` — the shell and the
  desktop, as submodules.
- `third_party/patches/` — the nestang patch series, the menu-core patch and
  the QMK keyboard patches.
- `scripts/` — `.tdsh` scripts for the card: `boot.tdsh` (power-up),
  `boot-cart.tdsh` (core, probe, ROM), `castlevania.tdsh` and `phosphor.tdsh`
  (start one track on the Phosphor core under TinyDesk); plus the two
  patch appliers, which run on the host.
- `tools/` — host tools:
  - `make_test_wav.py` — writes a deterministic 4-second test tone for the
    Phosphor player.
  - `make_codec_corpus.sh`, `phosphor_format_sweep.py` — regenerate
    Tang-Phosphor's twelve-format test corpus with ffmpeg, then play it on the
    board and check every file's samples, underruns and rate against the
    figures Tang-Phosphor qualified (its entry 43).
  - `tinytang_put.py`, `tinytang_run.py`, `tinytang_flash.py` — put a file on
    the card, run a command, reflash the BL616. `tinytang_put.py` refuses to
    send unless the console is at a shell prompt (fact 15).
  - `build_nestang_core.sh`, `build_menu_core.sh` — apply the patches and build
    a core with Gowin.
  - `test_textdisp_wide.sh` — simulates the layer against a free-running
    1650x750 raster and checks every visible pixel (Verilator).
  - `tests/` — host tests for the frame cache, the key translation, the pad,
    the desk layer, the iosys/compositor path, and the Phosphor transport's
    encoders.
  - `check_core_log.py` — checks `.ai/core-log.md` against its format.

## Board facts that shape the firmware

These were each established on hardware, and each one cost a debugging
session. `.ai/core-reference.md` holds the full records and their sources.

1. **`CONFIG_CHERRYUSB_HOST` is required.** With FreeRTOS enabled and only the
   device stack built, the CDC never enumerates on this board. Enabling the
   host stack (which brings in the FreeRTOS OSAL) is what fixes it.
2. **The SDK's newlib port cannot be used.** `CONFIG_NEWLIB` stops the USB
   device enumerating, with or without its FatFS file layer. The shell's stdio
   is therefore implemented here, over FatFS.
3. **The SD card is gated behind GPIO 16, held high.** The SDK's dev-board SD
   init does not touch it, and without it `f_mount` returns `FR_NOT_READY`.
4. **The FPGA's JTAG pins are GPIO 0/1/2/3** (TMS/TCK/TDO/TDI), driven for the
   vendored programmer's fast bit-bang path.
5. **The application lives at flash `0x40000`, up to 896 KB (`0xE0000`).**
   The vendor loader below it and the vendor data record at `0x200000` are
   never touched; `tangflash` stages an image at `0x120000`, verifies it, and
   only then commits it from TCM.
6. **A `tangflash` normally needs a power cycle.** Its soft reset usually lands
   in the vendor loader, and the board shows up as an FT2232 until it is
   power-cycled. Once, on 2026-10-04, it came straight back to the console
   instead; that is not explained, so power-cycle after every install and
   confirm the build identity `platform` reports.
7. **The core's UART is the BL616's UART1: TX GPIO 28, RX GPIO 27, 2 Mbaud.**
   Frames in both directions are `0xAA len_hi len_lo type payload[len-1]`;
   the length is big-endian and counts the type byte, and a length high byte
   of 8 or more drops the core's receiver back to hunting for the next magic
   byte. The RX side must be drained from an interrupt: the BL616's 32-byte
   RX FIFO cannot hold a 2 Mbaud burst between polls.
8. **The overlay selects the whole picture, and a core comes out of reset with
   it on.** `nes2hdmi.sv` shows the overlay instead of the game whenever it is
   asserted, so a cartridge started with it on plays its music behind a black
   screen. `nesload` clears it (command `0x08`, payload 0) before releasing the
   core, and that byte is also what F12 flips.
9. **The NES core's own text page is 32 columns by 28 rows of 8x8 cells**, and
   its store is never initialised, so a freshly loaded core shows stray glyphs
   until something clears it; `osd clear` in the scripts blanks it. Column 0
   draws in the cursor colour, and the core's logo sits in rows 25-26. The menu
   core has no such page.
10. **The shell's terminal protocol is small:** `\r`, `\n`, `\033[2K`,
    `\033[2J`, `\033[H`, `\033[<n>C`, `\033[<n>D` and SGR colours, and nothing
    else. Absolute cursor positioning on the console therefore means the
    desktop is running, not the shell -- the check to make before sending the
    board anything raw.
11. **The USB OTG block reports no role signal we can use.** Across a cable
    swap only `OTG_CSR`'s speed field moved. CherryUSB's
    `USBD_EVENT_CONFIGURED` and `USBD_EVENT_DISCONNECTED` are the usable
    host-present signal, exposed as `tdsh_bl616_console_connected()`.
12. **The OTG connector cannot host: it does not source VBUS**, in either
    `DRVBUS_POL` polarity. The connector is device-only, which is why the
    keyboard reaches the board through the FPGA rather than over USB.
13. **Ctrl+S from a computer's terminal looks like a hang.** With software flow
    control on, the terminal swallows it as XOFF and stops the display; Ctrl+Q
    brings it back. Turn flow control off (PuTTY: Serial, Flow control = None).
    The board's own keyboard is not affected.
14. **The FPGA is not empty at power-up.** With no `tangload` it is already
    running a stock TangCore core that answers core ID 0 and shows a TangCore
    splash. `boot.tdsh` replaces it within seconds, so it is only seen when the
    boot script is missing.
15. **The console's input is exclusive.** `tangput` and the desktop read the
    same CDC byte stream, so a file sent while the desktop is running is typed
    into whichever window has focus. That once filled the card's root with
    junk entries and damaged its FAT. Send files only at a plain shell prompt;
    `tinytang_put.py` checks for that before sending.
16. **A pipelined video layer must address the raster ahead of itself.** The
    HDMI counters name the pixel sent on the next clock, and the stock colour
    path is one register deep. The desktop layer is four deeper, so it looks
    up each pixel four pixels ahead, wrapping at the frame edges. Before patch
    0007 it did not, and the top-left character's left column was painted down
    the whole left edge of the screen.
17. **The board has no Bluetooth antenna, only a jack for one.** The BL616's
    antenna pin runs to U35, a U.FL socket marked ANT on the dock's underside
    beside the upper USB-A port. With nothing fitted the radio still works at
    arm's length, at about -76 to -94 dBm, but a connection attempt sometimes
    fails (HCI 0x3E, connection failed to be established) and `blekbd` has to
    be run again.

## Diagnostics

`proj.min.conf`, `proj.nonewlib.conf` and `proj.rtos.conf` are the
configurations used to isolate fact 1 and fact 2 above; `ref/` holds the SDK's
own device example, built inside this project with a chosen config. They are
selected by environment variable (`TINYTANG_MIN`, `TINYTANG_NONEWLIB`,
`TINYTANG_RTOS`, `TINYTANG_REF`, `TINYTANG_USB_ONLY`, `TINYTANG_NO_FS`,
`TINYTANG_NO_SHELL`) and are kept as the record of that bisection.

## Building requirements

- **Bouffalo SDK** at `BL_SDK_BASE` (defaults to `~/.cache/tangcore-dev/sdk`),
  with `find_package(bouffalo_sdk)` support.
- **RISC-V toolchain** (`riscv64-unknown-elf-*`), default
  `~/.cache/tangcore-dev/toolchain/bin`.
- **Gowin EDA 1.9.11.03** for the FPGA cores, and a nestang checkout at
  `../tangcore/nestang` (or `NESTANG_DIR`).
- **Verilator** for `tools/test_textdisp_wide.sh`, and a C compiler for
  `tools/tests/`.
- Python with `pyserial` for the tools in `tools/`.
- For the keyboard, Keychron's QMK fork and its toolchain; see
  `third_party/patches/qmk/README.md`.

## Third-party

`THIRD_PARTY.md` has the full list and the licence obligations. In short:

- **TinyDesk Shell** and **TinyDesk** — MIT — as submodules under
  `third_party/`.
- **Gowin JTAG programmer** — Apache-2.0 — `ports/bl616/tang_jtag_programmer.c`
  is nand2mario's GPIO JTAG programmer for Gowin GW5A/GW2A, taken from
  Tang-Control, used unmodified apart from its include list.
- **nestang** — GPL-3.0 — the FPGA cores, modified by the patches in
  `third_party/patches/`; the patch files carry GPL-3.0 lines. The BL616 end of
  the core's UART protocol was written from nestang's documentation of it, and
  no nestang code is copied into the firmware.
- **Keychron QMK** — GPL-2.0 overall, with the Keychron board files and the
  added keymap GPL-3.0-only — the keyboard firmware, modified by the patches in
  `third_party/patches/qmk/` and not vendored here.
- **Bouffalo SDK** — the vendor SDK, for the chip support and FatFS.
