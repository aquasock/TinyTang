#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The heap wrappers on the host (tools/tests/tb_tang_heap.c).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/tools/tests/stubs/tang_heap" \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_tang_heap.c" -o "$work/tb"
"$work/tb"
