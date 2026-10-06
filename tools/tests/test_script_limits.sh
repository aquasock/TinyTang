#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Validate pinned upstream suites, the carried patch and smaller script budgets.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Clone locally so baseline tests use the pinned commits, not working-tree edits.
git -c advice.detachedHead=false clone --quiet --no-hardlinks "$root/third_party/tinydesk-shell" "$work/shell"
git -c advice.detachedHead=false clone --quiet --no-hardlinks "$root/third_party/tinydesk" "$work/desk"
git -c advice.detachedHead=false clone --quiet --no-hardlinks "$root/third_party/tinydesk-shell" "$work/desk/third_party/tdsh"

failed=0

run_suites() {
    local label="$1" flags="$2" excluded="${3:-}"
    local ctest_args=()
    if [[ -n "$excluded" ]]; then
        ctest_args=(-E "$excluded")
    fi
    cmake -S "$work/shell" -B "$work/shell-$label" -DTDSH_BUILD_HOST=ON \
        "-DCMAKE_C_FLAGS=$flags" >"$work/shell-$label.log" 2>&1
    cmake --build "$work/shell-$label" -j4 >>"$work/shell-$label.log" 2>&1
    ctest --test-dir "$work/shell-$label" --output-on-failure "${ctest_args[@]}" || failed=1
    cmake -S "$work/desk" -B "$work/desk-$label" \
        "-DCMAKE_C_FLAGS=$flags" >"$work/desk-$label.log" 2>&1
    cmake --build "$work/desk-$label" -j4 >>"$work/desk-$label.log" 2>&1
    ctest --test-dir "$work/desk-$label" --output-on-failure || failed=1
    if rg -n 'warning:|error:' "$work/shell-$label.log" "$work/desk-$label.log"; then
        echo "test_script_limits: upstream build diagnostics in $label" >&2
        return 1
    fi
}

# Show build logs on failure, before the temporary directory is removed.
trap 'for log in "$work/"*.log; do [[ ! -f "$log" ]] || tail -n 50 "$log"; done' ERR

run_suites pinned ""
bash "$root/scripts/apply-tdsh-patches.sh" "$work/shell"
git -C "$work/shell" diff --binary >"$work/first.patch"
bash "$root/scripts/apply-tdsh-patches.sh" "$work/shell"
git -C "$work/shell" diff --binary >"$work/second.patch"
cmp "$work/first.patch" "$work/second.patch"
git -C "$root/third_party/tinydesk-shell" diff --binary >"$work/checkout.patch"
cmp "$work/first.patch" "$work/checkout.patch"
bash "$root/scripts/apply-tdsh-patches.sh" "$work/desk/third_party/tdsh"
run_suites defaults ""

# The comprehensive upstream fixture retains more than 32 variables.  Run
# every upstream test at 48 variables, then the compatible tests at TinyTang's
# 32.  Linux's libc/pthread worker needs more than a 16 KB stack for these
# scripts, so keep its native 32 KB default.  The exact TinyTang stack request
# is checked below with the worker probe and measured on the board.
small="-DTDSH_MAX_VARS=48 -DTDSH_VAR_NAME_MAX=32 -DTDSH_VAR_VALUE_MAX=128"
run_suites small "$small"
tinytang="-DTDSH_MAX_VARS=32 -DTDSH_VAR_NAME_MAX=32 -DTDSH_VAR_VALUE_MAX=128"
run_suites tinytang "$tinytang" '^tdsh_full_uscript_tests$'

# Check actual boundaries, isolated scripts, worker requests and cleanup.
for mode in defaults small shortnames; do
    vars=64 name=32 value=256 stack=32768
    if [[ "$mode" != defaults ]]; then
        vars=32 value=128 stack=16384
    fi
    if [[ "$mode" == shortnames ]]; then
        name=16
    fi
    flags=()
    if [[ "$mode" != defaults ]]; then
        flags=(-DTDSH_MAX_VARS="$vars" -DTDSH_VAR_NAME_MAX="$name"
               -DTDSH_VAR_VALUE_MAX="$value" -DTDSH_SCRIPT_TASK_STACK="$stack")
    fi
    cc -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -Werror \
        "${flags[@]}" -DEXPECT_VARS="$vars" -DEXPECT_NAME="$name" \
        -DEXPECT_VALUE="$value" -DEXPECT_STACK="$stack" \
        -I "$work/shell/include" "$root/tools/tests/tb_script_limits.c" \
        "$work/shell/src/core/"*.c -o "$work/probe-$mode"
    "$work/probe-$mode"
done

exit "$failed"
