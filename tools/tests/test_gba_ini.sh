#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The /tinygba.ini parser and writer on the host (tools/tests/tb_gba_ini.c): the
# defaults the core holds out of reset, several pairs on one line, `l` against
# `left`, `none`, the bytes frame 0x1A carries, and a save round trip.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cc -std=c11 -O1 -D_GNU_SOURCE -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/tools/tests/stubs/tang_fpga_uart" \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_gba_ini.c" \
   "$root/ports/bl616/tang_gba_ini.c" \
   "$root/ports/bl616/tang_settings_ini.c" -o "$work/tb"
"$work/tb"
