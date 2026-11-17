#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Check TinyTang's carried TinyDesk patches: they apply once and only once to
# the pinned commit, reproduce the submodule checkout exactly, and leave
# TinyDesk's own host suite passing without compiler diagnostics.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Clone locally so the patches meet the pinned commit, not working-tree edits.
git -c advice.detachedHead=false clone --quiet --no-hardlinks "$root/third_party/tinydesk" "$work/desk"
git -c advice.detachedHead=false clone --quiet --no-hardlinks "$root/third_party/tinydesk-shell" "$work/desk/third_party/tdsh"
bash "$root/scripts/apply-tdsh-patches.sh" "$work/desk/third_party/tdsh" >/dev/null

bash "$root/scripts/apply-tinydesk-patches.sh" "$work/desk"
git -C "$work/desk" diff --binary --ignore-submodules >"$work/first.patch"
bash "$root/scripts/apply-tinydesk-patches.sh" "$work/desk"
git -C "$work/desk" diff --binary --ignore-submodules >"$work/second.patch"
cmp "$work/first.patch" "$work/second.patch"
git -C "$root/third_party/tinydesk" diff --binary --ignore-submodules >"$work/checkout.patch"
cmp "$work/first.patch" "$work/checkout.patch"

# Settings now launches the port's app by its own name.
# Settings must still open this port's Bluetooth window.  Since TinyDesk 0.4
# that button is upstream's own (it adopted TinyTang's window, tinydesk issue
# #7), so the check is that it exists and launches "Bluetooth", wherever
# upstream puts it -- not the position the retired 0001 patch used.
grep -q '"Bluetooth...", on_bluetooth, NULL);' "$work/desk/apps/settings.c"
grep -q 'td_app_launch("Bluetooth");' "$work/desk/apps/settings.c"
# The two carried patches' own marks.
grep -q 'td_slider' "$work/desk/include/tinydesk/td_widgets.h"

trap 'tail -n 50 "$work/desk.log" 2>/dev/null; rm -rf "$work"' ERR
cmake -S "$work/desk" -B "$work/desk-build" >"$work/desk.log" 2>&1
cmake --build "$work/desk-build" -j4 >>"$work/desk.log" 2>&1
ctest --test-dir "$work/desk-build" --output-on-failure
# Diagnostics are judged against unmodified upstream at the same commit, so a
# warning upstream ships with is not blamed on the carried patches.
git -c advice.detachedHead=false clone --quiet --no-hardlinks "$root/third_party/tinydesk" "$work/pristine"
git -c advice.detachedHead=false clone --quiet --no-hardlinks "$root/third_party/tinydesk-shell" "$work/pristine/third_party/tdsh"
cmake -S "$work/pristine" -B "$work/pristine-build" >"$work/pristine.log" 2>&1
cmake --build "$work/pristine-build" -j4 >>"$work/pristine.log" 2>&1
diags() { grep -ohE '[^ :]+\.(c|h|cc|cpp):[0-9]+:[0-9]+: (warning|error):.*' "$1" | sed "s|$work/[^/]*/||" | sort -u || true; }
new="$(comm -23 <(diags "$work/desk.log") <(diags "$work/pristine.log") | sed '/^$/d')"
if [[ -n "$new" ]]; then
    printf '%s\n' "$new"
    echo "test_tinydesk_patches: TinyDesk build diagnostics the patches introduce" >&2
    exit 1
fi
echo "test_tinydesk_patches: PASS"
