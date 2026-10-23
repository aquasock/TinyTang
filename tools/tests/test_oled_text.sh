#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# `oled print`'s cell layout (ports/bl616/oled_text.c) on the host.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I "$root/ports/bl616" \
    "$root/tools/tests/tb_oled_text.c" "$root/ports/bl616/oled_text.c" -o "$work/test"
"$work/test"
