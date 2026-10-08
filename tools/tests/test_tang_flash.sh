#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# tangput's route refusal and tangload's single programmer
# (tools/tests/tb_tang_flash.c), with FreeRTOS replaced by POSIX threads.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

stubs="$root/tools/tests/stubs"
cc -std=gnu11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter \
   -DTDSH_MAX_VARS=48 -DTDSH_VAR_NAME_MAX=32 -DTDSH_VAR_VALUE_MAX=128 \
   -I "$stubs/rtos_threads" -I "$stubs/tang_flash" -I "$root/ports/bl616" \
   -I "$root/third_party/tinydesk-shell/include" \
   "$root/tools/tests/tb_tang_flash.c" "$stubs/rtos_threads/rtos_threads.c" \
   -lpthread -o "$work/tb" 2>"$work/cc.log" || { cat "$work/cc.log"; exit 1; }
timeout 30 "$work/tb"
