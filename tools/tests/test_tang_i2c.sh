#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The I2C command surface on the host, against a simulated register window
# (tools/tests/tb_tang_i2c.cpp).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

c++ -std=gnu++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/tools/tests/stubs/tang_i2c" \
   -I "$root/ports/bl616" \
   -I "$root/ports/bl616/phosphor" \
   "$root/tools/tests/tb_tang_i2c.cpp" \
   "$root/ports/bl616/tang_i2c_mux.c" -o "$work/tb"
"$work/tb"
