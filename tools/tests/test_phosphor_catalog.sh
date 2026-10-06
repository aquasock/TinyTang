#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
c++ -std=c++17 -O1 -Wall -Wextra -Wno-unused-function \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I "$root/tools/tests/stubs/phosphor_catalog" \
    -I "$root/ports/bl616/phosphor" \
    -I "$root/third_party/tinydesk/include" -I "$root/third_party/tinydesk/apps" \
    "$root/tools/tests/phosphor_catalog_test.cpp" -o "$work/test"
"$work/test"
