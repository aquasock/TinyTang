#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The Classic gamepad host (ports/bl616/tang_btpad.c) against a fake
# Bluetooth stack that counts references as the SDK does and plays the event
# orders the board saw (tools/tests/tb_btpad_host.c, BLE-021, BLE-023).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cc -std=c11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function \
   -fsanitize=address,undefined -DCONFIG_BT_BREDR \
   -I "$root/tools/tests/stubs/btpad" -I "$root/tools/tests/stubs" -I "$root/ports/bl616" \
   "$root/tools/tests/tb_btpad_host.c" "$root/ports/bl616/tang_btpad_codec.c" \
   "$root/ports/bl616/tang_ble_bonds.c" \
   "$root/ports/bl616/tang_pad_stick.c" -o "$work/tb"
"$work/tb"
