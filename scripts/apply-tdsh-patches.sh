#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Apply TinyTang's carried TinyDesk Shell patches before configuring a build.
# Usage: scripts/apply-tdsh-patches.sh [checkout]
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
checkout="${1:-$root/third_party/tinydesk-shell}"
base=8456dd1d8c2fa88706c065edba483be67fcb3553

if [[ "$(git -C "$checkout" rev-parse HEAD)" != "$base" ]]; then
    # A checkout newer than the pinned release -- upstream main now, or the
    # release that carries it once one is pinned -- may already contain the
    # limits change.  The shell has only this one carried patch, and the marker
    # is the #ifndef guard the patch adds; a second carried patch would need a
    # check of its own here before this early exit could be trusted.
    if grep -q '^#ifndef TDSH_MAX_VARS' "$checkout/include/tdsh.h" 2>/dev/null; then
        echo "apply-tdsh-patches: checkout is newer and already carries the limits patch"
        exit 0
    fi
    echo "apply-tdsh-patches: expected TinyDesk Shell v0.1.5 ($base)" >&2
    exit 1
fi

for patch in "$root/third_party/patches/tdsh/"*.patch; do
    if git -C "$checkout" apply --reverse --check "$patch" >/dev/null 2>&1; then
        echo "apply-tdsh-patches: $(basename "$patch") already applied"
    elif git -C "$checkout" apply --check "$patch"; then
        git -C "$checkout" apply "$patch"
        echo "apply-tdsh-patches: applied $(basename "$patch")"
    else
        echo "apply-tdsh-patches: refusing unrecognised changes for $(basename "$patch")" >&2
        exit 1
    fi
done
