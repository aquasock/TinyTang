#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The crash recorder on the host (tools/tests/tb_tang_crash.c), built with the
# switch that replaces four RISC-V-only instructions.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Two warnings are host artefacts, not module defects, and are turned off
# here rather than in the module:
#   pointer-to-int-cast  the target is 32-bit and a return address fits a
#                        uint32_t; on x86-64 it does not, which is not a fault
#                        in firmware built for the board.
#   format-truncation    glibc bounds `build[40]` into a 120-byte line and can
#                        prove a possible overlap; newlib, which the board
#                        builds against, neither warns nor truncates here.
cc -std=c11 -O1 -D_GNU_SOURCE -DTINYTANG_HOST_TEST \
   -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-format-truncation \
   -fsanitize=address,undefined \
   -I "$root/tools/tests/stubs/tang_crash" \
   -I "$root/third_party/tinydesk-shell/include" \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_tang_crash.c" -o "$work/tb"
"$work/tb"
