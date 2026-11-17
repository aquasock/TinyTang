#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# TinyDesk's Editor built with TinyTang's limits (tools/tests/tb_editor_limits.c).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# The same limits CMakeLists.txt gives the firmware.
limits="$(sed -n 's/.*\(-DTD_EDITOR_MAX=[0-9]*\) \(-DTD_EDITOR_UNDO=[0-9]*\).*/\1 \2/p' "$root/CMakeLists.txt")"
[[ -n "$limits" ]] || { echo "test_editor_limits: limits not found in CMakeLists.txt" >&2; exit 1; }

# The apps the firmware builds (CMakeLists.txt), for what the Editor calls.
# The firmware's own TinyDesk app list, read from CMakeLists.txt so this test
# links what the image links and cannot fall behind it (TinyDesk 0.5's Editor
# needs its Python lexer and Settings the on-screen keyboard, for instance).
apps=""
for a in $(sed -n 's|^ *third_party/tinydesk/apps/\([a-z_]*\)\.c)\?$|\1|p' "$root/CMakeLists.txt"); do
    apps+=" $root/third_party/tinydesk/apps/$a.c"
done
[[ -n "$apps" ]] || { echo "test_editor_limits: no app list in CMakeLists.txt" >&2; exit 1; }

# shellcheck disable=SC2086
cc -std=gnu11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter $limits \
   -DTD_MAX_COLS=80 -DTD_MAX_ROWS=45 -DTD_VT_SCROLLBACK=12 -DTD_MAX_WIDGETS=96 \
   -I "$root/third_party/tinydesk/include" -I "$root/third_party/tinydesk/apps" \
   -I "$root/third_party/tinydesk/tests" \
   "$root/tools/tests/tb_editor_limits.c" \
   -ffunction-sections -fdata-sections -Wl,--gc-sections \
   $apps \
   "$root/third_party/tinydesk/src/"*.c \
   -o "$work/tb" 2>"$work/cc.log" || { cat "$work/cc.log"; exit 1; }
"$work/tb"
