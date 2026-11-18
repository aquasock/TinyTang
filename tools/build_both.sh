#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Build both firmware variants and check each is what it should be.
#
#   tools/build_both.sh
#
# defconfig builds the BR/EDR controller library, so the default image is the
# Classic one (BLE-023); the LE-only build is selected on the make command
# line.  Nothing builds that second variant otherwise, so code under
# CONFIG_BT_BREDR can break it unnoticed -- run this before committing a
# Bluetooth change.  It is not part of tools/tests/run_all.sh because a build
# takes minutes and needs the SDK and toolchain.
#
# Both build in parallel, into build/ (default) and build-leonly/.  It fails
# when either build fails, when the Classic host is missing from the default
# image or present in the LE-only one, or when the project's own sources warn
# (the vendored JTAG programmer's known warnings excepted).  Each image's size
# and static RAM (tools/ram_report.py) are printed.
set -uo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
logs="$(mktemp -d)"
trap 'rm -rf "$logs"' EXIT

make CHIP=bl616 BOARD=bl616dk >"$logs/default.log" 2>&1 &
p1=$!
# Staggered, so the two configure steps do not apply the TinyDesk patches to
# the shared submodule at the same moment.
sleep 15
make CHIP=bl616 BOARD=bl616dk BUILD_DIR=build-leonly \
     CONFIG_BTBLECONTROLLER_LIB=ble1m10s1bredr0 CONFIG_BT_BREDR=n >"$logs/leonly.log" 2>&1 &
p2=$!
wait $p1; r1=$?
wait $p2; r2=$?

fail=0
report() {   # name, build dir, make status, log, whether the Classic host belongs
    local name="$1" dir="$2" rc="$3" log="$4" classic="$5"
    if ((rc != 0)); then
        echo "build_both: $name: build FAILED (make exit $rc)"
        grep -E "error:|undefined reference" "$log" | sed 's/\x1b\[[0-9;]*[mK]//g' | head -10
        fail=1
        return
    fi
    local bin map size ram has
    bin="$dir/build_out/tinytang_bl616.bin"     # not boot2's, which sorts first
    map="$dir/build_out/tinytang_bl616.map"
    size=$(stat -c %s "$bin")
    ram=$(python3 tools/ram_report.py "$map" --top 0 | head -1 | sed 's/static RAM \([0-9]*\) bytes.*/\1/')
    has=0
    grep -q "tang_btpad_cmd" "$map" && has=1
    echo "build_both: $name: $size bytes, static RAM $ram bytes, Classic host $([ $has = 1 ] && echo in || echo out)"
    if [[ $has != "$classic" ]]; then
        echo "build_both: $name: the Classic host should be $([ "$classic" = 1 ] && echo in || echo out)"
        fail=1
    fi
    local warn
    warn="$(sed 's/\x1b\[[0-9;]*[mK]//g' "$log" | grep -E "/(ports|tools)/[^ ]+\.(c|cpp|h):[0-9]+:[0-9]+: warning:" \
            | grep -v "tang_jtag_programmer.c" | sort -u)"
    if [[ -n $warn ]]; then
        echo "build_both: $name: warnings in the project's sources:"
        echo "$warn" | head -10
        fail=1
    fi
}
report default build "$r1" "$logs/default.log" 1
report LE-only build-leonly "$r2" "$logs/leonly.log" 0
exit $fail
