#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Apply TinyTang's carried TinyDesk patches before configuring a build.
# Usage: scripts/apply-tinydesk-patches.sh [checkout]
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
checkout="${1:-$root/third_party/tinydesk}"
base=feaf84130f03594845e5e9284817dca785b4d878

if [[ "$(git -C "$checkout" rev-parse HEAD)" != "$base" ]]; then
    echo "apply-tinydesk-patches: expected TinyDesk v0.1.5 ($base)" >&2
    exit 1
fi

for patch in "$root/third_party/patches/tinydesk/"*.patch; do
    if git -C "$checkout" apply --reverse --check "$patch" >/dev/null 2>&1; then
        echo "apply-tinydesk-patches: $(basename "$patch") already applied"
    elif git -C "$checkout" apply --check "$patch"; then
        git -C "$checkout" apply "$patch"
        echo "apply-tinydesk-patches: applied $(basename "$patch")"
    else
        echo "apply-tinydesk-patches: refusing unrecognised changes for $(basename "$patch")" >&2
        exit 1
    fi
done
