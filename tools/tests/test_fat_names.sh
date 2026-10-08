#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# File names outside code page 437 through the SDK's real FatFs and this
# repository's fatfs_conf_user.h (core-log entry 70).  The harness runs twice:
# with the configuration as committed, which must keep every Hungarian name
# usable, and with names forced back to code page 437, which must reproduce
# the card, so the image and harness are known to show the fault.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
sdk="${BL_SDK_BASE:-$HOME/.cache/tangcore-dev/sdk}"
fatfs="$sdk/components/fs/fatfs"
[[ -f "$fatfs/ff.c" ]] || { echo "test_fat_names: no FatFs at $fatfs" >&2; exit 1; }
export PATH="$PATH:/usr/sbin:/sbin"
command -v mkfs.fat >/dev/null || { echo "test_fat_names: mkfs.fat (dosfstools) is required" >&2; exit 1; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

python3 "$root/tools/tests/make_fat_names_image.py" "$work/names.img" "$work/expected.tsv"

build() {   # build <dir with fatfs_conf_user.h> <binary>
    cc -std=gnu11 -O1 -Wall -Wno-unused-function -Wno-unused-variable \
       -I "$1" -I "$fatfs" \
       "$root/tools/tests/tb_fat_names.c" "$fatfs/ff.c" "$fatfs/ffunicode.c" -o "$2"
}

mkdir -p "$work/fixed" "$work/cp437"
cp "$root/fatfs_conf_user.h" "$work/fixed/"
sed -E 's/^#define FF_LFN_UNICODE [0-9]+/#define FF_LFN_UNICODE 0/' "$root/fatfs_conf_user.h" > "$work/cp437/fatfs_conf_user.h"
build "$work/fixed" "$work/tb_fixed"
build "$work/cp437" "$work/tb_cp437"

cp "$work/names.img" "$work/fixed.img"
cp "$work/names.img" "$work/cp437.img"
"$work/tb_cp437" "$work/cp437.img" > "$work/cp437.out"
python3 "$root/tools/tests/check_fat_names.py" "$work/expected.tsv" "$work/cp437.out" cp437
"$work/tb_fixed" "$work/fixed.img" > "$work/fixed.out"
python3 "$root/tools/tests/check_fat_names.py" "$work/expected.tsv" "$work/fixed.out" fixed
# Linux must still read everything the fixed build left behind.
fsck.fat -n "$work/fixed.img" > "$work/fsck.log" 2>&1 || { cat "$work/fsck.log"; exit 1; }
echo "test_fat_names: fsck.fat accepts the image after the moves"
