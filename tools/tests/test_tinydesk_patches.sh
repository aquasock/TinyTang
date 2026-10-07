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
grep -q 'td_button(s_win, 1, 15, "Bluetooth...", on_bluetooth, NULL);' "$work/desk/apps/settings.c"
grep -q 'td_app_launch("Bluetooth");' "$work/desk/apps/settings.c"
grep -q 'td_button(s_win, 18, 15, "Date & time...", on_datetime, NULL);' "$work/desk/apps/settings.c"

trap 'tail -n 50 "$work/desk.log" 2>/dev/null; rm -rf "$work"' ERR
cmake -S "$work/desk" -B "$work/desk-build" >"$work/desk.log" 2>&1
cmake --build "$work/desk-build" -j4 >>"$work/desk.log" 2>&1
ctest --test-dir "$work/desk-build" --output-on-failure
if grep -n 'warning:\|error:' "$work/desk.log"; then
    echo "test_tinydesk_patches: TinyDesk build diagnostics" >&2
    exit 1
fi
echo "test_tinydesk_patches: PASS"
