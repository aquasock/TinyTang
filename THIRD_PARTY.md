# Third-party components

TinyTang's own code is MIT; the text is in `LICENSE`. Thirteen files carried
over from earlier projects are Apache-2.0, and the entries below name them and
their authors: nand2mario's JTAG programmer, and ten Phosphor transport files
and two host tests from Tang-Control. Each says so in its header; that file's
licence governs it and MIT governs everything else. Reproducing both
licences is deliberate: an Apache-2.0 file inside an MIT project keeps its own
terms, and the pointer in its header has to resolve to something true.

The components below are inherited, vendored, or consulted. Where a source was
read as a specification rather than copied, that is said plainly, because that
distinction is what decides which obligations attach. The same facts are kept
as records in `.ai/core-reference.md` (`PROV`, `TOOL-007`, `TOOL-016`,
`TDSH-001`, `TDESK-001`, `PROT-001`).

## TangCore firmware (nand2mario)

The board knowledge this firmware was built on, and the lineage it descends
from:

- Project: https://github.com/nand2mario/tangcore
- Author: nand2mario and contributors
- License: Apache-2.0 for the firmware (`firmware-bl616`). Each core under the
  same repository keeps its own licence; no core is used here.

TinyTang descends from TangCore: it started from Tang-Control's board layer,
and Tang-Control is a fork of TangCore's `firmware-bl616`, whose board layer
and JTAG programmer are nand2mario's. The board knowledge learned there --
pins, the SD card's power switch, the USB stack's quirks, the flash layout --
is his groundwork; every such fact is called out in the source where it is
used and recorded in `.ai/core-reference.md`. No TangCore source file is
copied into TinyTang other than the programmer below.

## Tang-Control

The intermediate project, a fork of TangCore's `firmware-bl616` by this
project's author, retired in favour of TinyTang:

- Project: https://github.com/aquasock/Tang-Control
- License: Apache-2.0, as a fork of an Apache-2.0 project

The Tang-Phosphor host side is ported from Tang-Control into
`ports/bl616/phosphor/`: `fpga_ext_frame.h` and `flac_stream_prefix.h` copied
unchanged, `fpga_debug`, `fpga_stream` and `fpga_file_stream` ported with only
their link plumbing changed, and `ae350_play` with its card path changed and a
settle delay added after the AE350 restart, each saying so in its header. The host tests
`tools/tests/fpga_ext_frame_test.cpp` and `flac_stream_prefix_test.cpp` are
copied unchanged. These were written in Tang-Control by this project's author,
not taken from TangCore, and they keep Tang-Control's Apache-2.0 here; the
licence text is at `LICENSES/Apache-2.0.txt`. The files: `ae350_play.cpp`,
`ae350_play.h`, `flac_stream_prefix.h`, `fpga_debug.cpp`, `fpga_debug.h`,
`fpga_ext_frame.h`, `fpga_file_stream.cpp`, `fpga_file_stream.h`,
`fpga_stream.cpp` and `fpga_stream.h` in `ports/bl616/phosphor/`, and
`tools/tests/fpga_ext_frame_test.cpp` and `flac_stream_prefix_test.cpp`.

## Gowin JTAG programmer (nand2mario)

`ports/bl616/tang_jtag_programmer.c` is nand2mario's bit-banged GPIO JTAG
programmer for Gowin GW5A and GW2A, taken from Tang-Control's
`fpga/programmer.cpp`. Used unmodified apart from its include list and one
added comment saying where its licence text lives in this tree;
`ports/bl616/tang_jtag_glue.h` supplies what it expected from its own tree and
documents this provenance.

- Copyright: (c) 2025.2, nand2mario
- License: Apache-2.0, reproduced at `LICENSES/Apache-2.0.txt`. It is the one
  file of nand2mario's in the tree; the other Apache-2.0 files are listed
  under Tang-Control above
- Based in part on openFPGALoader by Gwenhael Goavec-Merou,
  https://github.com/trabucayre/openFPGALoader, also Apache-2.0

## TinyDesk Shell

The shell this firmware runs, consumed as `third_party/tinydesk-shell`:

- Project: https://github.com/tinydesk-project/tinydesk-shell
- Commit: `a067fa7` (upstream `main`, ahead of the 0.1.6 release; `8456dd1`,
  v0.1.5, is the last release tag)
- License: MIT

TinyTang carries `third_party/patches/tdsh/0001-configurable-script-limits.patch`
for a build against v0.1.5, applied idempotently by
`scripts/apply-tdsh-patches.sh` during CMake configuration. It wraps four
public memory limits in `#ifndef` without changing their defaults, and
documents that all components using `tdsh.h` must share the overrides because
they change the session layout. The change itself is upstream from `7cffa85`,
so against a newer checkout the script detects it and applies nothing; the
patch file stays for the v0.1.5 pin.
The matching upstream discussion is
https://github.com/tinydesk-project/tinydesk-shell/issues/2.
TinyTang sets 48 variables, 32-byte name buffers, 128-byte value buffers and
a 16 KB script stack from `CMakeLists.txt`. Session cloning and script
isolation follow upstream's implementation. The patch is MIT, as is the
upstream code it modifies. Remove it when a pinned release provides the guards.

## TinyDesk

The desktop environment, consumed as `third_party/tinydesk`:

- Project: https://github.com/tinydesk-project/tinydesk
- Commit: `feaf84130f03594845e5e9284817dca785b4d878` (v0.1.5)
- License: MIT

TinyTang carries `third_party/patches/tinydesk/0001-settings-bluetooth-button.patch`
against that commit, applied idempotently by `scripts/apply-tinydesk-patches.sh`
during CMake configuration. It changes Settings' Network button, whose app this
port leaves out, into a Bluetooth button that launches TinyTang's own Bluetooth
window (`ports/bl616/td_bluetooth_app.c`), and moves the Date & time button
two columns right to make room. The patch is MIT, as is the upstream code it
modifies.

## nestang core UART protocol (read as a specification, no code copied)

The frame format and command set implemented by `ports/bl616/tang_fpga_uart.c`
and `tang_osd.c` are nand2mario's, documented in comments
above the nestang receiver:

- Project: https://github.com/nand2mario/nestang
- File read: `src/iosys/iosys_bl616.v`
- License: GPL-3.0

No nestang code is copied into this repository. The protocol was read as a
specification and the BL616 side written independently against it. The readings
are recorded as `PROT-001` and `PROT-002`.

## nestang core patch set (vendored modifications, carried as patches)

The FPGA core this project runs is nand2mario's nestang, modified in place and
carried as patches rather than as a fork, so the checkout underneath stays
reconstructible from upstream:

- Project: https://github.com/nand2mario/nestang
- Base commit: `c2450818e1f0c858e13c5dd16746ee5221a5c760` ("upgrade iosys")
- License: GPL-3.0 (`COPYING` at that commit)
- Patches: `third_party/patches/0001` through `0007`, applied by
  `scripts/apply-nestang-patches.sh`

The patches are modifications to GPL-3.0 sources, so the patch files reproduce
lines of GPL-3.0 code and that licence governs those lines. Two files they add
are this project's own work and say so in their headers: `src/keylink_rx.sv`
(the keyboard-link receiver) is marked `SPDX-License-Identifier: GPL-3.0-only`,
which is the licence a addition to a GPL-3.0 core should carry. `0004` also
removes `src/usb_hid_host.v` and its ROM image from the build: the low-speed USB
hosts they implemented are displaced on both front ports by the keyboard link,
so the module is no longer instantiated anywhere.

The menu variant is carried in `third_party/patches/menu/`, and the desktop
host variant in `third_party/patches/desktop/`. These are also modifications
to GPL-3.0 sources. The new HDL, board definition and constraints in
`fpga/desktop/`, and its simulation benches in `tools/tests/sim/tb_desktop_*`,
are GPL-3.0-only. Except for the OLED panel/SPI modules identified below, they
are TinyTang's work. Build scripts and timing-validation
tools retain their explicit MIT notices. The desktop binary links upstream
nestang and HDMI modules and is governed by GPL-3.0; the BL616 firmware remains
separate and communicates through the documented UART protocol.

The desktop handshake design uses synchronization and held-data conventions
reviewed in CERN's colibri library as engineering guidance. No colibri module
was copied, translated or adapted into the desktop core.

## Keychron QMK firmware (Tang keyboard link, carried as patches)

The keyboard end of the link is a Keychron K2 HE running a modified QMK, built
from Keychron's fork of QMK and not vendored here — only the changes are:

- Project: https://github.com/Keychron/qmk_firmware
- Branch: `2025q3`, base commit `c5d998442710b38650903937249144a0cd728ebe`
- License: GPL-2.0 (`LICENSE`); individual files carry their own notices, and
  `tmk_core/protocol/chibios/usb_main.c` is marked
  `SPDX-License-Identifier: GPL-3.0-or-later OR Apache-2.0`. The Keychron board
  files these patches modify descend from Keychron's GPL-3.0-only tree
- Patches: `third_party/patches/qmk/`, applied by
  `scripts/apply-qmk-tang-patches.sh`
- Added files: `keyboards/keychron/k2_he/ansi/keymaps/tang/`, written for this
  project and marked `SPDX-License-Identifier: GPL-3.0-only`

As above, the patch files reproduce lines of GPL-licensed sources and those
lines keep their licence. Nothing of QMK is built into this repository or into
this firmware: the patches exist so the keyboard firmware can be rebuilt, and
the resulting image runs on the keyboard, not on the Tang. The keyboard is
not a USB keyboard while that image is on it, which the patches' README states
along with both ways back to stock behaviour.

## Bouffalo SDK

The vendor SDK providing chip support, FatFS and the USB stack:

- License: Apache-2.0 for the SDK's own code

The SDK bundles third-party components under their own licences, and the ones
this firmware links are named here rather than folded into the line above —
because a bundle's licence does not cover what is bundled inside it:

- **FreeRTOS Kernel V10.4.6** — MIT — Copyright (C) 2021 Amazon.com, Inc. or
  its affiliates. Selected by `CONFIG_FREERTOS` in `proj.conf`; it is the
  scheduler the shell's worker primitive runs on. MIT requires the notice and
  its permission text to travel with copies and with substantial portions of
  the software, which is what this firmware's binary is
- **CherryUSB** — Apache-2.0 — Copyright (C) 2006 Bertrik Sikken, (c) 2016
  Intel Corporation, (c) 2022, sakumisu. This is the device CDC console the
  user sees, and with it the FreeRTOS OSAL that hosts it
  (`osal/usb_osal_freertos.c`)
- **FatFs R0.15 w/patch3** — Copyright (C) 2022, ChaN. Its condition is
  narrower than the two above: a redistribution of *source* must retain the
  notice and its condition. This project ships no FatFS source — it builds
  against the SDK's — but the filesystem the shell sees is FatFS

### Bluetooth LE

`blescan` and `blekbd` (`ports/bl616/tang_ble.c`) link the SDK's Bluetooth
stack. It comes in three parts, and two of them carry licences the line above
does not cover:

- **Bluetooth host (`blestack`)** — Apache-2.0, derived from Zephyr's
  Bluetooth host. The linked files carry `SPDX-License-Identifier: Apache-2.0`
  and these copyrights: (c) 2015-2018 Intel Corporation; (c) 2016-2017 Nordic
  Semiconductor ASA; (c) 2011-2014, 2016-2017 Wind River Systems, Inc.;
  (c) 2019 Oticon A/S; (c) 2016 Vinayak Kariappa Chettimada. Bouffalo's own
  port files (Copyright (C) Bouffalo Lab 2018, 2019) carry no licence header
  and fall under the SDK's Apache-2.0. The licence text is at
  `LICENSES/Apache-2.0.txt`
- **TinyCrypt and micro-ecc, inside the host** — BSD. This is the Bluetooth
  host's cryptography for pairing (AES, CCM, CMAC, HMAC, SHA-256, the PRNGs and
  P-256 ECDH). Each of these licences requires a distribution *in binary form*
  to reproduce its copyright notice, conditions and disclaimer in the
  documentation, and this firmware's image is such a distribution. The texts
  are reproduced verbatim from the SDK's source:
  - Copyright (C) 2017 by Intel Corporation, All Rights Reserved — BSD-3-Clause,
    `LICENSES/BSD-3-Clause-tinycrypt.txt`
  - Copyright (c) 2014, Kenneth MacKay (micro-ecc, in `ecc.c`, `ecc_dh.c`,
    `ecc_dsa.c` and `ecc_platform_specific.c`) — BSD-2-Clause,
    `LICENSES/BSD-2-Clause-micro-ecc.txt`
  - Copyright (c) 2016, Chris Morrison (`ctr_prng.c`) — BSD-2-Clause,
    `LICENSES/BSD-2-Clause-tinycrypt-ctr_prng.txt`
- **Bluetooth controller and radio** — supplied as prebuilt binaries only:
  `libbtblecontroller_bl616_ble1m10s1bredr0.a`, a controller based on CEVA's
  RivieraWaves IP (its port file `btblecontroller_port_uart.c` reads
  "Copyright (C) RivieraWaves 2009-2015"), and the PHY/RF libraries
  `libbl616_phyrf.a` and `librfparam.a`. None carries a licence of its own,
  so the SDK's root Apache-2.0 is the only grant that covers redistributing
  them. This project ships none of them; they are linked from the SDK at
  build time

Not linked into this firmware, and therefore not named: the SDK's LVGL,
TJpgDec, mbedTLS, littlefs and multimedia codecs.

## Bouffalo SDK device example (`ref/`)

`ref/` holds the SDK's own CDC ACM device example, kept as the control in the
bisection that produced `TOOL-001`, together with this project's FreeRTOS
variant of it:

- `ref/cdc_acm_template.c` — byte-identical to the SDK's
  `examples/peripherals/usbdev/usbd_cdc_acm/cdc_acm_template.c`
- `ref/ref_main.c` — byte-identical to that example's `main.c`
- `ref/ref_rtos_main.c` — this project's variant of the same main, with
  FreeRTOS running, answering whether FreeRTOS alone stops enumeration

- License: Apache-2.0, from the SDK's root `LICENSE`. The SDK's example files
  carry no per-file licence header of their own, and neither do these copies
  — deliberately, because byte-identity with the SDK is what makes them a
  control. Do not add headers to the two unmodified files.

`usb_config.h` at this project's root is the SDK's file, modified, and retains
its `SPDX-License-Identifier: Apache-2.0` and its `Copyright (c) 2022,
sakumisu`. The `ref/` files are selected only by the `TINYTANG_REF` and
`TINYTANG_RTOS` configurations (`TOOL-005`).

## colibri (CERN) — reference consulted, no code copied

CERN's vendor-independent, fully verified, open-source VHDL common library,
and an unofficial SystemVerilog port of it. Kept here as a design and
verification reference for work on this project.

- Original, and the authority: https://gitlab.cern.ch/colibri/colibri
  (mirror: https://gitlab.com/colibri-cern/colibri)
- SystemVerilog port, and the practical surface here:
  https://github.com/kavierim/colibri-sv — unofficial, not endorsed by CERN,
  and pinned to the original at commit
  `3fa784121ccea86d9e65b2e0dc08d2a3327f5f2f`
- License: CERN-OHL-W-2.0, weakly reciprocal

Read the port when the work is in SystemVerilog. It targets Verilator and free
EDA tooling, which is what this family builds with, and it is the only one of
the two written for agents at all: it ships an `AGENTS.md`, a `CONVENTIONS.md`,
docs with a module template and playbooks, testbenches, formal material and a
SysML v2 requirements model. Treat the VHDL original as the authority on intent
and currency — the port is unofficial and pinned, so it lags upstream and
carries one translator's choices, and it is new and small.

No colibri code is copied into this repository. The reciprocity applies from
the moment any of it is, including from the port, which is itself Covered
Source modified under section 3 of the licence.

## Sipeed board documentation (schematics, not this project's to license)

`docs/` **carried** Sipeed's schematics for the board this firmware runs on, and
they were removed from this repository on 2026-10-10. They were kept because a
port needs them -- the FPGA banks, the PMOD sockets, the HDMI level shifters and
the I2C the clock tree hangs on are only legible from these sheets, and reading
them is what produced this project's pin and bank records -- but they were
tracked in a public repository under terms that are Sipeed's and unstated.

- `docs/Tang_Mega_138K_Console_32001C__Schematics.pdf` -- the dock and the SOM,
  board revision 32001C, 17 sheets
- `docs/tang_mega_138k_30354_Schematics..pdf` -- the Tang Mega 138K board, 7
  sheets (the doubled dot in the filename is Sipeed's)

- Source: Sipeed's download share for the Tang Console,
  https://dl.sipeed.com/shareURL/Tang/Console, which mirrors to Baidu and to
  MEGA. The same bundle holds the board's KiCad project and its Gerbers.
- Terms: **Sipeed's own, and unstated.** The bundle carries no licence for the
  hardware documents, so nothing there is permission to redistribute them, and
  they are **not** covered by this project's MIT licence. The records that cite
these sheets name the sheet and the revision so that anyone holding a copy can
  re-check them. Do not bring material from that bundle into this repository --
  the Gowin toolchain, the vendor datasheets and the example bitstreams in
  particular.

## Board photographs (this project's own work, removed)

`images/circuit_boards/` carried seven photographs of the dock, the FPGA module
and the SDRAM module, taken by the project's author on 2026-10-05, and cited by
`.ai/core-reference.md` as the record of the dock's two edge headers. They were
this project's own work and owed no licence to anyone; they were removed from
this repository on 2026-10-10 because they carried EXIF location data and the
repository is public. Anything of the kind added later should have its EXIF
stripped first.

## Tang-Phosphor OLED panel engine

The desktop OLED terminal reuses the user's GPL-3.0-only panel engine from
https://github.com/aquasock/Tang-Phosphor at commit `7cf9ede`.
`fpga/desktop/oled_spi.sv` retains `src/oled/oled_spi.sv` unchanged
apart from the provenance comment. `fpga/desktop/oled_panel.sv` derives from
`src/oled/oled_panel.sv`, adding a complete RGB565 pixel latch and
its sampled-pixel output so live text updates cannot split two pixel bytes.
The source was taken from the committed files, leaving that repository's
working changes untouched. The compact font and desktop text renderer are
original TinyTang work under GPL-3.0-only.

`ports/bl616/oled_vterm.c` separately compiles TinyDesk's MIT `src/vterm.c`
with private 24×16 geometry and distinct symbols; its UTF-8 display adapter
substitutes the OLED fallback glyph without altering shell input or files.
The existing TinyDesk MIT attribution above also covers this instance.
