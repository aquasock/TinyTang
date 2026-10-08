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
    "$work/rtos.o" -lpthread -o "$work/tb"
"$work/tb"
