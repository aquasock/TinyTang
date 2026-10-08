#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cc -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I "$root/ports/bl616" -I "$root/third_party/tinydesk/include" \
    "$root/tools/tests/oled/test_oled_vterm.c" "$root/ports/bl616/oled_vterm.c" \
    "$root/third_party/tinydesk/src/utf8.c" -Wl,--gc-sections -o "$work/test"
"$work/test"
