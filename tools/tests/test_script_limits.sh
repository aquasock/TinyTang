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
    # Diagnostics are compared with what unmodified upstream already emits (the
    # "pinned" run): a warning upstream ships with -- TinyDesk 0.5.2's own
    # tests/test_task_snapshot.c clips a long name on purpose and GCC says so --
    # is upstream's, and this test is about what TinyTang's limits introduce.
    local diags
    diags="$(grep -ohE '[^ :]+\.(c|h|cc|cpp):[0-9]+:[0-9]+: (warning|error):.*' \
                 "$work/shell-$label.log" "$work/desk-$label.log" |
             sed "s|$work/[^/]*/||" | sort -u || true)"
    if [[ "$label" == pinned ]]; then
        printf '%s\n' "$diags" >"$work/upstream.diags"
        [[ -z "$diags" ]] || { echo "test_script_limits: upstream's own diagnostics (baseline):"; printf '  %s\n' "$diags"; }
    else
        local new
        new="$(comm -23 <(printf '%s\n' "$diags") "$work/upstream.diags" | sed '/^$/d')"
        if [[ -n "$new" ]]; then
            printf '%s\n' "$new"
            echo "test_script_limits: new build diagnostics in $label" >&2
            return 1
        fi
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

# All upstream tests fit TinyTang's 48-variable table.  Retain a 32-variable
# configuration to check that ports can still choose smaller budgets; the
# comprehensive fixture retains more than 32 variables and runs at 48.
# Linux's libc/pthread worker needs more than a 16 KB stack for these scripts,
# so keep its native 32 KB default.  The exact TinyTang stack request is checked
# below with the worker probe and measured on the board.
tinytang="-DTDSH_MAX_VARS=48 -DTDSH_VAR_NAME_MAX=32 -DTDSH_VAR_VALUE_MAX=128"
run_suites tinytang "$tinytang"
compact="-DTDSH_MAX_VARS=32 -DTDSH_VAR_NAME_MAX=32 -DTDSH_VAR_VALUE_MAX=128"
run_suites compact "$compact" '^tdsh_full_uscript_tests$'

# Check actual boundaries, isolated scripts, worker requests and cleanup.
for mode in defaults tinytang compact shortnames; do
    vars=64 name=32 value=256 stack=32768
    if [[ "$mode" != defaults ]]; then
        vars=48 value=128 stack=16384
    fi
    if [[ "$mode" == compact ]]; then
        vars=32
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
