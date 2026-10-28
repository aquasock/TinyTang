#!/bin/bash
# Build one Gutenberg shelf: a Library of Congress class archive -> a TinyGutenberg pack.
#
#   tools/gutenberg_shelf.sh b          # philosophy and religion
#   tools/gutenberg_shelf.sh q          # science
#   tools/gutenberg_shelf.sh ps         # American literature
#
# It finds the newest archive Kiwix publishes for that class, downloads it,
# checks it against its own MD5, converts it with the settings a book archive
# needs, and verifies the pack. Progress is printed at every stage, and each
# stage stops the chain rather than letting a bad archive become a bad pack.
#
# Environment:
#   SCRATCH=<dir>   where the archive and the pack are written
#                   (default /run/media/vash/GIT/tinytang-scratch)
#   CARD=<dir>      the mounted card; when given, the pack is copied to
#                   $CARD/root/books/<name> as the last step
set -u

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd -- "$HERE/.." && pwd)"
CONVERT="$ROOT/tools/make_wiki_pack.py"
CHECK="$ROOT/tools/check_wiki_pack.py"
LISTING="https://download.kiwix.org/zim/gutenberg/"

CLASS=${1:-}
if [ -z "$CLASS" ]; then
    echo "usage: $(basename "$0") <class>    e.g. b, q, ps, d" >&2
    echo "classes are Library of Congress letters; see manual/gutenberg.md" >&2
    exit 2
fi
CLASS=$(printf '%s' "$CLASS" | tr '[:upper:]' '[:lower:]')
SCRATCH=${SCRATCH:-/run/media/vash/GIT/tinytang-scratch}
NAME="gutenberg-$CLASS"

say() { printf '%s  %s\n' "$(date +%H:%M:%S)" "$*"; }
human() { numfmt --to=iec --suffix=B --format=%.1f "$1" 2>/dev/null || printf '%s B' "$1"; }
clock() {
    local s=${1:-0}
    if [ "$s" -ge 3600 ]; then printf '%dh%02dm' $((s / 3600)) $((s % 3600 / 60));
    elif [ "$s" -ge 60 ]; then printf '%dm%02ds' $((s / 60)) $((s % 60));
    else printf '%ds' "$s"; fi
}
size_of() { stat -c%s "$1" 2>/dev/null || echo 0; }

mkdir -p "$SCRATCH" || exit 1
cd "$SCRATCH" || exit 1

# ---------------------------------------------------------------- the archive

say "class $CLASS: looking for the newest archive"
FILE=$(curl -sSL --max-time 120 "$LISTING" 2>/dev/null |
       grep -oE "gutenberg_en_lcc-${CLASS}_[0-9]{4}-[0-9]{2}\.zim" |
       sort -u | tail -1)
if [ -z "$FILE" ]; then
    say "no archive named gutenberg_en_lcc-$CLASS in the listing; check the class letter"
    exit 1
fi
URL="$LISTING$FILE"
say "           $FILE"

total=$(curl -sSIL --max-time 60 "$URL" 2>/dev/null | tr -d '\r' |
        awk 'tolower($1)=="content-length:"{print $2}' | tail -1)
total=${total:-0}

have=$(size_of "$FILE")
if [ "$total" -gt 0 ]; then
    say "stage 1/4  download  $(human "$have") of $(human "$total") already here"
else
    say "stage 1/4  download  $(human "$have") already here"
fi
[ "$have" -gt 0 ] && say "           resuming, not restarting"

curl -sSL -C - --retry 20 --retry-delay 10 --retry-all-errors -o "$FILE" "$URL" &
curl_pid=$!
prev=$have
prev_t=$(date +%s)
while kill -0 "$curl_pid" 2>/dev/null; do
    sleep "${PROGRESS_SECS:-15}"
    kill -0 "$curl_pid" 2>/dev/null || break
    now=$(size_of "$FILE")
    now_t=$(date +%s)
    dt=$((now_t - prev_t)); [ "$dt" -le 0 ] && dt=1
    rate=$(((now - prev) / dt))
    prev=$now; prev_t=$now_t
    if [ "$total" -gt 0 ]; then
        pct=$((now * 100 / total))
        eta=0
        if [ "$rate" -gt 0 ] && [ "$now" -lt "$total" ]; then
            eta=$(((total - now) / rate))
        fi
        say "  $(human "$now") / $(human "$total")  ${pct}%  $(human "$rate")/s  eta $(clock "$eta")"
    else
        say "  $(human "$now")  $(human "$rate")/s"
    fi
done
wait "$curl_pid"; rc=$?
if [ "$rc" -ne 0 ]; then
    say "download failed (curl exit $rc); stopping"
    exit 1
fi
final=$(size_of "$FILE")
say "           download complete: $(human "$final")"

# ------------------------------------------------------------------ integrity

say "stage 2/4  verify    the archive's own MD5 over $(human "$final")"
python3 - "$FILE" <<'PY'
import hashlib, sys
path = sys.argv[1]
with open(path, "rb") as f:
    f.seek(0, 2)
    size = f.tell()
    f.seek(size - 16)
    tail = f.read(16)
    f.seek(0)
    digest = hashlib.md5()
    left = size - 16
    step = 8 << 20
    done = 0
    shown = -5
    while left:
        block = f.read(min(step, left))
        if not block:
            break
        digest.update(block)
        done += len(block)
        left -= len(block)
        pct = done * 100 // max(size - 16, 1)
        if pct >= shown + 20:
            shown = pct
            print(f"  {pct}% hashed", flush=True)
ok = digest.digest() == tail
print("  md5", "matches" if ok else "DOES NOT MATCH", flush=True)
sys.exit(0 if ok else 1)
PY
if [ "$?" -ne 0 ]; then
    say "archive failed its own MD5; stopping"
    exit 1
fi

# ----------------------------------------------------------------- conversion

say "stage 3/4  convert   -> $NAME"
python3 "$CONVERT" --zim "$FILE" --out "$NAME" --min-chars 2000 \
    --max-article-bytes 2097152 2>&1 |
    while IFS= read -r line; do say "  $line"; done
rc=${PIPESTATUS[0]}
if [ "$rc" -ne 0 ]; then
    say "conversion failed; stopping"
    exit 1
fi

# ---------------------------------------------------------------------- check

say "stage 4/4  check     every record, a sample of the bodies"
python3 "$CHECK" "$NAME" --sample 20
if [ "$?" -ne 0 ]; then
    say "the pack failed its check; stopping"
    exit 1
fi

say "done: $SCRATCH/$NAME"
grep -E '^(title|articles|redirects|parts|authors|chunks) =' "$NAME/wiki.meta" | sed 's/^/           /'
if [ -n "${CARD:-}" ] && [ ! -d "$CARD/root" ]; then
    say "card is not mounted at $CARD; leaving the pack in $SCRATCH/$NAME"
    say "to use it: mkdir -p <card>/root/books/$NAME && cp $NAME/wiki.* <card>/root/books/$NAME/"
elif [ -n "${CARD:-}" ]; then
    say "installing into $CARD/root/books/$NAME"
    mkdir -p "$CARD/root/books/$NAME" && cp "$NAME"/wiki.* "$CARD/root/books/$NAME/" && sync
    say "installed; Refresh in TinyGutenberg lists it"
else
    say "to use it: mkdir -p <card>/root/books/$NAME && cp $NAME/wiki.* <card>/root/books/$NAME/"
fi
