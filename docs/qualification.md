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

**Current state: 32 of 32 pass** (31 scripts plus the width gate).

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
| **deviation** | **Flip bit 12 on a personality-8 word is accepted and misroutes the UART** | demonstrated: `0x1080` drives NRST/TX on pins 2 and 3 while RX keeps reading pin 10 — a half-flipped link, silently. `0x2080` (bit 13, the released socket) changes nothing. Need a host that writes `0xc0` itself; no layout the firmware can produce sets a flip bit |
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
| silicon | 1280x720 HDMI output with desktop | manual, README, photographs |
| silicon | DS3231 at 0x68, MPR121 at 0x5a, Wii adapter at 0x52 | BRD-011, BRD-015, entry 83 |

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
| **host** | `tang_proto.c` — encode/decode, CRC-16/CCITT-FALSE vector, length boundaries, truncation, version, reply typing, sequence match | **`test_tang_proto`**; 5 planted faults caught |
| **host** | `tang_lora.c` — base64 both ways, hold-then-burst write, `+RCV` reassembly, address filter, sequence match, retry budget, link lock | **`test_tang_lora`**, against a PMOD UART model; 5 planted faults caught |
| **host** | `tang_heap.c` — refusal recording, `MM_FLAG_HEAP_ANY` for task stacks, the krealloc move path, active-heap sum | **`test_tang_heap`**; 4 planted faults caught |
| host | `tang_i2c_mux.c` — channel map, routing rule, channel names | `test_i2c_mux` |
| untested | `tang_i2c.cpp` — `tangi2c` and `tangtouch` command paths | 911 lines, seven subcommands; the next cycle |
| untested | `tang_psram.c` | no test; part reports no PSRAM anyway (BL6-002) |
| untested | `tang_usbstat.c` | no test |
| untested | `tang_crash.c`, `tang_ble_trace.c` | no test |
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
that reintegrates later. TinyCreature, TinyGBA, TinyVCMI and TinyMaps are not
shipping.

---

## 6. Coverage summary

- 34 host regressions plus the width gate, all passing; proven to bite where
  fault injection was done (desktop core 6 faults, protocol 5, LoRa 5, heap 4).
- **29 port and Phosphor modules have no host test at all**, down from 32 with
  `tang_proto.c`, `tang_lora.c` and `tang_heap.c` now covered. Named by this
  command:

```
for f in ports/bl616/*.c ports/bl616/*.cpp ports/bl616/phosphor/*.cpp; do
  b=$(basename "$f")
  grep -ql "$b" tools/tests/*.{sh,c,cpp} tools/tests/sim/*.sv 2>/dev/null || echo "NOTEST $b"
done
```

The largest gaps in consequence, not in count: `tang_fpga_uart.c` (every
register access goes through it), `usb_cdc_bl616.c` (the only console),
`tang_i2c.cpp` (the whole I2C surface, and the settled config's three devices),
and the five app wrappers.

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
