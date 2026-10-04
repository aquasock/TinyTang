#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Apply the local patches this project carries for the QMK keyboard side.
#
# Tang mode is a change to QMK, not a change to TinyTang, so it is carried as a
# patch rather than as local commits.  The checkout under
# /home/vash/tools/qmk-keychron therefore stays pristine and in sync with
# upstream, which `.ai/core.md` requires of every local clone, and everything
# this project changes is visible in one reviewable file per patch.  The
# patches live in third_party/patches/qmk/ and target commit c5d9984 of
# Keychron's QMK fork (branch 2025q3).
#
# A `git checkout .` or a fresh clone reverts them, so this script runs before
# the keyboard firmware is built (make keychron/k2_he/ansi:tang) and is
# idempotent: an already-applied patch is reported and skipped.
#
# A patch is only ever applied or skipped on the strength of a check git itself
# makes -- `git apply --check` forward for "not yet applied", and
# `git apply --reverse --check` for "already applied".  A patch that neither
# applies nor reverses means the checkout is not in a state this script
# understands (upstream moved, or someone edited the tree by hand); the script
# stops with exit 1 rather than writing into a tree it cannot account for.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
patch_dir="$root/third_party/patches/qmk"

# The qmk-keychron checkout.  Override with QMK_DIR when it lives elsewhere.
qmk="${QMK_DIR:-}"
if [[ -z "$qmk" ]]; then
    for candidate in "$root/../qmk-keychron" "/home/vash/tools/qmk-keychron"; do
        if [[ -d "$candidate/.git" || -f "$candidate/.git" ]]; then
            qmk="$(cd -- "$candidate" && pwd)"
            break
        fi
    done
fi

if [[ -z "$qmk" || ( ! -d "$qmk/.git" && ! -f "$qmk/.git" ) ]]; then
    echo "apply-qmk-tang-patches: qmk-keychron checkout not found; set QMK_DIR" >&2
    exit 1
fi

shopt -s nullglob
patches=("$patch_dir"/*.patch)
shopt -u nullglob

if (( ${#patches[@]} == 0 )); then
    echo "apply-qmk-tang-patches: no patches in $patch_dir"
    exit 0
fi

for patch in "${patches[@]}"; do
    name="$(basename "$patch")"
    if git -C "$qmk" apply --reverse --check "$patch" >/dev/null 2>&1; then
        echo "apply-qmk-tang-patches: $name already applied"
    elif git -C "$qmk" apply --check "$patch" >/dev/null 2>&1; then
        git -C "$qmk" apply "$patch"
        echo "apply-qmk-tang-patches: applied $name"
    else
        echo "apply-qmk-tang-patches: $name neither applies nor is already applied in $qmk" >&2
        exit 1
    fi
done

exit 0
