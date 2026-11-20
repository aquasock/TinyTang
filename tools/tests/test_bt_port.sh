#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The port's Bluetooth tables (ports/bl616/tang_bt_port.c) over recorders of
# tang_ble.c and tang_btpad.c: the pad table's translation for the Bluetooth
# window (tools/tests/tb_bt_port.c).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cc -std=c11 -O1 -Wall -Wextra -Werror -Wno-format-truncation -fsanitize=address,undefined \
   -DTD_MAX_COLS=80 -DTD_MAX_ROWS=45 \
   -I "$root/ports/bl616" -I "$root/third_party/tinydesk/include" \
   "$root/tools/tests/tb_bt_port.c" "$root/ports/bl616/tang_bt_port.c" -o "$work/tb"
"$work/tb"
