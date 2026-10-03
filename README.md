# TinyTang

A Tang Console 138K with a real desktop on it: **TinyDesk Shell** running on the
board's BL616, drawn in your terminal, with the SD card as its filesystem and
the FPGA booting cores on command.

The board does the work; the computer only displays it. There is no terminal
emulator in the FPGA and no display hardware on the microcontroller — the
BL616 renders ANSI and streams it over USB CDC, and any ANSI/UTF-8 terminal
shows it.

This is a from-scratch firmware. It does not inherit from TangCore; the only
thing taken from the previous firmware is board knowledge, and every such fact
is called out in the source where it is used.

## What works

- **TinyDesk Shell** on the BL616, over the USB CDC. Prompt:
  `root@tinytang:~#`. The full 25-command shell, redirection, scripts.
- **The SD card** mounted read/write over FatFS, so `ls`, `cat`, `echo >`,
  `rm`, `mkdir`, `cp`, `mv` all work on the card.
- **`tangload <path>`** — program the FPGA with a core image from the SD over
  the board's JTAG. A core loads in a second or two.
- **`fpga`** — ask the running core for its ID over the BL616's UART link. The
  link's liveness proof, and the only way to tell a live core from a dead one
  without watching the HDMI output.
- **`nesload <path>`** — stream an iNES ROM into the running core and start it.
  Castlevania boots off the card in about a second.
- **`tangflash <path>` / `tangput <size> <path>`** — reflash the BL616 itself
  from a file on the SD, with no BOOT button.

## Quick start

```bash
# Build (needs the Bouffalo SDK and the RISC-V toolchain; see below)
make CHIP=bl616 BOARD=bl616dk

# Install over the ROM bootloader (BOOT button, once):
#   put the board in ROM-bootloader mode, then
make flash CHIP=bl616 BOARD=bl616dk COMX=/dev/ttyACM0

# From then on, reflash over USB with no BOOT button:
tools/tinytang_flash.py build/build_out/tinytang_bl616.bin
#   ... then power-cycle the board to run the new image

# Talk to it
screen /dev/ttyACM0 115200
```

In the shell:

```
help                       # the command set
platform                   # bl616/freertos
tang                       # platform and SD status
ls /                       # the SD card, seen as the shell's root
tangload /cores/console138k/pmodtang.bin    # boot the FPGA with that core
```

Booting a cartridge takes three commands, in this order, because the FPGA
loses its configuration when the board is power-cycled:

```
tangload /cores/console138k/nestang.bin     # put the core in
fpga                                        # confirm it answers (expect: core 1)
nesload /roms/castlevania.nes               # stream the ROM; the core starts
```

The `console138k` images are the right ones for this board, and that is worth
stating because the nestang tree makes it easy to doubt. The Tang Console's
FPGA is a GW5AST-138 — its JTAG IDCODE is `0x0001081b` — and the `nestang.bin`
on the card is nand2mario's console138k build: 4,593,044 bytes, byte-identical
to the artifact his tree's `build.tcl console138k ds2` target produces, whose
place-and-route report names `GW5AST-LV138PG484AC1/I0`. The console60k image is
2,321,194 bytes and does not belong here. Note that the 138K project is
generated from the device name by `build.tcl` rather than checked in as a
`.gprj`, so grepping for a project file will not find it.

All three steps are a script on the card, `scripts/boot-cart.tdsh`, which runs
them with a check between each:

```
tdsh run /scripts/boot-cart.tdsh
```

Put it there from this repository with
`tools/tinytang_put.py scripts/boot-cart.tdsh /scripts/boot-cart.tdsh`. Set
`CORE` or `ROM` in the session first to use another pair — `tdsh run` passes no
arguments to a script, but the script inherits the session's variables:

```
ROM=/roms/other.nes; tdsh run /scripts/boot-cart.tdsh
```

The check that matters is the probe between the two loads. Streaming a ROM
into a core that is not listening is a second of UART traffic that disappears
with no error anywhere, and the symptom is a black screen that looks like a
video fault; the script stops with a non-zero status instead.

## How it is put together

- `main.cpp` — board bring-up, then the shell task.
- `ports/bl616/` — everything board-specific, which is what TinyDesk Shell's
  own porting document asks a target to provide:
  - `tdsh_platform_bl616.c` — the platform API (timer, sleep, entropy,
    worker tasks) and the Tang commands.
  - `usb_cdc_bl616.c` — the console: a USB CDC-ACM device, and the terminal.
  - `tdsh_fs_bl616.c` — the standard C file calls the shell uses, over FatFS,
    plus the console's stdin/stdout.
  - `tdsh_console_stdio_bl616.c`, `tdsh_stdio_redirect.h` — the shell's
    `printf` family, redirected to the console.
  - `tdsh_tang_flash.c` — `tangput` and `tangflash`.
  - `tang_fpga_uart.c` — the UART link to the core, and `nesload` over it.
  - `tang_jtag_programmer.c`, `tang_jtag_glue.h` — the JTAG programmer.
- `third_party/tinydesk-shell` — the shell itself, as a submodule, compiled
  unchanged.
- `scripts/` — `.tdsh` scripts for the card, run there with `tdsh run`:
  - `boot-cart.tdsh` — program the FPGA, probe the core, stream a ROM.

## Board facts that shape the firmware

These were each measured on hardware, and each one cost a debugging session:

1. **`CONFIG_CHERRYUSB_HOST` is required.** With FreeRTOS enabled and only the
   device stack built, the CDC never enumerates on this board. Enabling the
   host stack (which brings in the FreeRTOS OSAL) is what fixes it. The working
   firmware on this board enables it too.
2. **The SDK's newlib port cannot be used.** `CONFIG_NEWLIB` stops the USB
   device enumerating, with or without its FatFS file layer. The shell's stdio
   is therefore implemented here, over FatFS.
3. **The SD card is gated behind GPIO 16, held high.** The SDK's dev-board SD
   init does not touch it, and without it `f_mount` returns `FR_NOT_READY`.
4. **The FPGA's JTAG pins are GPIO 0/1/2/3** (TMS/TCK/TDO/TDI), driven for the
   vendored programmer's fast bit-bang path.
5. **The application lives at flash `0x40000`.** The vendor loader below it is
   never touched; `tangflash` stages an image at `0x100000`, verifies it, and
   only then commits it from TCM.
6. **A soft reset lands in the vendor loader**, so a `tangflash` needs a
   power-cycle afterwards to run the new image. That is the same behaviour the
   previous firmware documented for its own updater.
7. **The core's UART is the BL616's UART1: TX GPIO 28, RX GPIO 27, 2 Mbaud.**
   Frames in both directions are `0xAA len_hi len_lo type payload[len-1]` —
   the length is big-endian and counts the type byte, and a length high byte
   of 8 or more drops the core's receiver back to hunting for the next magic
   byte. The RX side must be drained from an interrupt: the BL616's 32-byte
   RX FIFO cannot hold a 2 Mbaud burst between polls, and without that the
   core's replies are silently never seen.
8. **The OSD is a whole-picture layer, not a text mask, and it starts ON.**
   `nes2hdmi.sv` selects it with `if (overlay) rgb <= overlay_color`, so a
   cartridge loaded while it is on plays its music behind a black screen
   carrying only the core's logo — which looks exactly like a video fault and
   is not one. The loader must clear it (command `0x08`, payload 0) before
   releasing the core. This cost a session to find, with the game running
   audibly the whole time.

## Diagnostics

`proj.min.conf`, `proj.nonewlib.conf` and `proj.rtos.conf` are the
configurations used to isolate fact 1 and fact 2 above; `ref/` holds the SDK's
own device example, built inside this project with a chosen config. They are
selected by environment variable (`TINYTANG_MIN`, `TINYTANG_NONEWLIB`,
`TINYTANG_RTOS`, `TINYTANG_REF`, `TINYTANG_USB_ONLY`, `TINYTANG_NO_FS`,
`TINYTANG_NO_SHELL`) and are kept as the record of that bisection.

## Building requirements

- **Bouffalo SDK** at `BL_SDK_BASE` (defaults to
  `~/.cache/tangcore-dev/sdk`), with `find_package(bouffalo_sdk)` support.
- **RISC-V toolchain** (`riscv64-unknown-elf-*`), default
  `~/.cache/tangcore-dev/toolchain/bin`.
- Python with `pyserial` for the tools in `tools/`:
  `tinytang_flash.py` (reflash over USB), `tinytang_put.py` (put a file on the
  card), `tinytang_run.py` (run a shell command and stream its output).

## Third-party

- **TinyDesk Shell** — MIT — as the `third_party/tinydesk-shell` submodule.
- **Gowin JTAG programmer** — Apache-2.0 — `ports/bl616/tang_jtag_programmer.c`
  is nand2mario's GPIO JTAG programmer for Gowin GW5A/GW2A, taken from
  Tang-Control, used unmodified apart from its include list. Its glue in
  `tang_jtag_glue.h` documents what was supplied for it.
- **The core-side UART protocol** is nand2mario's, read from the nestang
  tree's `src/iosys/iosys_bl616.v`, which documents the frame format and the
  command set in comments above its receiver. `ports/bl616/tang_fpga_uart.c`
  implements the BL616 end of it; the timings and chunk size follow from the
  core's own receiver and Tang-Control's NES loader.
- **Bouffalo SDK** — the vendor SDK, for the chip support and FatFS.
