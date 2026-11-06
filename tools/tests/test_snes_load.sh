#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The SNES cartridge loader on the host (tools/tests/tb_snes_load.c): header
# choice, the copier header, the frame order the TinySNES core needs, the
# refusals that send nothing, and the battery save in both directions -- the
# 0x18 blocks a load restores before the reset is released, and the 0x19
# readout an exit writes, verifies and renames.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cc -std=c11 -O1 -D_GNU_SOURCE -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/tools/tests/stubs/tang_fpga_uart" \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_snes_load.c" \
   "$root/ports/bl616/tang_snes_load.c" \
   "$root/ports/bl616/tang_snes_ini.c" \
   "$root/ports/bl616/tang_settings_ini.c" \
   "$root/ports/bl616/ae350_load.c" -o "$work/tb"
"$work/tb"
