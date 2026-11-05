#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The FatFS syscall layer on the host (tools/tests/tb_tdsh_fs.c).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=c11 -O1 -D_GNU_SOURCE -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/tools/tests/stubs/tdsh_fs" \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_tdsh_fs.c" -o "$work/tb"
"$work/tb"
