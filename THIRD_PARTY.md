# Third-party components

TinyTang is licensed under Apache-2.0; the text is in `LICENSE`.

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

## Gowin JTAG programmer (nand2mario)

`ports/bl616/tang_jtag_programmer.c` is nand2mario's bit-banged GPIO JTAG
programmer for Gowin GW5A and GW2A, taken from Tang-Control's
`fpga/programmer.cpp`. Used unmodified apart from its include list;
`ports/bl616/tang_jtag_glue.h` supplies what it expected from its own tree and
documents this provenance.

- Copyright: (c) 2025.2, nand2mario
- License: Apache-2.0
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

The frame format and command set implemented by `ports/bl616/tang_fpga_uart.c`,
`tang_osd.c` and `tang_osd_term.c` are nand2mario's, documented in comments
above the nestang receiver:

- Project: https://github.com/nand2mario/nestang
- File read: `src/iosys/iosys_bl616.v`
- License: GPL-3.0

No nestang code is copied into this repository. The protocol was read as a
specification and the BL616 side written independently against it. The readings
are recorded as `PROT-001` and `PROT-002`.

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
