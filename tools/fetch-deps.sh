#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Materialise this release's dependency set from deps.conf.
#
#   tools/fetch-deps.sh              clone/check out every pinned source, verify it
#   tools/fetch-deps.sh --check      change nothing; report where the tree stands
#
#   RELEASE_ROOT     the directory holding this checkout (default: this
#                    repository's parent, which is the layout the release is cut
#                    in -- TinyTang and its dependencies as siblings)
#   ALLOW_UNPINNED=1 report the UNPINNED entries and still exit 0
#
# Every `source` entry is checked out at its full commit SHA and verified, so a
# release either reproduces exactly or fails loudly.  Entries this script cannot
# fetch -- binaries with no recorded checksum, the generated chipdb, the vendor
# tools -- are reported as gaps rather than skipped silently: that list is the
# honest answer to whether the tree can build a release.
#
# Submodule destinations (third_party/*) are materialised into their real paths
# so the superproject's gitlinks are satisfied.  Everything else lands in a
# sibling directory named for the entry and is pointed at by the build's own
# environment variables -- BL_SDK_BASE, NEXTPNR_BUILD, APICULA_FORK, TINYNES_DIR
# -- rather than by a path this script guesses at.  See BUILDING.md.

set -uo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(git -C "$here" rev-parse --show-toplevel 2>/dev/null || dirname "$here")
conf="${DEPS_CONF:-$repo/deps.conf}"
root="${RELEASE_ROOT:-$(dirname "$repo")}"
check=0
[[ "${1:-}" == "--check" || "${1:-}" == "-n" ]] && check=1

[[ -f $conf ]] || { echo "no manifest at $conf" >&2; exit 2; }

pinned=0; unpinned=0; ok=0; fetched=0; failed=0; refs=0
unpinned_names=()

echo "release root : $root"
echo "manifest     : $conf"
(( check )) && echo "mode         : check only"
echo

while read -r kind name pin url dest _; do
    [[ -z ${kind:-} ]] && continue
    case "$kind" in \#*) continue ;; esac

    if [[ $kind == reference ]]; then
        refs=$((refs+1))
        printf 'reference %-10s %-30s %s\n' "$kind" "$name" "${pin:0:12}"
        continue
    fi

    if [[ $pin == UNPINNED* ]]; then
        unpinned=$((unpinned+1))
        unpinned_names+=("$kind $name")
        printf 'GAP       %-10s %-30s %s\n' "$kind" "$name" "${pin#UNPINNED*}"
        continue
    fi

    case "$kind" in
        source)
            pinned=$((pinned+1))
            target="$root/${dest:-$name}"
            if [[ -d $target/.git ]]; then
                have=$(git -C "$target" rev-parse -q --verify HEAD || true)
                if [[ $have == "$pin" ]]; then
                    printf 'ok        %-10s %-30s %s\n' "$kind" "$name" "${pin:0:12}"
                    ok=$((ok+1)); continue
                fi
                (( check )) && { printf 'MISMATCH  %-10s %-30s have %s want %s\n' "$kind" "$name" "${have:0:12}" "${pin:0:12}"; failed=$((failed+1)); continue; }
                # existing but wrong: re-fetch the pin rather than clobber the tree
                git -C "$target" fetch -q --depth 1 origin "$pin" 2>/dev/null
                git -C "$target" -c advice.detachedHead=false checkout -q --detach "$pin" 2>/dev/null
            else
                (( check )) && { printf 'MISSING   %-10s %-30s %s\n' "$kind" "$name" "$target"; failed=$((failed+1)); continue; }
                mkdir -p "$target"
                git -C "$target" init -q
                git -C "$target" remote add origin "$url" 2>/dev/null || git -C "$target" remote set-url origin "$url"
                git -C "$target" fetch -q --depth 1 origin "$pin" || { printf 'FETCHFAIL %-10s %-30s %s\n' "$kind" "$name" "$url"; failed=$((failed+1)); continue; }
                git -C "$target" -c advice.detachedHead=false checkout -q --detach "$pin"
                fetched=$((fetched+1))
            fi
            have=$(git -C "$target" rev-parse -q --verify HEAD || true)
            if [[ $have == "$pin" ]]; then
                printf 'ok        %-10s %-30s %s\n' "$kind" "$name" "${pin:0:12}"
                ok=$((ok+1))
            else
                printf 'FAILED    %-10s %-30s have %s want %s\n' "$kind" "$name" "${have:0:12}" "${pin:0:12}"
                failed=$((failed+1))
            fi
            ;;
        binary|generated|vendor)
            # A pinned binary would carry a SHA-256 in the pin field; none do yet.
            pinned=$((pinned+1))
            printf 'unfetch.. %-10s %-30s %s\n' "$kind" "$name" "${pin:0:16}"
            failed=$((failed+1))
            ;;
        *)
            printf 'BADKIND   %-10s %-30s\n' "$kind" "$name"
            failed=$((failed+1))
            ;;
    esac
done < <(sed 's/#.*//' "$conf")

echo
echo "pinned sources : $pinned   ($ok verified, $fetched fetched, $failed unresolved)"
echo "unpinned gaps  : $unpinned"
for n in "${unpinned_names[@]:-}"; do [[ -n $n ]] && echo "                 $n"; done
echo "references     : $refs   (not build inputs)"

if (( failed )); then
    echo
    echo "There are entries this script cannot satisfy.  A release cut from this"
    echo "tree is not reproducible until they are resolved or explicitly accepted."
    exit 1
fi
if (( unpinned )); then
    echo
    if [[ ${ALLOW_UNPINNED:-} == 1 ]]; then
        echo "ALLOW_UNPINNED is set: continuing with $unpinned unpinned dependencies."
        echo "Record that choice in the release notes; the tree is not reproducible."
        exit 0
    fi
    echo "Set ALLOW_UNPINNED=1 to continue anyway."
    exit 1
fi
echo
echo "every dependency is pinned and verified."
