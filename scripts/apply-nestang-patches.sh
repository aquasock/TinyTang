#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Apply the local patches this project carries for the nestang core.
#
# The desktop OSD layer is added to nand2mario's nestang as a patch rather than
# as local commits.  The checkout under tangcore/ therefore stays pristine and
# in sync with upstream, which `.ai/core.md` requires of every local clone, and
# everything this project changes is visible in one reviewable file.  The
# patches live in third_party/patches/ and are recorded in THIRD_PARTY.md.
#
# A `git submodule update` or a fresh clone reverts them, so this script runs
# before the core is built (tools/build_nestang_core.sh) and is idempotent: an
# already-applied patch is reported and skipped.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
patch_dir="$root/third_party/patches"

# The nestang checkout.  Override with NESTANG_DIR when it lives elsewhere.
submodule="${NESTANG_DIR:-}"
if [[ -z "$submodule" ]]; then
    for candidate in "$root/../tangcore/nestang" "/run/media/vash/GIT/tangcore/nestang"; do
        if [[ -d "$candidate/.git" || -f "$candidate/.git" ]]; then
            submodule="$(cd -- "$candidate" && pwd)"
            break
        fi
    done
fi

if [[ -z "$submodule" || ( ! -d "$submodule/.git" && ! -f "$submodule/.git" ) ]]; then
    echo "apply-nestang-patches: nestang checkout not found; set NESTANG_DIR" >&2
    exit 1
fi

shopt -s nullglob
patches=("$patch_dir"/*.patch)
shopt -u nullglob

if (( ${#patches[@]} == 0 )); then
    echo "apply-nestang-patches: no patches in $patch_dir"
    exit 0
fi

for patch in "${patches[@]}"; do
    name="$(basename "$patch")"
    # Strict first, then with one line of context.
    #
    # Strict is the right default, and it is what catches a patch belonging to a
    # different tree.  But these patches are anchored on upstream sources and
    # later work lands next to them -- a port added to an instantiation a patch
    # also edits, a declaration inside a hunk's context -- and then the
    # reverse-check of an already-applied patch fails, not because the tree is
    # wrong but because its surroundings moved a few lines.  That is the failure
    # this fallback exists for, and it stays narrow: one line of context, and
    # only after the strict test has already said no.
    state=none
    for ctx in 3 1; do
        if git -C "$submodule" apply --reverse --check -C"$ctx" "$patch" >/dev/null 2>&1; then
            state=applied
            break
        fi
        if git -C "$submodule" apply --check -C"$ctx" "$patch" >/dev/null 2>&1; then
            git -C "$submodule" apply -C"$ctx" "$patch"
            echo "apply-nestang-patches: applied $name"
            state=done
            break
        fi
    done
    case "$state" in
        applied) echo "apply-nestang-patches: $name already applied" ;;
        done)    ;;
        *)       echo "apply-nestang-patches: $name does not apply cleanly" >&2
                 exit 1 ;;
    esac
done

exit 0
