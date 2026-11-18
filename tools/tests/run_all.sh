#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# The one command that answers "is this tree green": every host regression
# under tools/tests plus the help-width gate, one line each, and a summary that
# names every failure.
#
#   tools/tests/run_all.sh            run everything
#   tools/tests/run_all.sh desktop    run the scripts whose names contain "desktop"
#   tools/tests/run_all.sh --list     list the tests without running them
#   tools/tests/run_all.sh --quick    skip the slow tests whose sources did
#                                     not change (see SLOW below)
#
# Exit status is 0 only when every test passed, so this is usable as a release
# gate as well as a convenience.  A --quick run is not a release gate: it says
# what it skipped, and the full run is what a commit that touches fpga/ or the
# pinned upstream trees, or a release, is checked with.
#
# This runs on the host and needs no board.  It is deliberately serial: several
# of these tests build Verilator models or FatFS images at -j4 and run them
# concurrently badly on a small machine, and a matrix is worth more than a
# minute of wall clock.
set -uo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$root"

filter=""
list_only=0
quick=0
for arg in "$@"; do
    case "$arg" in
        --list) list_only=1 ;;
        --quick) quick=1 ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) filter="$arg" ;;
    esac
done

# The slow tests -- about 440 of the full run's 460 seconds -- and the paths
# each depends on besides its own script.  --quick runs one only when a path
# differs from the upstream branch, counting unpushed commits, uncommitted
# edits and new files.  A dirty submodule is ignored: third_party/tinydesk is
# always dirty, because the build applies the carried patches to it, so only
# a change of its pinned commit counts.
declare -A SLOW=(
    [test_desktop_plane]="fpga/ tools/tests/sim/"
    [test_desktop_core]="fpga/ tools/tests/sim/"
    [test_composite]="fpga/ tools/tests/sim/"
    [test_desktop_video]="fpga/ tools/tests/sim/"
    [test_desktop_oled]="fpga/ tools/tests/sim/"
    [test_desktop_link]="fpga/ tools/tests/sim/"
    [test_link_uart]="fpga/ tools/tests/sim/"
    [test_script_limits]="third_party/tinydesk-shell third_party/tinydesk third_party/patches/ scripts/apply-tdsh-patches.sh scripts/apply-tinydesk-patches.sh"
    [test_tinydesk_patches]="third_party/tinydesk-shell third_party/tinydesk third_party/patches/ scripts/apply-tdsh-patches.sh scripts/apply-tinydesk-patches.sh"
)
changed=""
if ((quick)); then
    base="$(git rev-parse --abbrev-ref --symbolic-full-name '@{upstream}' 2>/dev/null || echo HEAD)"
    changed="$( { git diff --name-only --ignore-submodules=dirty "$base"
                  git ls-files --others --exclude-standard; } 2>/dev/null )"
fi

# Whether a slow test has to run in a quick pass: a changed path under one of
# its dependencies, or a change to its own script.
quick_needs() {
    local name="$1" dep path
    for dep in ${SLOW[$name]} "tools/tests/$name.sh"; do
        while IFS= read -r path; do
            [[ -n $path && $path == "$dep"* ]] && return 0
        done <<<"$changed"
    done
    return 1
}

shopt -s nullglob
tests=(tools/tests/test_*.sh)
shopt -u nullglob

if ((${#tests[@]} == 0)); then
    echo "run_all: no tests found under tools/tests" >&2
    exit 2
fi

selected=()
for t in "${tests[@]}"; do
    if [[ -z $filter || $t == *"$filter"* ]]; then
        selected+=("$t")
    fi
done

if ((list_only)); then
    printf '%s\n' "${selected[@]}"
    exit 0
fi

if ((${#selected[@]} == 0)); then
    echo "run_all: no test matches '$filter'" >&2
    exit 2
fi

pass=0
skipped=0
failed=()
failed_names=()

for t in "${selected[@]}"; do
    name="$(basename "$t" .sh)"
    if ((quick)) && [[ -n ${SLOW[$name]:-} ]] && ! quick_needs "$name"; then
        printf 'SKIP  %-34s       (quick: nothing it depends on changed)\n' "$name"
        skipped=$((skipped + 1))
        continue
    fi
    start=$SECONDS
    # Each test gets its own log: a failure has to be readable afterwards
    # without a second run, and the matrix line stays one line.
    log="$(mktemp)"
    if bash "$t" >"$log" 2>&1; then
        elapsed=$((SECONDS - start))
        printf 'PASS  %-34s %3ds\n' "$name" "$elapsed"
        pass=$((pass + 1))
    else
        rc=$?
        elapsed=$((SECONDS - start))
        printf 'FAIL  %-34s %3ds  (exit %d)\n' "$name" "$elapsed" "$rc"
        failed+=("$log")
        failed_names+=("$name")
    fi
    if [[ -n ${TINYTANG_KEEP_LOGS:-} ]]; then
        mv "$log" "$root/$(basename "$log").$name.log"
    fi
done

# The help-width gate is not a shell script, but it gates the same surface: a
# command whose help text overflows the console wraps and becomes unreadable.
start=$SECONDS
if python3 tools/check_help_width.py >/tmp/check_help_width.$$ 2>&1; then
    printf 'PASS  %-34s %3ds  %s\n' "check_help_width" "$((SECONDS - start))" \
        "$(tail -1 /tmp/check_help_width.$$)"
    pass=$((pass + 1))
else
    printf 'FAIL  %-34s %3ds\n' "check_help_width" "$((SECONDS - start))"
    tail -5 /tmp/check_help_width.$$
    failed_names+=("check_help_width")
fi
rm -f /tmp/check_help_width.$$

total=$((pass + ${#failed_names[@]}))
echo
if ((skipped)); then
    echo "run_all: $pass of $total passed, $skipped slow test(s) skipped (--quick)"
else
    echo "run_all: $pass of $total passed"
fi

if ((${#failed_names[@]})); then
    echo "failures:"
    for n in "${failed_names[@]}"; do echo "  - $n"; done
    echo
    echo "output of the failing tests:"
    for log in "${failed[@]}"; do
        echo "===== $(basename "$log")"
        tail -25 "$log"
        rm -f "$log"
    done
    exit 1
fi
exit 0
