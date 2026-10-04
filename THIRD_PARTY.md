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

- License: Apache-2.0

## colibri (CERN) — reference consulted, no code copied

CERN's vendor-independent, fully verified, open-source VHDL common library,
and an unofficial SystemVerilog port of it. Kept here as a design and
verification reference for work on this project.

- Project: https://gitlab.cern.ch/colibri/colibri
  (mirror: https://gitlab.com/colibri-cern/colibri)
- SystemVerilog port: https://github.com/kavierim/colibri-sv (unofficial, not
  endorsed by CERN)
- License: CERN-OHL-W-2.0, weakly reciprocal

No colibri code is copied into this repository. Its reciprocity applies from
the moment any of it is, so read the licence before reusing a module here.
