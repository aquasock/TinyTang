#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The /tinysnes.ini parser on the host (tools/tests/tb_snes_ini.c): the identity
# defaults, several pairs on one line, `r` against `rt`, `none`, player 2 kept
# apart from player 1, and the word frame 0x1A carries.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cc -std=c11 -O1 -D_GNU_SOURCE -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/tools/tests/stubs/tang_fpga_uart" \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_snes_ini.c" \
   "$root/ports/bl616/tang_snes_ini.c" \
   "$root/ports/bl616/tang_settings_ini.c" -o "$work/tb"
"$work/tb"
