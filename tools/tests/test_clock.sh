#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The desktop's wall clock on the host (tools/tests/tb_clock.c).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -DTANG_CLOCK_HOST \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_clock.c" "$root/ports/bl616/tang_clock.c" -o "$work/tb"
"$work/tb"
