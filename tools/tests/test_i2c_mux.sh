#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The PCA9548A channel map on the host (tools/tests/tb_i2c_mux.c).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_i2c_mux.c" "$root/ports/bl616/tang_i2c_mux.c" -o "$work/tb"
"$work/tb"
