#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The OLED cell link against a scripted core (tools/tests/tb_oled_link.cpp),
# with FreeRTOS replaced by POSIX threads (tools/tests/stubs/rtos_threads).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

stubs="$root/tools/tests/stubs/rtos_threads"
cc -std=gnu11 -O1 -Wall -Wextra -Werror -I "$stubs" -c "$stubs/rtos_threads.c" -o "$work/rtos.o"
c++ -std=gnu++17 -O1 -Wall -Wextra -Werror \
    -I "$stubs" -I "$root/ports/bl616" -I "$root/ports/bl616/phosphor" \
    "$root/tools/tests/tb_oled_link.cpp" "$root/ports/bl616/phosphor/oled_link.cpp" \
    "$work/rtos.o" -Wl,--wrap=calloc -lpthread -o "$work/tb"
"$work/tb"

# The link's buffers come from the heap when the terminal starts; a large
# static buffer would cost every board that never opens it (entry 68).
c++ -std=gnu++17 -O1 -c -I "$stubs" -I "$root/ports/bl616" -I "$root/ports/bl616/phosphor" \
    "$root/ports/bl616/phosphor/oled_link.cpp" -o "$work/oled_link.o"
large="$(nm -S -t d "$work/oled_link.o" | awk 'NF == 4 && $3 ~ /^[bBdD]$/ && $2 + 0 > 256')"
[[ -z "$large" ]] || { echo "test_oled_link: large static data:"; echo "$large"; exit 1; }
echo "test_oled_link: no static buffer over 256 bytes"
