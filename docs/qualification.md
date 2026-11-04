# Qualification matrix

What this project has proven about itself, what it has only tested on a host, and
what nobody has looked at yet. One row per contract; a row is only worth
trusting if the evidence column names something that fails when the promise
breaks.

This is a living document. Phase 0 produced the baseline and the gate below;
Phases 1 and 2 fill in rows. A row with status `untested` is not an accusation —
it is a work item with an owner.

---

## 0. Status vocabulary

- `silicon` — measured on this board, with a date and a procedure. Cites a
  core-log entry or a reference record where the measurement lives.
- `host` — covered by a host regression that fails when the behaviour breaks.
  The test is named. "Passes" is not the claim; "bites" is.
- `partial` — the interesting half is covered and the rest is not. The row says
  which half.
- `untested` — no coverage of any kind.
- `deviation` — the code and the documented contract disagree. Recorded, not
  fixed, because `tinytang.bin` and its RTL are frozen for release.

A row can be `host` and still be unconfirmed on hardware. That is the normal
case and is stated rather than blurred.

---

## 1. The frozen baseline

`tinytang.bin` and the FPGA sources are frozen as of this cycle. Everything
below is measured against these artifacts and this toolchain, and nothing else.

### Firmware — the BL616 image

- **Size**: 682,272 bytes
- **SHA-256**: `ae318f0268c06f62ee401346a52784e024c1f4213d36ae683e2920b92ba5e646`
- **Built from**: `6ff42c9` (this tree, at the commit Phase 0 started from)
- **Build**: `SOURCE_DATE_EPOCH=1793750400 make -j$(nproc)`

The `SOURCE_DATE_EPOCH` is load-bearing. Without it the build is **not**
reproducible: two builds of identical source differ in three bytes, because the
SDK's image tool stamps the app header with a `BLFBVERF` string holding the
compile time. With it pinned, two clean builds are byte-identical — verified by
building twice and comparing with `cmp`. The digest above is therefore the
digest of *a* build at a stated time, not of the source; the reproducible form
is the one to quote.

Nothing in this tree sets `SOURCE_DATE_EPOCH`. It is a documented recipe rather
than a build setting, because changing the Makefile would change the frozen
image. Deciding to bake it in is a release decision, not a Phase 0 one.

### Desktop core — `tinytang.bin`

- **Size**: 4,526,774 bytes
- **SHA-256**: `00d8b48cac8e54b27cdbdacf2adfe8e67331bd3ff95c05e6fd671192f46582cf`
- **`desktop.fs`**: `60dd20032095f6d1b7e14dab9e77ca57973e5d494051d0acf2955dd929e45fda`, 36,239,160 bytes
- **Build**: `tools/build_desktop_core_open.sh`, seed 3
- **Provenance**: `provenance.00d8b48cac8e.txt` beside the artifacts

This rebuild **reproduced the digest on the card bit for bit**. That is the
first time the core of record has been reproduced from its own recorded tuple,
and it retires the open question entry 88 left: the tuple is now reproducible,
even though entry 87 could not reproduce it either.

### Toolchain

- apicula `495661aaf86594232e4eecf02210a7f9464996d2`, clean
- nextpnr `16237b0f1f78a054aceb0b5334b3ff7c6eac91f1`, clean
- chipdb `e74b77e429e9be2e176477fa4d3c9984a1bdee8a17dad5dd3ab80620aefe175b`
- nextpnr binary `2eca603a2a90424305fd72d53a50c3d1b8daeb21e8f0451690cfc4797b85c6c1`
- Xuantie GCC 10.2.0 (V2.6.1 B-20220906) for the firmware
- Yosys 0.69+260 for the core

All three pins agree with `deps.conf`.

---

## 2. The gate

```
tools/tests/run_all.sh
```

Runs every `tools/tests/test_*.sh` plus `check_help_width.py`, one line each,
and exits non-zero if any fails. `--list` enumerates, an argument filters by
substring, `TINYTANG_KEEP_LOGS=1` keeps the per-test logs.

**Current state: 32 of 32 pass** (31 scripts plus the width gate).

---

## 3. Matrix — the desktop core (`tinytang.bin`)

The register endpoint is `fpga/desktop/desktop_regs.sv`; the socket logic is
`desktop_pmod.sv`. ABI is **1.6**.

### Identity and capability

| Status | Item | Evidence |
|---|---|---|
| host | Core answers ID `0x0054` | `test_desktop_core` (UART frames) |
| host | ABI word at `0x04` reads `0x00010006` | `test_desktop_core` |
| host | Capability bitmap at `0x08` reads `0x0000010f` | `test_desktop_core` |
| host | Capabilities at `0x00` reads `0x13` | `test_desktop_core` |
| silicon | Loaded core reports ABI 1.6 | `tangini` on the board, 2026-11-04 |

### Socket word `0xc0`

| Status | Item | Evidence |
|---|---|---|
| host | `0x0000` accepted (both released) | `test_desktop_core` |
| host | Personalities 0/1 per socket accepted | `test_desktop_core` |
| host | VGA pair `0x0230` and `0x0320` accepted | `test_desktop_core` |
| host | Personality 8 as `0x0080` / `0x0800` accepted | `test_desktop_core` |
| host | Personalities 6 and 7 (I2C, i2c_uart) refused, status 6 | `test_desktop_core` |
| host | Personality 5 (Phosphor I2S2) refused, status 6 | `test_desktop_core` |
| host | 8 on both sockets refused | `test_desktop_core` |
| host | A refused word leaves the previous word in place | `test_desktop_core` |
| deviation | Flip bits 12/13 are **not** rejected for personality 8 | read of `supported_word`: the mask is `0xffffc00f`, so bits 4–13 pass unconstrained. PMOD-009 says personality 8 has no flip support. Silent wrong-row drive is possible if a host sets a bit the firmware never sets |
| silicon | `layout = lora` declared and accepted | `tangini`, 2026-11-04 |
| silicon | `layout = i2s2_oled` on the desktop core | core-log entries 22–24 |

### OLED text layer

| Status | Item | Evidence |
|---|---|---|
| host | Grid modes Graphics/Standard/Compact at `0x100` and `0x118` | `test_desktop_core` |
| host | Undefined font mode refused, mode unchanged | `test_desktop_core` |
| host | Cursor register `0x114` bounds | `test_desktop_core` |
| host | Cell writes `0x200`–`0x7fc`, invalid word refused | `test_desktop_core` |
| host | Block write applies all or nothing | `test_desktop_core` |
| host | Frame and signature counters `0x108`/`0x10c` | `test_desktop_core` |
| host | OLED pins, panel, SPI | `test_desktop_oled`, `test_desktop_core` |
| silicon | Panel draws `oled print` text | core-log entry 24 |

### PMOD UART, `0x170`–`0x17c` — personality 8

This window had **no coverage at all** before this cycle: the register bench
instantiated `desktop_link` alone, so `desktop_regs`' `uart_status` input was
dangling and every status read returned zero. The bench now instantiates the
real `pmod_uart` behind the window.

| Status | Item | Evidence |
|---|---|---|
| host | Status `0x178` reads zero at reset | `test_desktop_core` |
| host | Write to read-only `0x174`/`0x178` refused, status 4 | `test_desktop_core` |
| host | `0x170` push, read back last byte pushed | `test_desktop_core` |
| host | `0x17c` bits above 3 refused, value unchanged | `test_desktop_core` |
| host | Hold (bit 3) queues: three pushed, count reads 3 | `test_desktop_core` |
| host | Release drains to zero at 115200 | `test_desktop_core` |
| host | Loopback (bit 2) returns the byte; `0x174` read pops it | `test_desktop_core` |
| host | Flush (bit 1) empties a queued byte | `test_desktop_core` |
| host | NRST (bit 0) reaches the pin, inverted | `test_desktop_core` |
| silicon | Module answers `AT`, `+VER`, `+ADDRESS=2`, `+BAND=915000000`, `+NETWORKID=6`, `+PARAMETER=7,7,1,12`, `+CRFOP=14` | 2026-11-04, this session |
| silicon | `node status`, `node lora`, `node time` round-trip to TinyNode | 2026-11-04, this session |
| untested | Pin-level row/lane mapping for personality 8 | no bench drives socket pins for 8; `test_desktop_pmod` covers VGA and I2C only |

Each assertion above was proven to bite: six planted faults (stale ABI, stale
bitmap, hold ignored, loopback ignored, personality 8 refused, inverted NRST)
were each caught, and the failure text showed the hardware's real value.

### I2C engine and clock monitor

| Status | Item | Evidence |
|---|---|---|
| host | I2C master bit timing, NACK, stall | `test_desktop_core` (`tb_i2c_master`) |
| host | Mux channel select and retry | `test_i2c_mux` |
| host | Socket I2C window `0x160`/`0x164` and status `0x140`–`0x158` | `test_desktop_core` |
| silicon | DS3231 answers at 0x68, clock keeps time | core-log entries 83, 87 |
| silicon | MPR121 answers at 0x5a, touch status reads | core-log entry 83 |
| silicon | Wii adapter at 0x52 | BRD-011, WII-001 |

### Video and compositor

| Status | Item | Evidence |
|---|---|---|
| host | HDMI raster, VGA mirror, no left-edge leak | `test_desktop_video` |
| host | Audio clock regeneration | `test_composite` |
| host | Link framing, ID, no unprompted frames | `test_desktop_link` |
| silicon | 1280x720 HDMI output with desktop | manual, README, photographs |

---

## 4. Matrix — the BL616 platform

### Boot, flash and recovery

| Status | Item | Evidence |
|---|---|---|
| untested | Boot header validation | no host test; FLS-002 is the contract |
| untested | Soft reset lands in vendor loader | recorded, not tested |
| untested | Crash record across watchdog reset | `tang_crash.c` has no test |
| silicon | Firmware reflash over CDC | `tinytang_flash.py`, entries 80/86 |
| silicon | BL616 recovery via BOOT button | BRD-009 |

### SD and FatFS

| Status | Item | Evidence |
|---|---|---|
| host | Long file names | `test_fat_names` |
| host | Rename | `test_fat_rename` (16 checks, fsck-validated) |
| silicon | Card mounts and boot script runs | every power cycle |
| untested | `mkdir -p` — **the recorded defect could not be reproduced by reading** | handoff claims "reports success, creates nothing"; `mkdir_parents` and `_mkdir_r` (FR_EXIST→EEXIST) look correct. Needs a board measurement, not a code read |
| untested | Multi-chunk pack read on hardware | never done; first real one is English Wikipedia |
| untested | 4 GiB file boundary | KIWI-004 is the contract |

### Console and USB

| Status | Item | Evidence |
|---|---|---|
| host | Terminal lifecycle / backend restart | `test_terminal_lifecycle` |
| silicon | CDC enumerates at high speed | USB-001 |
| silicon | `ESC[?77n` probe answers 1/2/not-at-all | TOOL-011, this session |
| silicon | Console recovers after radio start | USB-008 |
| untested | `usb_cdc_bl616.c` and `tdsh_console_stdio_bl616.c` | no test at all |

### Shell

| Status | Item | Evidence |
|---|---|---|
| host | Script limits (48 vars, 31-byte names, 127-byte values, 16 KB stack) | `test_script_limits` |
| host | Help width under 80 columns | `check_help_width` |
| host | Editor memory limits | `test_editor_limits` |
| untested | The shell's own builtins: `cp`, `mv`, `rm`, `cat`, `head`, `mkdir`, `test`, `export` | only `mkdir -p` is questioned; the rest have no host test |
| count | 46 commands registered by this port, 25 by the shell core | counted from the `tdsh_command_t` tables |

Note: `check_help_width.py` reports 150 *entries*, which is the number of
width-checked string candidates it scans, not the number of commands. Do not
quote it as a command count.

### FPGA link

| Status | Item | Evidence |
|---|---|---|
| host | Frame format and block write | `test_desktop_core`, `test_phosphor_frames` |
| host | AE350 program load | `test_ae350_load` |
| silicon | Core answers on UART1 at 2 Mbaud | every boot |
| silicon | Register read/write over the link | `fpgar`, `tangini`, this session |
| untested | `tang_fpga_uart.c` as a whole — baud switch, lock, re-entrancy | no host test |

### Input

| Status | Item | Evidence |
|---|---|---|
| host | HID usage translation | `test_key` |
| host | Wii pad decode, centring, adopted kind | `test_pad`, `test_wiipad`, `test_osd_desk` |
| host | BLE bonds persistence | `test_bonds` |
| host | Bluetooth app widget | `test_bluetooth_app` |
| silicon | BLE keyboard, mouse, combo pair and reconnect | entries 77–82 |
| silicon | Classic Controller works at the prompt | entry 86 baseline |
| **open** | **Classic Controller dies after a game** | entries 86–88; two candidates named, uninstrumented |

### Heap, I2C and peripherals

| Status | Item | Evidence |
|---|---|---|
| untested | `tang_heap.c` — OCRAM/WRAM split, wrappers, refusal paths | no test |
| untested | `tang_i2c.cpp` — `tangi2c` and `tangtouch` command paths | no test (mux is tested separately) |
| untested | `tang_psram.c` | no test; part reports no PSRAM anyway (BL6-002) |
| untested | `tang_usbstat.c` | no test |
| untested | `tang_lora.c` and `tang_proto.c` | no test; exercised by hand on 2026-11-04 |
| untested | `tang_crash.c`, `tang_ble_trace.c` | no test |
| untested | `tang_jtag_programmer.c` | vendored, unmodified, untested |

---

## 5. Matrix — applications

Phase 3 work. Listed so the release picture is complete; not qualified yet.

| Status | App | Note |
|---|---|---|
| partial | TinyNES | `test_nes_ini` covers settings; the player and window have no host test. Silicon-proven per its own project |
| untested | TinyGBC | `tang_gbc_ini.c` is tested via `test_nes_ini`; the window and player are not |
| untested | Tiny89 | no host test in this tree |
| host | TinyPhosphor | `test_phosphor_catalog`, `test_phosphor_frames`; the window's other paths are untested |
| untested | TinyWiki, TinyGutenberg | `test_wiki_pack` covers the pack format, not the apps |
| untested | TinyNode | no host test |
| untested | Bluetooth, Software Update | `test_bluetooth_app` covers the first; `td_update_app.c` has none |

Out of scope for this release, per the operator: **TinySNES** is a single core
that reintegrates later. TinyCreature, TinyGBA, TinyVCMI and TinyMaps are not
shipping.

---

## 6. Coverage summary

- 31 host regressions, all passing; proven to bite where fault injection was
  done (desktop core: 6 faults).
- **32 port and Phosphor modules have no host test at all**, named by this
  command:

```
for f in ports/bl616/*.c ports/bl616/*.cpp ports/bl616/phosphor/*.cpp; do
  b=$(basename "$f")
  grep -ql "$b" tools/tests/*.{sh,c,cpp} tools/tests/sim/*.sv 2>/dev/null || echo "NOTEST $b"
done
```

The largest gaps in consequence, not in count: `tang_fpga_uart.c` (every register
access goes through it), `usb_cdc_bl616.c` (the only console), `tang_lora.c` and
`tang_proto.c` (just exercised by hand, not by test), `tang_heap.c`, and the
five app wrappers.

---

## 7. What Phase 0 changed

Nothing in `tinytang.bin`, the FPGA sources, or the firmware sources. Only
tests, the gate, and this document.

- `tools/tests/sim/tb_desktop_uart.sv` — brought to ABI 1.6, the PMOD UART
  engine instantiated behind the window, 20 new transactions, NRST polarity.
- `tools/tests/test_desktop_core.sh` — added `pmod_uart.sv` to both file lists
  (the iverilog PMOD bench could not elaborate without it), reply count 71→109.
- `tools/tests/test_phosphor_catalog.sh` — added `-I ports/bl616`, without which
  the app cannot find `tang_core_load.h`.
- `tools/tests/phosphor_catalog_test.cpp` — put the app in the directory state
  its own launch establishes; corrected four stale expectations (`19`→`20`
  entries because the folder it adds is listable, folder-first sort order,
  "tracks"→"items").
- `tools/tests/run_all.sh` — new.
- `tools/tests/test_fat_rename.sh` — executable bit.
- `docs/qualification.md` — this file.

Two defects were found in the *tests*, both older than this cycle, and both
recorded rather than quietly fixed:

1. The register bench had no UART engine, so the newest core personality could
   never have been checked there.
2. The PMOD bench could not elaborate since `pmod_uart` was added to
   `desktop_pmod`, and that was masked by the earlier abort.
