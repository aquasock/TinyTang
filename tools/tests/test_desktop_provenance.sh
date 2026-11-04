#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Check tools/check_desktop_provenance.py: it must accept the differences a
# rebuild legitimately produces and refuse every difference that means the
# tuple changed.
#
# The point of the checker is that a recorded bitstream can be shown to
# reproduce.  A checker that accepts too much would report a match for a core
# built some other way, which is worse than no checker at all, so every case
# below has a direction: the tolerated fields are tolerated, and the fields that
# are inputs are not.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
check="$root/tools/check_desktop_provenance.py"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

pass=0
fail=0
ok() { pass=$((pass + 1)); }
bad() { fail=$((fail + 1)); printf 'FAIL: %s\n' "$1" >&2; }

# A provenance file shaped exactly as tools/desktop_open/build.sh writes one.
write_prov() {
    local path=$1 stamp=$2 head=$3 status=$4 chipdb=$5 bin=$6 fs=$7
    cat > "$path" <<EOF
# desktop core build provenance
# compared by tools/check_desktop_provenance.py; only built_at may differ
built_at=$stamp
seed=3
device=GW5AST-LV138PG484AC1/I0
device_family=GW5AST-138C
source_sha256=b313d00f7824b3d425f222dd8b24355a0e2a4186286491a14af69b539022d96d
tinytang_head=$head
tinytang_status_files=$status
apicula_head=495661aaf86594232e4eecf02210a7f9464996d2
apicula_status_files=0
apicula_chipdb_sha256=$chipdb
nextpnr_version="nextpnr-himbaechel" -- Next Generation Place of Place (Version nextpnr-0.11.1-78-g16237b0f)
nextpnr_binary_sha256=2eca603a2a90424305fd72d53a50c3d1b8daeb21e8f0451690cfc4797b85c6c1
yosys_version=Yosys 0.69+260 (git sha1 dcd59343b, Release, Clang /usr/bin/clang++ 21.1.8)
python_version=Python 3.11.6
desktop_fs_sha256=$fs
desktop_fs_bytes=36239160
desktop_bin_sha256=$bin
desktop_bin_bytes=4526774
EOF
}

CHIP=e74b77e429e9be2e176477fa4d3c9984a1bdee8a17dad5dd3ab80620aefe175b
ABSENT=absent
BIN=00d8b48cac8e54b27cdbdacf2adfe8e67331bd3ff95c05e6fd671192f46582cf
FS=60dd20032095f6d1b7e14dab9e77ca57973e5d494051d0acf2955dd929e45fda

write_prov "$work/a.txt" 2026-11-03T20:52:52-07:00 9b6fdb24f6886a4adf7e696d7ddc15fa3f83c6f6 5 "$CHIP" "$BIN" "$FS"

# 1. Identical files match.
"$check" "$work/a.txt" "$work/a.txt" >/dev/null && ok || bad "identical files should match"

# 2. A rebuild of the same tuple differs only in built_at; that must be tolerated.
sed 's/^built_at=.*/built_at=2026-11-04T01:02:03-07:00/' "$work/a.txt" > "$work/b.txt"
"$check" "$work/a.txt" "$work/b.txt" >/dev/null && ok || bad "built_at must be ignored"

# 3. So must the repository HEAD and its dirty-file count: they are context,
#    not inputs, and a commit unrelated to RTL must not invalidate a core.
sed -e 's/^tinytang_head=.*/tinytang_head=deadbeefdeadbeefdeadbeefdeadbeefdeadbeef/' \
    -e 's/^tinytang_status_files=.*/tinytang_status_files=17/' \
    "$work/a.txt" > "$work/c.txt"
"$check" "$work/a.txt" "$work/c.txt" >/dev/null && ok || bad "repository head and dirt must be ignored"

# 4. Every input differs when it differs.
mutate_and_expect_diff() {
    local name=$1 sedexpr=$2
    sed "$sedexpr" "$work/a.txt" > "$work/m.txt"
    if "$check" "$work/a.txt" "$work/m.txt" >/dev/null 2>&1; then
        bad "$name changed but the checker reported a match"
    else
        ok
    fi
}
mutate_and_expect_diff "chipdb"   's/^apicula_chipdb_sha256=.*/apicula_chipdb_sha256=0000000000000000000000000000000000000000000000000000000000000000/'
mutate_and_expect_diff "seed"     's/^seed=.*/seed=4/'
mutate_and_expect_diff "nextpnr binary" 's/^nextpnr_binary_sha256=.*/nextpnr_binary_sha256=1111111111111111111111111111111111111111111111111111111111111111/'
mutate_and_expect_diff "nextpnr version" 's/^nextpnr_version=.*/nextpnr_version=x/'
mutate_and_expect_diff "apicula head" 's/^apicula_head=.*/apicula_head=0000000000000000000000000000000000000000/'
mutate_and_expect_diff "source digest" 's/^source_sha256=.*/source_sha256=2222222222222222222222222222222222222222222222222222222222222222/'
mutate_and_expect_diff "desktop.bin digest" 's/^desktop_bin_sha256=.*/desktop_bin_sha256=3333333333333333333333333333333333333333333333333333333333333333/'
mutate_and_expect_diff "desktop.fs digest" 's/^desktop_fs_sha256=.*/desktop_fs_sha256=4444444444444444444444444444444444444444444444444444444444444444/'
mutate_and_expect_diff "output size" 's/^desktop_bin_bytes=.*/desktop_bin_bytes=1/'

# 5. A field present in one file and absent from the other is a difference:
#    a build that cannot say what its chipdb was is not the same build.
grep -v '^apicula_chipdb_sha256=' "$work/a.txt" > "$work/m.txt"
if "$check" "$work/a.txt" "$work/m.txt" >/dev/null 2>&1; then
    bad "a missing chipdb field reported a match"
else
    ok
fi

# 6. A build whose chipdb was not found says 'absent'; that is a real value and
#    must not compare equal to a digest.
write_prov "$work/d.txt" 2026-11-03T20:52:52-07:00 9b6fdb24f6886a4adf7e696d7ddc15fa3f83c6f6 5 "$ABSENT" "$BIN" "$FS"
if "$check" "$work/a.txt" "$work/d.txt" >/dev/null 2>&1; then
    bad "an absent chipdb reported a match"
else
    ok
fi

# 7. Malformed input is refused, not guessed at.
: > "$work/empty.txt"
if "$check" "$work/a.txt" "$work/empty.txt" >/dev/null 2>&1; then
    bad "an empty file was accepted"
else
    ok
fi
printf 'this is not provenance\n' > "$work/junk.txt"
if "$check" "$work/a.txt" "$work/junk.txt" >/dev/null 2>&1; then
    bad "a non-provenance file was accepted"
else
    ok
fi
if "$check" "$work/a.txt" "$work/nonexistent.txt" >/dev/null 2>&1; then
    bad "a missing file was accepted"
else
    ok
fi
# A file with no output digest is not a desktop-core provenance file even if
# every other field is present.
grep -v '^desktop_bin_sha256=' "$work/a.txt" > "$work/nodigest.txt"
if "$check" "$work/nodigest.txt" "$work/nodigest.txt" >/dev/null 2>&1; then
    bad "a file with no output digest was accepted"
else
    ok
fi
# A repeated field is ambiguous; refuse it rather than take the last one.
{ cat "$work/a.txt"; echo 'seed=9'; } > "$work/dup.txt"
if "$check" "$work/dup.txt" "$work/dup.txt" >/dev/null 2>&1; then
    bad "a repeated field was accepted"
else
    ok
fi

# 8. Comments and blank lines are ignored, so a hand-annotated copy still checks.
{ echo '# annotated by hand'; echo; cat "$work/a.txt"; echo; } > "$work/annot.txt"
"$check" "$work/a.txt" "$work/annot.txt" >/dev/null && ok || bad "comments and blank lines must be ignored"

# 9. A real file from a real build, if one is present, must parse.
real=$(ls -1 "$root"/build/*/out/provenance.txt 2>/dev/null | head -1 || true)
if [[ -n $real ]]; then
    "$check" "$real" "$real" >/dev/null && ok || bad "a real build's provenance did not parse: $real"
    echo "checked a real build's provenance: $real"
fi

echo "test_desktop_provenance: $pass checks, $fail failed"
[[ $fail -eq 0 ]] || exit 1
echo "test_desktop_provenance: all checks passed"
