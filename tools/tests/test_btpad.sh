#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Classic Bluetooth pads on the host: the pairings file and the Switch report
# decoder (tools/tests/tb_btpad.c, ports/bl616/tang_btpad_codec.c).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cc -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_btpad.c" "$root/ports/bl616/tang_btpad_codec.c" \
   "$root/ports/bl616/tang_ble_bonds.c" -o "$work/tb"
"$work/tb"
