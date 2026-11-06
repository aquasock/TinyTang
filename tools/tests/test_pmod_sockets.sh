#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# What a core load does to the I2C bus belief, on the real
# ports/bl616/phosphor/pmod_sockets.cpp (tools/tests/tb_pmod_sockets.cpp).
# The FPGA is stubbed at the link frames and fpga_debug_request; tang_ini.c is
# linked for real, so the socket word the module builds is the one it ships.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

stubs="$root/tools/tests/stubs"
cc="${CC:-cc}"
cxx="${CXX:-c++}"
# The shell's real tdsh.h must win over the empty stand-in in stubs/, which
# exists for modules that only declare what they need; pmod_sockets.cpp
# registers a command, so it needs the real types.
incs=(-I "$root/third_party/tinydesk-shell/include" -I "$stubs"
      -I "$root/ports/bl616" -I "$root/ports/bl616/phosphor"
      -DTDSH_MAX_VARS=48 -DTDSH_VAR_NAME_MAX=32 -DTDSH_VAR_VALUE_MAX=128)

# The module is C++ and the parser is C, so each is compiled by its own
# compiler and the two objects are linked -- one command line cannot carry a
# language switch across three files.
"$cxx" -std=gnu++17 -O1 -Wall -Wextra -Werror -Wno-unused-parameter "${incs[@]}" \
    -c "$root/ports/bl616/phosphor/pmod_sockets.cpp" -o "$work/pmod.o" \
    2>"$work/cc.log" || { cat "$work/cc.log"; exit 1; }
"$cxx" -std=gnu++17 -O1 -Wall -Wextra -Werror -Wno-unused-parameter "${incs[@]}" \
    -c "$root/tools/tests/tb_pmod_sockets.cpp" -o "$work/tb.o" \
    2>>"$work/cc.log" || { cat "$work/cc.log"; exit 1; }
"$cc" -std=gnu11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter "${incs[@]}" \
    -c "$root/ports/bl616/tang_ini.c" -o "$work/ini.o" \
    2>>"$work/cc.log" || { cat "$work/cc.log"; exit 1; }

"$cxx" "$work/tb.o" "$work/pmod.o" "$work/ini.o" -o "$work/tb"
timeout 30 "$work/tb"
