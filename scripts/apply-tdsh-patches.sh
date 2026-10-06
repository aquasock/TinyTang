#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Apply TinyTang's carried TinyDesk Shell patches before configuring a build.
# Usage: scripts/apply-tdsh-patches.sh [checkout]
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
checkout="${1:-$root/third_party/tinydesk-shell}"
base=8456dd1d8c2fa88706c065edba483be67fcb3553

if [[ "$(git -C "$checkout" rev-parse HEAD)" != "$base" ]]; then
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
