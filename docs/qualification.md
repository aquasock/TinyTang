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

- **Size**: 682,272 bytes for the baseline build recorded in entry 89
- **Build**: `make` — the timestamp is pinned in the `Makefile`

The `SOURCE_DATE_EPOCH` is load-bearing and is now **baked in**. The SDK's image
tool writes the app header's `BLFBVERF` string from the compiler's `__DATE__`
and `__TIME__`, so without a pinned stamp two builds of identical source differ
in three bytes and no firmware digest means anything. The `Makefile` sets
`SOURCE_DATE_EPOCH ?= 1793750400` and exports it, so a plain `make` now stamps
`Nov  4 2026 00:00:00` every time; `?=` keeps a caller free to choose another
stamp deliberately. Verified by two clean builds comparing byte-identical.

**The digest is commit-scoped, and that is by design.** `TINYTANG_BUILD_ID` is
the abbreviated commit hash, plus a hash of the uncommitted changes when the
tree is dirty, and it is compiled into the image. So a firmware digest names a
*tree state*, not a source tree:

- a clean build at commit `X` gives one digest, reproducible by anyone at `X`;
- any edit, even to a test, changes it;
- a build from a dirty tree can never be reproduced from a commit.

The baseline quoted in entry 89 (`ae318f02…`) was a dirty-tree build, so no
commit reproduces it. Treat it as the identity of one image, not of the source.
The value to quote for a release is the clean-tree build at the release commit,
and it can only be measured after that commit exists.

**The dirty component is coarser than it looks, and must not be trusted to
tell two builds apart.** When the only uncommitted change is a submodule's
working tree, `git diff` records a single line,
`Subproject commit <sha>-dirty`, and that is all the dirty hash covers. The
hash is therefore the same for *any* set of patches inside
`third_party/tinydesk`, and the same for any dirt in any submodule. Two
builds from different eras can carry the same identity:

- entry 66's release firmware: `dba104c-dirty.17f81b2`
- this tree at `685b3ac`: `685b3ac-dirty.17f81b2`

Same dirty component, different commit, different patch set, different code.
The `platform` string distinguishes *patched* from *unpatched*; it does not
distinguish one patch set from another. For a release, identify the firmware by
its SHA-256 **and** a recorded submodule state (`git submodule status` plus the
patch list `THIRD_PARTY.md` names), never by the build id alone.

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

**Current state: 43 of 43 pass** (42 scripts plus the width gate).

---

## 3. Matrix — the desktop core (`tinytang.bin`)

The register endpoint is `fpga/desktop/desktop_regs.sv`; the socket logic is
`desktop_pmod.sv`. ABI is **1.6**.

### 3.1 The wire protocol

Every access is one request and one reply on the core's UART link, in the legacy
`0xAA len_hi len_lo type payload` framing (PROT-001). The extended channel is
frame type `0x10` for register access and `0x12` for the validated block write
(EXTCTL-001, EXTCTL-002).

A type-`0x10` payload is 12 bytes: version `1`, opcode, sequence, address,
data, CRC-16. The CRC is CRC-16/CCITT-FALSE (poly `0x1021`, init `0xffff`, no
reflection, no final xor) over the frame type `0x10` and the 12 payload bytes.

| Opcode | Meaning | Reply opcode |
|---|---|---|
| `0x00` | capabilities: reads `0x13` (read32, write32, block write) | `0x80` |
| `0x01` | read32 | `0x81` |
| `0x02` | write32 | `0x82` |
| `0x04` | validated block write, frame type `0x12` | `0x84` |

Status codes carried in every reply:

| Code | Meaning |
|---|---|
| `0` | ok |
| `1` | version field is not 1 |
| `2` | opcode not supported for this frame |
| `3` | CRC mismatch |
| `4` | unknown address: right frame, wrong register |
| `5` | length or count malformed for the frame type |
| `6` | value refused by the register's own rules |

### 3.2 The register contract

One row per address. **Bold** marks a row whose evidence changed in this cycle.
A read-only address answers a write with status `4`: the register belongs to a
peripheral and a host must not forge it.

| Addr | Access | Meaning | Status | Evidence |
|---|---|---|---|---|
| `0x00` | R | core ID `0x00544453` | host | `test_desktop_core` |
| `0x04` | R | desktop ABI `0x00010006` | host, silicon | `test_desktop_core`; `tangini` 2026-11-04 |
| `0x08` | R | capability bitmap `0x0000010f` | host | `test_desktop_core` |
| `0xc0` | RW | socket word; validated, see 3.3 | host, silicon | `test_desktop_core`; `tangini` |
| `0x100` | R | active OLED grid (24x16, 12x8 or 16x8) | host | `test_desktop_core` |
| `0x108` | R | OLED frame counter, pass-through | **host** | `test_desktop_core` |
| `0x10c` | R | OLED signature, pass-through | **host** | `test_desktop_core` |
| `0x114` | RW | text cursor, 17 bits; write refuses `[31:17]≠0`, x≥24, y≥16 | host | `test_desktop_core` |
| `0x118` | RW | font mode 0/1/2; write refuses >2 | host | `test_desktop_core` |
| `0x120` | R | clock-monitor `clk` cycles, probe A | host | `test_desktop_core` |
| `0x124` | R | clock-monitor `clk` cycles, probe B | host | `test_desktop_core` |
| `0x128` | R | clock-monitor pixel cycles, probe A | host | `test_desktop_core` |
| `0x12c` | R | clock-monitor pixel cycles, probe B | host | `test_desktop_core` |
| `0x130` | R | clock-monitor 50 MHz reference cycles | host | `test_desktop_core` |
| `0x134` | R | `MON1` stamp `0x4d4f4e31` | host | `test_desktop_core` |
| `0x140` | R | I2C engine status | **host** | `test_desktop_core` |
| `0x144` | R | I2C read window, first four bytes | **host** | `test_desktop_core` |
| `0x148` | R | I2C read window, second four bytes | **host** | `test_desktop_core` |
| `0x154` | RW | I2C command: op`[1:0]`, addr`[14:8]`, reg`[23:16]`, len`[28:24]`, start`[31]` | **host** | `test_desktop_core` |
| `0x158` | RW | I2C auto-read: addr`[6:0]`, reg`[15:8]`, len`[20:16]`, enable`[31]` | **host** | `test_desktop_core` |
| `0x160` | RW | I2C write window, bytes 0–3 | **host** | `test_desktop_core` |
| `0x164` | RW | I2C write window, bytes 4–7 | **host** | `test_desktop_core` |
| `0x170` | RW | PMOD UART transmit byte; any value accepted | host, silicon | `test_desktop_core`; LoRa `AT` 2026-11-04 |
| `0x174` | R | PMOD UART received byte; **the read pops it** | host, silicon | `test_desktop_core` |
| `0x178` | R | PMOD UART status: bit0 rx avail, bit1 tx full, bit2 overrun, bit3 in flight, rx count`[19:8]`, tx count`[31:20]` | host, silicon | `test_desktop_core` |
| `0x17c` | RW | PMOD UART control: bit0 NRST low, bit1 flush, bit2 loopback, bit3 hold; write refuses `[31:4]≠0` | host, silicon | `test_desktop_core`; `lora loop` |
| `0x200`–`0x7fc` | RW | text cells, 4-byte stride, 16-bit word, `[31:16]` must be 0 | host | `test_desktop_core`, `test_desktop_link` |

Block write (`0x12`) takes 1–64 consecutive cells from `0x200` to `0x7fc`,
applies all or none, and refuses a word with `[31:16]≠0` (status 6) or a range
outside the cell window (status 4): host-tested in `test_desktop_core`.

### 3.3 Socket word, and the one deviation

`0xc0` accepts a 16-bit word: PMOD0 personality in bits 7:4, PMOD1 in 11:8,
flip flags in 12 and 13. Accepted combinations are both sockets none or
OLED (`≤1`), the VGA pair `2`+`3` in either order, and personality 8 on one
socket with the other released (`0x0080`, `0x0800`). Everything else is refused
whole with status 6 and both sockets keep their previous state.

| Status | Item | Evidence |
|---|---|---|
| host | `0x0000` accepted | `test_desktop_core` |
| host | 0/1 per socket accepted | `test_desktop_core` |
| host | VGA pair `0x0230`, `0x0320` accepted | `test_desktop_core` |
| host | Personality 8 as `0x0080` / `0x0800` accepted | `test_desktop_core` |
| host | Personalities 5, 6, 7 refused (status 6) | `test_desktop_core` |
| host | 8 on both sockets refused | `test_desktop_core` |
| host | A refused word leaves the previous word in place | `test_desktop_core` |
| host | Personality 8 pins: NRST on pin 8, TX on 9, RX on 10, per socket | `test_desktop_pmod` (`check_uart_pins`) |
| **deviation** | **Flip bit 12 on a personality-8 word is accepted and misroutes the UART** — **decision 2026-11-04: closed in the next core revision**, which the operator will build as the final firmware and `tinytang.bin` before release | demonstrated: `0x1080` drives NRST/TX on pins 2 and 3 while RX keeps reading pin 10 — a half-flipped link, silently. `0x2080` (bit 13, the released socket) changes nothing. Need a host that writes `0xc0` itself; no layout the firmware can produce sets a flip bit |
| silicon | `layout = lora` declared and accepted | `tangini`, 2026-11-04 |
| silicon | `layout = i2s2_oled` on the desktop core | core-log entries 22–24 |

The accept mask is `0xffffc00f`, so bits 4–13 pass unconstrained and the
personality rules in the same expression do the real work; they never inspect
bits 12 or 13. `test_desktop_pmod` asserts the current behaviour and fails with
"the deviation was fixed" if the RTL ever refuses the word, so a future core
revision that closes the hole cannot pass silently.

### 3.4 Video and compositor

| Status | Item | Evidence |
|---|---|---|
| host | HDMI raster, VGA mirror, no left-edge leak | `test_desktop_video` |
| host | Audio clock regeneration | `test_composite` |
| host | Link framing, ID, no unprompted frames | `test_desktop_link` |
| host | I2C master bit timing, NACK, stall | `test_desktop_core` (`tb_i2c_master`) |
| host | Mux channel select and retry | `test_i2c_mux` |
| silicon | Mux scan enumerates all 8 channels; 0x52/0x68/0x5a on 0/6/7; spare channels empty; select and release work | §6.2, 2026-11-04 |
| silicon | 1280x720 HDMI output with desktop | manual, README, photographs |
| silicon | DS3231 at 0x68, MPR121 at 0x5a, Wii adapter at 0x52 | BRD-011, BRD-015, entry 83 |
| silicon | **The RTC holds across a power cycle** — OSF clear after both inputs were removed and the board re-enumerated; time within the round trip of the host | §6.2, 2026-11-04 |
| silicon | **`tangi2c mux <n>` is not sticky** — the desk clock re-selects channel 6, so an unrouted address must be reached by a driver that re-selects per access (`tangtouch` does) | §6.2, 2026-11-04 |

## 4. Matrix — the BL616 platform

### Boot, flash and recovery

| Status | Item | Evidence |
|---|---|---|
| untested | Boot header validation | no host test; FLS-002 is the contract |
| untested | Soft reset lands in vendor loader | recorded, not tested |
| **host** | Crash record across a reset: checksum, init state machine, trap dispatch, controller fatal-error codes, report formatting, flash persist and reload, one-shot reporting | **`test_tang_crash`**; 4 planted faults caught |
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
| **host** | **`tang_fpga_uart.c`** — frame length and magic, the 2047-byte limit, the 100 ms transmit bound, re-entrant and owner-checked transmit lock, bounded acquisition, read32/write32 request packing and CRC, reply decoding, baud switch, pipe flag, joypad cache, ring drops, critical-section balance | **`test_tang_fpga_uart`**, against a UART FIFO model; 6 planted faults caught |

### Input

| Status | Item | Evidence |
|---|---|---|
| host | HID usage translation | `test_key` |
| host | Wii pad decode, centring, adopted kind | `test_pad`, `test_wiipad`, `test_osd_desk` |
| host | BLE bonds persistence | `test_bonds` |
| host | Bluetooth app widget | `test_bluetooth_app` |
| **host** | **Bluetooth HID report routing** — which parser takes a notification: handle match, boot-format lengths (8 keyboard, 3/4 mouse), the combo slot carrying both, handle 0 as "undiscovered", and equivalence with the inline code it replaced over 65,520 inputs | **`test_tang_hid_route`**; 8 planted faults caught |
| silicon | BLE keyboard, mouse, combo pair and reconnect | entries 77–82 |
| silicon | Classic Controller works at the prompt | entry 86 baseline |
| **open** | **Classic Controller dies after a game** | entries 86–88; two candidates named, uninstrumented |

### Heap, I2C and peripherals

| Status | Item | Evidence |
|---|---|---|
| **host** | `tang_proto.c` — encode/decode, CRC-16/CCITT-FALSE vector, length boundaries, truncation, version, reply typing, sequence match | **`test_tang_proto`**; 5 planted faults caught |
| **host** | `tang_lora.c` — base64 both ways, hold-then-burst write, `+RCV` reassembly, address filter, sequence match, retry budget, link lock | **`test_tang_lora`**, against a PMOD UART model; 5 planted faults caught |
| **host** | `tang_heap.c` — refusal recording, `MM_FLAG_HEAP_ANY` for task stacks, the krealloc move path, active-heap sum | **`test_tang_heap`**; 4 planted faults caught |
| host | `tang_i2c_mux.c` — channel map, routing rule, channel names | `test_i2c_mux` |
| **host** | `tang_i2c.cpp` — command-word packing, status decode, mux routing and probe, per-channel scan, `tangi2c` read/write/mux/auto/rtc and `tangtouch` init/status/watch | **`test_tang_i2c`**, against a simulated register window and bus; 9 planted faults caught |
| untested | `tang_psram.c` | no test; part reports no PSRAM anyway (BL6-002) |
| untested | `tang_usbstat.c` | no test |
| host | `tdsh_fs_bl616.c` — fd table, open-mode mapping, errno per FatFS result, seek, stat with the directory probe, unlink/mkdir/rmdir/rename, opendir walk, ENOSYS calls | **`test_tdsh_fs`**; 6 planted faults caught |
| untested | `tang_ble_trace.c` | no test |
| untested | `tang_jtag_programmer.c` | vendored, unmodified, untested |

Two findings came out of writing these tests rather than from reading the code.
`tang_proto_decode`'s exact-length check is **redundant** with the CRC: the CRC's
span is derived from the buffer's actual length, so a truncated message covers
different bytes and cannot match. Planting a fault in that check does not fail
the test, because no natural truncation distinguishes it. It is defence in depth
against a length field disagreeing with a CRC that happens to validate, and the
test says so rather than pretending to cover it. And `tang_lora.c`'s receive
assembler keys frames by id, so a new id restarts the assembly instead of
splicing two messages — a behaviour worth having a test on, because the failure
it prevents is silent.

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
that reintegrates later.  Its launcher (`tinysnes_app.cpp`) and the cartridge
loader it shares with `snesload` (`tang_snes_load.c`, covered by
`test_snes_load`) are in the tree and board-tested, but not qualified. TinyCreature, TinyGBA, TinyVCMI and TinyMaps are not
shipping.

---

## 6. Coverage summary

- 39 host regressions plus the width gate, all passing; proven to bite where
  fault injection was done (desktop core 6 faults, protocol 5, LoRa 5, heap 4,
  I2C 9, link 6, crash 4, FatFS 6, HID routing 8).
- **30 of 59 port modules are built by no host test**, the thirtieth being
  the TinySNES launcher added after this summary was taken. The eight
  this programme added — `tang_proto.c`, `tang_lora.c`, `tang_heap.c`,
  `tang_i2c.cpp`, `tang_fpga_uart.c`, `tang_crash.c`, `tdsh_fs_bl616.c` and
  `tang_hid_route.c` — are covered. Measured by:

```
tools/check_test_coverage.py            # the uncovered modules
tools/check_test_coverage.py --covered  # with the test that builds each
```

**The figure this document quoted before was wrong, and the tool exists because
of it.** The earlier command was

```
grep -ql "$basename" tools/tests/*.{sh,c,cpp}
```

which counts a *mention*. Two comments in `tb_frames.c` and `tb_saves_log.c`
merely name `tang_fpga_uart.c` — "used to live inside tang_fpga_uart.c" and
"the exact shapes ports/bl616/tang_fpga_uart.c writes" — and that was enough to
report the link as covered while nothing compiled it. The real count was 29
where this file said 28, and the same false positive can hide any module a test
comment talks about. `tools/check_test_coverage.py` requires a module to be
either named by path in a test script, which is how it reaches a compiler, or
included by a test source, and it strips comments first. Section 6's numbers are
that tool's, not a grep's.

### 6.1 The other 30, deferred with reasons

These are **knowingly deferred**, not overlooked. The coverage sweep was stopped
by the operator after the seven modules that matter were covered, because driving
the count to zero would spend many cycles proving things about a firmware whose
remaining risk is on the board, not in a host stub. Each group below says why a
host suite is not the next best use of a cycle.

| Modules | Reason |
|---|---|
| `tang_ble.c` (2681 after the extraction) | Needs 56 `bt_*` symbols and most of the Zephyr-derived host stack to link, so a suite would stub the stack and test the stubs. **Its one testable decision has been extracted**: report routing now lives in `tang_hid_route.c` and is covered (BLE-019, and the Logitech and Rii K06 cases). What remains in `tang_ble.c` is discovery, pairing, bonds and the stack's lifecycle, all of which need the stack or the radio. |
| `tang_jtag_programmer.c` (1102) | Vendored unmodified from nand2mario under Apache-2.0 (TOOL-007). Testing it tests a third party's code, and its behaviour is proven by every `tangload` on the board. |
| 8 app wrappers: `tinygbc_app`, `tinynes_app`, `tinysnes_app`, `td_gutenberg_app`, `td_wiki_app`, `tiny89_app`, `tinynode_app`, `tinycreature_app` (6269 total) | Mostly widget layout and list navigation over the app's own data. A stubbed window proves the layout calls; Phase 3 exercises them end to end on the board, which is both cheaper and truer. The data-side logic (`tang_nes_ini`, `tang_gbc_ini`, `tang_snes_load`, the pack formats) is already covered separately. |
| 9 Phosphor transport files (`phosphor_player`, `phosphor_cmd`, `pmod_sockets`, `fpga_file_stream`, `ae350_play`, `fpga_debug`, `oled_cmd`, `ae350_file_server`, `fpga_stream`; 2722 total) | Each is a thin layer over the extended-protocol endpoint with a core and an AE350 on the far end. The protocol itself is covered by `test_desktop_core`, `test_phosphor_frames` and `test_tang_fpga_uart`; what remains is the far end's behaviour, which only hardware supplies. |
| `tang_osd.c` (321) | Commands that assert an overlay and write a text page. Covered on the board by `osd`, and the page format is `test_desktop_core`'s. |
| `tang_fw_update.c` (258), `td_update_app.c` (210) | The path that rewrites the firmware. A host test can check the header arithmetic, but the behaviour that matters — a power loss mid-commit — is destructive and belongs in a deliberate hardware trial, not a stub. |
| `usb_cdc_bl616.c` (418) | **The largest real gap.** The only console. Hosting it means modelling the SDK's USB device stack, which is the thing most likely to be wrong. Deferred rather than dismissed: the board exercises it every session, and its failure modes (USB-007, USB-008) are already recorded. |
| `td_desktop_bl616.c` (432), `tdsh_platform_bl616.c` (718), `tdsh_console_stdio_bl616.c` (41) | Registration tables and platform glue. `check_help_width.py` already parses every registered command, which is the part a stub could catch. |
| `td_pack.c` (260) | The pack *format* is checked by `tools/check_wiki_pack.py` against real converted archives, which is a stronger claim than a stub. |
| `tang_usbstat.c` (274), `tang_psram.c` (145), `tang_ble_trace.c` (182) | Register readers and a trace buffer. `tang_psram` cannot be exercised on this board at all — the part reports no PSRAM (BL6-002). |

The two that would be worth the next cycle if the operator wants more:
**`usb_cdc_bl616.c`** because it is the only console, and **`tang_fw_update.c`**
because a wrong commit is unrecoverable without a BOOT button.

### 6.2 Silicon results taken 2026-11-04

- **The DS3231's backup supply holds.** After setting the RTC, a full power
  cycle (both inputs removed; the USB device re-enumerated as `088`) left
  `status 0x0F = 0x08` — **OSF clear** — and the time correct to within the
  round trip: `02:28:55` against a host reading of `02:28:59`, then `02:29:38`
  against `02:29:41` thirty seconds later. The earlier invalid state was an
  event, not a flat cell. Entry 83 recorded the same transient after probe work
  and attributed it to a register-pointer write; nothing here contradicts that.
- **The mux scan enumerates all eight channels.** `tangi2c scan` found `0x52` on
  channel 0, `0x68` on channel 6 and `0x5a` on channel 7, with channels 1–5
  reporting "the switch only". Selecting a spare channel and releasing it both
  work, which is the mechanism a new device needs.
- **`tangi2c mux <n>` is not sticky, by design.** `tangi2c mux 7` followed by
  `tangi2c read 5a 00 2` fails consistently, because the desk clock's RTC read
  re-selects channel 6 between the two commands and `0x5a` is not routed by
  address. The correct route for an unrouted device is a driver that re-selects
  per access — `tangtouch` does, and reads `0x0000 none` on the same sensor. This
  is BRD-015's rule working as recorded, and it means the "wrong channel" test
  used to prove isolation was invalid: it measured the clock, not the switch.
- The board's image (`5039b09-dirty.04ed790`) predates the channel-7 rename in
  entry 91, so `tangi2c mux 7` still prints `ir` there. The tree says `touch`; the
  board will too once a firmware built after that entry is deployed.

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

---

## 8. What Phase 1A and 1B changed

Phase 1A wrote the contract table in §3.2 and closed the registers that had no
coverage: `0x108`, `0x10c`, `0x140`, `0x144`, `0x148`, `0x154`, `0x158`,
`0x160` and `0x164`. Phase 1B gave the settled-config path its first automated
coverage.

- `tools/tests/sim/tb_desktop_uart.sv` — the I2C engine's side of the register
  file is now driven by the bench (status, both windows, command and auto-read),
  and the two OLED counters are driven rather than tied to zero. Reply count
  109→147.
- `tools/tests/tb_tang_proto.c`, `tools/tests/test_tang_proto.sh` — new.
- `tools/tests/tb_tang_lora.c`, `tools/tests/test_tang_lora.sh`,
  `tools/tests/stubs/tang_lora/` — new; the stubs are FreeRTOS, the timer, the
  shell types and the two link helpers, nothing else.
- `tools/tests/tb_tang_heap.c`, `tools/tests/test_tang_heap.sh`,
  `tools/tests/stubs/tang_heap/` — new; the SDK allocator is a table so each
  call can be made to refuse on demand.
- `ports/bl616/tang_i2c_mux.[ch]` — channel 7 is named for the MPR121 it
  actually carries, `TANG_I2C_MUX_CH_TOUCH`, with the old spelling kept as an
  alias. The shell had been printing `ir` for a touch sensor since 2026-10-30.
- `ports/bl616/tang_i2c.cpp` — the touch driver names the new constant.
- `tools/tests/tb_i2c_mux.c` — asserts the new name and that the alias routes
  identically.

`docs/qualification.md` also gained the build-id caveat: the dirty component of
`TINYTANG_BUILD_ID` hashes only the `Subproject commit …-dirty` line, so it is
identical for any patch set inside `third_party/tinydesk`. Two builds from
different eras carry the same identity, so a release must name the firmware by
SHA-256 **and** the submodule state, never by the `platform` string alone.

Nothing here changed the RTL, `tinytang.bin`, or the firmware image. The two
firmware-source edits are the channel-7 rename and its use, which do not alter
any pin, register or wire behaviour.

---

## 9. What the I2C cycle changed

`ports/bl616/tang_i2c.cpp` was the last large untested module on the settled
path: 911 lines, seven subcommands, and the three devices the operator's bench
actually carries. It is now covered by `tools/tests/tb_tang_i2c.cpp`, which
takes the FPGA engine's place behind `fpga_debug_request()` — the single call
the driver makes for every register access — so one simulated window answers
for all of it:

| Address | Modelled as |
|---|---|
| `0x140` | status: valid, busy, nack, scan_done, seq, scan address and count |
| `0x144`/`0x148` | the eight-byte result window |
| `0x154` | command write, which arms an operation |
| `0x158` | the automatic-read word |
| `0x160`/`0x164` | the write window |

An armed command completes on the **next status read**, which is the engine's
own shape: busy rises in the clock the command is written and falls when the
result is latched, a read moves the sequence number and a write does not. That
means the driver's poll loop and its "done when the sequence moves" rule are
exercised rather than bypassed. Behind the window sit a PCA9548A, a DS3231 and
an MPR121, all register-addressable, with the switch gating which device can
answer at all.

What the suite pins, in the order the commands use it:

- `cmd_word` and `auto_word` bit packing, including that the address masks to
  seven bits and the length to five rather than spilling into a neighbour.
- Status decode across all four flag bits and the three packed fields.
- The routing rule: `0x52` selects channel 0, `0x68` selects channel 6, and an
  unknown address selects nothing.
- `tangi2c mux <n>` and `off`, with out-of-range and non-hex refused, and the
  no-switch path reporting a direct bus that still works.
- The per-channel scan, with the switch's own reply subtracted, and a dead bus
  reporting zero devices rather than a false success.
- That the once-a-second read is turned off by the probe and **stays** off:
  if something switches it back on, a transaction turns it off again rather
  than restoring it, because the switch cannot follow that read.
- `tangi2c rtc`: the seven-byte read, the OSF flag, 12- and 24-hour decoding
  (including that 12 PM is hour 12 and not 0), `set` parsing and its refusals,
  and `sync` reporting the clock's own verdict.
- The MPR121 init sequence register by register, with `0x5E` off before the
  filter blocks and `0x8C` last so the chip starts only once everything above
  it is in place; the two-byte poll at `0x00`; and the electrode mask printed
  as `E0`/`E2`/`E11`.
- That every touch access re-selects channel 7, shown by moving the switch to
  channel 6 with an RTC read and then polling.

Nine planted faults were each caught. One is worth naming because it says the
guard is load-bearing rather than decorative: raising `tangi2c write`'s byte
limit past eight makes UBSan report an index out of bounds for `data[8]`, so
the `n > 8` check is what stops a long list overrunning the stack buffer.

One fault was not caught on the first attempt and the test was strengthened
rather than the fault waived: restoring the automatic read while the switch is
fitted is invisible unless the read is actually on when the transaction starts.
The suite now switches it on behind the driver's back and asserts it is turned
off and left off.

---

## 10. What the link cycle changed

`ports/bl616/tang_fpga_uart.c` is the module every register access in this
project passes through, and it had no test. It is 2356 lines, but its
hardware touches are narrow, and the bench takes their place:

| Seam | Modelled as |
|---|---|
| `uart_write()` | a register file: the TX FIFO's free count comes from one offset, pushed bytes are captured from another, and the room can be refused to provoke the 100 ms bound |
| the RX interrupt | the ring is fed directly, and a reply can be made to arrive *during* a call, which is what the interrupt does |
| the transmit lock | the "current task" is whichever the test says, so contention is a switch rather than a second thread |
| `fpga_frames.c`, `ae350_load.c`, the three ini modules | linked for real, not stubbed |

What the suite pins:

- **Frame construction.** `AA len_hi len_lo type payload`, with the length
  counting the type byte, a big-endian length above 255, and the 2047-byte
  limit: 2046 payload is legal, 2047 is refused with nothing left on the wire.
- **The 100 ms transmit bound.** A stuck FIFO is reported rather than spun on.
- **The transmit lock.** Re-entrant by owner, counted by depth, released once —
  and another task's unlock is not ours to give, so the holder keeps it. The
  bounded form nests for the owner and *fails* for a second task rather than
  blocking, which is what stops a host pipe or a stuck load from wedging a
  caller. Before the link is open all three calls are harmless.
- **`read32`/`write32`.** The 14-byte payload, opcode 1 and 2, the address and
  value big-endian in the right fields, the CRC-16 over frame type `0x10` plus
  twelve bytes, the 15-byte reply and its status byte, and the reply's register
  at bytes 9..12. A short reply, a refused status and a busy link are each
  failures rather than partial reads.
- **`core_id()`** sends the command and takes a reply that arrives during the
  wait — and `core_id()` drains before transmitting, so a frame queued earlier
  is deliberately discarded rather than mistaken for this answer.
- **The rest of the surface:** baud switch (de-init, re-init, the new rate
  reported), the pipe flag leaving the ring to its owner, the joypad cache
  reading words big-endian, ring overflow counted rather than overwriting, and
  every critical section left balanced — an imbalanced one would leave
  interrupts off, which on this board stops the link's own receive.

Six planted faults were each caught: the frame length off by one, the lock not
re-entrant, the unlock ignoring its owner, the CRC over eleven bytes, the
address bytes reversed, the reply length unchecked, and the bounded take made
unbounded.

Two of the bench's own bugs were worth recording, because both were the kind
that makes a test lie. The virtual clock had to advance a millisecond per read:
`uart_write()` polls the clock with no yield, so with a frozen clock its timeout
never fires and the test hangs rather than failing. And four assertions were
wrong in my favour — the CRC's address field is a register address rather than
a value, a pipe owning the ring means no bytes are consumed at all, and the
joypad words are big-endian in the frame — each corrected against the module,
not the reverse.

`tools/check_test_coverage.py` was added in this cycle for the reason Section 6
gives: the number the matrix had been quoting came from a grep that counted
comments, so the coverage claim it supported could not be trusted.

---

## 11. What the crash and FatFS cycles changed

### `tang_crash.c`

The recorder that has to work when nothing else did, built with
`-DTINYTANG_HOST_TEST`. Its hardware is replaced: the three CSRs are settable,
the flash sector is a buffer, `GLB_SW_POR_Reset()` leaves through `longjmp`
instead of resetting, and the watchdog device is a counter. What the suite pins:

- **The checksum** — FNV-1a over every field before `sum` and none after, so a
  record can be sealed and then verified; a change in any covered field moves
  it, and a change to `sum` itself does not.
- **`copy_name`** — truncation to `n-1`, zero-fill of the remainder, and a NULL
  source as an empty name.
- **`cause_name`** — the masked lookup, including that the interrupt bit and the
  high bits are not part of the cause.
- **The init state machine** — a power-up leaves nothing valid; a correctly
  sealed RAM record is taken and its boot count carried forward; a corrupt sum,
  a wrong magic or a future version is refused; and a flash record wins over a
  RAM one, because it is the crash that survived the power cycle.
- **The trap dispatch** — mcause 8 and 11 pass through to the SDK's handler and
  record nothing; anything else records and resets. The "code argument rather
  than a register frame" case, which is what made 2.3.36 fault twice before
  recording, is the single trap the suite runs, and it asserts that no callee
  register is taken from a code.
- **The controller's fatal-error exit** — `0xC3C3C3C3` and `0xA5A5A5A5` go on to
  the library, checked as "no reset happened" rather than as a counter, because
  a reset on an ordinary return is a permanent boot loop; any other code is
  recorded with the code in `mtval`.
- **The report** — each kind's detail line, the "reset while running" hang case
  with the reset recorder's active-low watchdog bit both ways, a deliberate
  reset reporting nothing, the trail printed newest-first, and the one-shot
  report appending to `/sd/crash.log` and clearing the sector.

Four planted faults were caught. One of them needed a better test first:
removing the fatal-error magic-code check was invisible, because the call was
not inside a `setjmp` and the reset it caused escaped detection — the suite now
wraps that call so "this must not reset" is itself asserted.

### `tdsh_fs_bl616.c`

The newlib-to-FatFS syscall layer, with FatFS results chosen per call so the
errno mapping is exercised against FatFs's own values:

- **The file-descriptor table** — eight slots behind a tag bit; the ninth open
  is `EMFILE`; a refused open gives its slot back (checked by eight refusals
  followed by eight successes in one world); closing the console frees nothing,
  checked with the table full so a spurious free would show.
- **The open-mode mapping** — `O_CREAT` alone is open-or-create and must not
  empty an existing file; `O_CREAT|O_TRUNC` is create-always; `O_APPEND` is the
  append bit.
- **The errno each result becomes** — `FR_NO_FILE`/`FR_NO_PATH` → `ENOENT`,
  `FR_DENIED` → `EACCES`, anything else → `EIO`; and `FR_EXIST` → `EEXIST` for
  `mkdir`, which is what makes the shell's `mkdir -p` idempotent. That last one
  is load-bearing rather than cosmetic, and planting a fault there fails the
  test.
- **Seek, fstat, stat** — `SEEK_CUR` and `SEEK_END` arithmetic, `ESPIPE` on the
  console, character versus regular in `fstat`, and the directory probe in
  `_stat_r` that makes `ls /` work when FatFS cannot describe the volume root.
- **Directories** — the `opendir`/`readdir`/`closedir` walk with `DT_DIR` and
  `DT_REG`, a stable NULL at the end, and no double free.

Six planted faults were caught. Two of the assertions had to be strengthened
first: a `reset_world()` between the failing opens and the successful ones
defeated the leak check, and the console-close check only asserted a return
value rather than that the table was undisturbed.

**`mkdir -p` is not in this file.** The recursive parent walk is the shell's
`mkdir_parents` in `third_party/tinydesk-shell/src/core/tdsh_builtin.c`, which
this suite does not cover; what it pins is the `EEXIST` mapping that walk
depends on.

### The one firmware change

`tang_crash.c` could not be compiled off-target at all: an inline-asm statement
is rejected by the assembler whatever the optimiser might elide. Four
RISC-V-only instructions — three CSR reads, `csrc mstatus, 8`, and the `.word 0`
that `crash test trap` executes — are now selected by `TINYTANG_HOST_TEST`, with
the original instructions in the `#else` branch verbatim. On the target the
macro is absent and the same instructions are built as before.

**The firmware image digest still changes**, because the guard moves line
numbers in the preprocessed file and the build id covers the whole tree. The
baseline recorded in §1 is therefore superseded for any build that includes
this change; the instruction stream for this file is unchanged.

---

## 12. The English Wikipedia pack, built 2026-11-04/05

The first multi-chunk pack this project has produced, and **the first one read on
hardware** — the gap carried since the pack format was designed is now closed for
the main path; see the read result below. Built by
`tools/convert_wikipedia_overnight.py` from
`wikipedia_en_all_nopic_2026-06.zim` in **7h25m** (20:43 → 04:08), which is
close to the 7h15m its own rate estimate predicted at ~320 entries a second.

| Fact | Value |
|---|---|
| Records | 8,425,137 |
| Redirects resolved | 1,185,677 |
| Chunks | 11 (`wiki.000`–`wiki.010`) |
| Chunk size | 3.00 GiB each, the last 2.93 GiB |
| Pack total | 32.93 GiB in 14 files |
| Parts (chapter navigation) | 1,097 |
| Authors | 0 — an articles pack has none, so there is no `wiki.auth` |
| Verified | `check_wiki_pack.py`: 8,425,137 records, 16,851 bodies sampled, 11 chunks, OK |

The archive's content was 52,690,706,539 bytes with **1,012,842,619 bytes of junk
after its MD5 trailer**; `tools/zim_verify.py` matched the stored checksum over
exactly the content bytes, which is the check the old shell-script logic could
not perform.

**Chunk boundaries are exact.** For every chunk the last record's
`offset + length` equals the file size, so the gap at the end of each chunk is
**zero** and the writer cut only on an article boundary — KIWI-004's rule, and
the reason a body can never be split across two files. The final article in
chunk 0 ends with Wikipedia's licence footer, not mid-word.

**The reader has no chunk cap.** `td_wiki_app.cpp` opens `wiki.%03u` from the
record's own chunk field, so 11 chunks is not a limit case; `TD_PACK_MAX` (16)
bounds how many *packs* a folder may hold, not chunks.

**One record is wrong, and it is the converter's fault.** The pack contains a
single article titled `null` whose body is a MathML namespace notice. The cause:
the archive keeps its own machinery beside the articles under URL prefixes that
begin with an underscore (`_assets_`, `_mw_`, `_res_`, `_webp_`), and one such
page — `_mw_/MathML` — carries `text/html` **and** the literal string `"null"` as
its ZIM title, so it passed the namespace, mimetype and length filters and was
converted. Measured across the whole archive: 8,516,992 namespace-C text/html
entries, of which exactly **one** has an underscore-prefixed URL. The converter
now skips underscore-prefixed URLs and treats a `"null"` title as absent;
`check_wiki_convert.py` builds that exact entry and fails if it reaches the pack,
which was verified by reverting the filter and watching four checks fail.

The built pack was **not** rebuilt for that: re-running 7h25m to remove one
bogus article in 8.4 million is not worth the electricity, and the pack's
structure is unaffected. Copy it to the card as-is, or rebuild if a clean index
matters more than the time.

### It reads on the board

Copied to `/root/wiki/wikipedia` on 2026-11-05: all 14 files byte-identical to
the source by `cmp`, and `check_wiki_pack.py` against the **card copy** reported
8,425,137 records with 4,213 bodies sampled across 11 chunks, OK.

The user then read **Arizona** — the whole article — on the board. That is a
stronger result than it looks, because `read_page()` in `td_wiki_app.cpp` opens
the chunk file **afresh on every page turn**: `f_open("wiki.%03u")` then
`f_lseek` to `record->offset + cursor`. Arizona is 91,031 bytes against a
12,288-byte page, so the article is **8 pages**, and reading it end to end means
eight separate open-seek-read cycles into `wiki.005` starting at offset
1,009,142,055.

What that establishes:

- The record's **chunk field routes to the right file**. This was the core of
  the multi-chunk question: a pack whose records all landed in chunk 0 would
  read correctly and still be broken for any archive this size.
- **Seeking ~1 GB into a 3 GiB file** works on FAT32 through the port's syscall
  layer.
- **Paging across 8 pages** works, each turn a fresh seek.
- The article was **not truncated**: 91,031 bytes is well under the 256 KB
  article cap, and the user read to its end.

**Two boundaries that read did not cross**, and which a thorough qualification
should still take:

| Boundary | Why it matters | Target |
|---|---|---|
| The last chunk, `wiki.010` | 2.93 GiB rather than 3.00, so its offset space ends differently from the other ten | `A & E Home Video` — chunk 10, offset 311,802,683, 52,753 bytes, 5 pages |
| An offset above 2 GiB | Arizona sits at ~1.0 GB. Each chunk is 3 GiB, so roughly its last third lies above `0x80000000` — the classic place a 32-bit signed offset wraps | `A battuta` — chunk 9, offset 2,500,311,686, 64,713 bytes, 6 pages |

Neither is known to fail; neither has been tried. Until they are, the honest
statement is that multi-chunk reading is proven for chunks 0–9 below the 2 GiB
boundary, not for the whole pack.
