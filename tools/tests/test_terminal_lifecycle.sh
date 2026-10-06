#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=c11 -O1 -Wall -Wextra -Wno-unused-parameter \
   -ffunction-sections -fdata-sections -Wl,--gc-sections \
   -I "$root/tools/tests/stubs" \
   -include "$root/tools/tests/stubs/terminal_lifecycle.h" \
   -I "$root/ports/bl616" -I "$root/third_party/tinydesk/include" \
   -I "$root/third_party/tinydesk/apps" \
   -I "$root/third_party/tinydesk-shell/include" \
   "$root/tools/tests/tb_terminal_lifecycle.c" \
   "$root/ports/bl616/td_bridge_bl616.c" \
   "$root/third_party/tinydesk/apps/terminal.c" \
   -o "$work/tb"
"$work/tb"
