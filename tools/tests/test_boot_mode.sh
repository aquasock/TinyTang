#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# What the board comes up as, on the host (tools/tests/tb_boot_mode.c).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=c11 -O1 -D_GNU_SOURCE -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_boot_mode.c" \
   "$root/ports/bl616/tang_boot_mode.c" -o "$work/tb"
"$work/tb"
