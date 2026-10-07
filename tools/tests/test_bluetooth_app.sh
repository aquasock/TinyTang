#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The Bluetooth window against the real TinyDesk core, with the Bluetooth
# engine replaced by a recorder (tools/tests/tb_bluetooth_app.c).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=gnu11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter \
   -DTD_MAX_COLS=80 -DTD_MAX_ROWS=45 -DTD_VT_SCROLLBACK=12 -DTD_MAX_WIDGETS=96 \
   -I "$root/ports/bl616" -I "$root/third_party/tinydesk/include" \
   -I "$root/third_party/tinydesk/apps" \
   "$root/tools/tests/tb_bluetooth_app.c" \
   "$root/ports/bl616/td_bluetooth_app.c" \
   "$root/third_party/tinydesk/src/"*.c \
   -o "$work/tb" 2>"$work/cc.log" || { cat "$work/cc.log"; exit 1; }
"$work/tb"
