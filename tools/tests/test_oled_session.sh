#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The OLED terminal's shell session on real threads (tools/tests/tb_oled_session.c):
# the real app, terminal routes, script workers and printf layer, with
# FreeRTOS replaced by POSIX threads (tools/tests/stubs/rtos_threads).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

stubs="$root/tools/tests/stubs/rtos_threads"
cc -std=gnu11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter \
   -ffunction-sections -fdata-sections -Wl,--gc-sections \
   -DTD_MAX_COLS=80 -DTD_MAX_ROWS=45 -DTD_VT_SCROLLBACK=12 -DTD_MAX_WIDGETS=96 \
   -DTDSH_MAX_VARS=48 -DTDSH_VAR_NAME_MAX=32 -DTDSH_VAR_VALUE_MAX=128 \
   -DTDSH_SCRIPT_TASK_STACK=16384 \
   -I "$stubs" -I "$root/ports/bl616" \
   -I "$root/third_party/tinydesk-shell/include" \
   -I "$root/third_party/tinydesk/include" -I "$root/third_party/tinydesk/apps" \
   "$root/tools/tests/tb_oled_session.c" \
   "$root/ports/bl616/tang_oled.c" "$root/ports/bl616/oled_vterm.c" \
   "$root/ports/bl616/tdsh_platform_bl616.c" \
   "$root/ports/bl616/tdsh_console_stdio_bl616.c" \
   "$stubs/rtos_threads.c" \
   "$root/third_party/tinydesk/src/"*.c \
   -lpthread -o "$work/tb" 2>"$work/cc.log" || { cat "$work/cc.log"; exit 1; }
timeout 60 "$work/tb"

# The terminal's emulator and input ring come from the heap when it starts.
cc -std=gnu11 -O1 -c -DTD_MAX_COLS=80 -DTD_MAX_ROWS=45 -DTD_VT_SCROLLBACK=12 \
   -DTDSH_MAX_VARS=48 -DTDSH_VAR_NAME_MAX=32 -DTDSH_VAR_VALUE_MAX=128 \
   -I "$stubs" -I "$root/ports/bl616" -I "$root/third_party/tinydesk-shell/include" \
   -I "$root/third_party/tinydesk/include" "$root/ports/bl616/tang_oled.c" -o "$work/tang_oled.o"
large="$(nm -S -t d "$work/tang_oled.o" | awk 'NF == 4 && $3 ~ /^[bBdD]$/ && $2 + 0 > 256')"
[[ -z "$large" ]] || { echo "test_oled_session: large static data:"; echo "$large"; exit 1; }
echo "test_oled_session: no static buffer over 256 bytes"
