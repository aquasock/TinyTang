#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Run the host test for the keyboard-to-typing translation.
#
#   tools/tests/test_key.sh
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v cc >/dev/null 2>&1; then
    echo "test_key: no C compiler" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=c11 -O1 -Wall -Wextra -Wno-unused-parameter \
   -I "$root/ports/bl616" \
   -o "$work/tb" \
   "$root/tools/tests/tb_key.c" \
   "$root/ports/bl616/tang_key.c"

"$work/tb"
