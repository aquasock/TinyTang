#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Renaming over a name that is already in use, through the SDK's real FatFs and
# this repository's fatfs_conf_user.h (tools/tests/tb_fat_rename.c).  f_rename()
# refuses an occupied name with FR_EXIST, so the Controls app's save cannot be a
# single rename over /nes.ini and goes through a backup name instead.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
sdk="${BL_SDK_BASE:-$HOME/.cache/tangcore-dev/sdk-2.3.36}"
fatfs="$sdk/components/fs/fatfs"
[[ -f "$fatfs/ff.c" ]] || { echo "test_fat_rename: no FatFs at $fatfs" >&2; exit 1; }
export PATH="$PATH:/usr/sbin:/sbin"
command -v mkfs.fat >/dev/null || { echo "test_fat_rename: mkfs.fat (dosfstools) is required" >&2; exit 1; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

dd if=/dev/zero of="$work/nes.img" bs=1M count=48 status=none
mkfs.fat -F 32 -S 512 -s 1 -n TINYNES "$work/nes.img" >/dev/null

mkdir -p "$work/conf"
cp "$root/fatfs_conf_user.h" "$work/conf/"

cc -std=gnu11 -O1 -Wall -Wextra -Wno-unused-function -Wno-unused-variable \
   -I "$work/conf" -I "$fatfs" \
   "$root/tools/tests/tb_fat_rename.c" "$fatfs/ff.c" "$fatfs/ffunicode.c" -o "$work/tb"

"$work/tb" "$work/nes.img"

# Linux must still read the card the sequence left behind.
fsck.fat -n "$work/nes.img" > "$work/fsck.log" 2>&1 || { cat "$work/fsck.log"; exit 1; }
echo "test_fat_rename: fsck.fat accepts the image after the renames"
