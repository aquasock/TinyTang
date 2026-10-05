# Third-party components

TinyTang's own code is MIT; the text is in `LICENSE`. One vendored file is
Apache-2.0, and the entry for it below says so — where that happens, that
file's licence governs it and MIT governs everything else. Reproducing both
licences is deliberate: an Apache-2.0 file inside an MIT project keeps its own
terms, and the pointer in its header has to resolve to something true.

The components below are inherited, vendored, or consulted. Where a source was
read as a specification rather than copied, that is said plainly, because that
distinction is what decides which obligations attach. The same facts are kept
as records in `.ai/core-reference.md` (`PROV`, `TOOL-007`, `TDSH-001`,
`TDESK-001`, `PROT-001`).

## TangCore firmware (nand2mario)

The board knowledge this firmware was built on, and the lineage it descends
from:

- Project: https://github.com/nand2mario/tangcore
- Author: nand2mario and contributors
- License: Apache-2.0 for the firmware (`firmware-bl616`). Each core under the
  same repository keeps its own licence; no core is used here.

TinyTang started from Tang-Control's board layer, which is itself a fork of
TangCore's `firmware-bl616`. Every fact taken from it is called out in the
source where it is used and recorded in `.ai/core-reference.md`.

## Tang-Control

The intermediate project, retired in favour of TinyTang:

- Project: https://github.com/aquasock/Tang-Control
- License: Apache-2.0

The Tang-Phosphor host side is ported from Tang-Control into
`ports/bl616/phosphor/`: `fpga_ext_frame.h` and `flac_stream_prefix.h` copied
unchanged, and `fpga_debug`, `fpga_stream` and `fpga_file_stream` ported with
only their link plumbing changed, each saying so in its header. The host tests
`tools/tests/fpga_ext_frame_test.cpp` and `flac_stream_prefix_test.cpp` are
copied unchanged. All are Apache-2.0, as this project is.

## Gowin JTAG programmer (nand2mario)

`ports/bl616/tang_jtag_programmer.c` is nand2mario's bit-banged GPIO JTAG
programmer for Gowin GW5A and GW2A, taken from Tang-Control's
`fpga/programmer.cpp`. Used unmodified apart from its include list and one
added comment saying where its licence text lives in this tree;
`ports/bl616/tang_jtag_glue.h` supplies what it expected from its own tree and
documents this provenance.

- Copyright: (c) 2025.2, nand2mario
- License: Apache-2.0, reproduced at `LICENSES/Apache-2.0.txt`. This is the only
  file in the tree under a licence other than MIT
- Based in part on openFPGALoader by Gwenhael Goavec-Merou,
  https://github.com/trabucayre/openFPGALoader, also Apache-2.0

## TinyDesk Shell

The shell this firmware runs, consumed as `third_party/tinydesk-shell` and
compiled unchanged:

- Project: https://github.com/schikani/tinydesk-shell
- Commit: `232a39fa3375f2c8eb2560cdc69440f7095f8a25` (v0.1.3)
- License: MIT

## TinyDesk

The desktop environment, consumed as `third_party/tinydesk`:

- Project: https://github.com/schikani/tinydesk
- Commit: `791cad83dc49a5901300438ed4af8eaf73e4b265` (v0.1.3-1)
- License: MIT

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
