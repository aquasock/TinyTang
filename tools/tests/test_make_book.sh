#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# TinyReader books: tools/make_book.py on synthetic pages -- from a folder and
# from a CBZ, which must give the same bytes -- checked independently in
# Python (header, table, layout and pixels), then read by the firmware's own
# parser with the spread cache's planning (tools/tests/tb_tang_book.c).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

python3 - "$work" <<'PY'
import sys, os
from PIL import Image
work = sys.argv[1]
os.makedirs(f"{work}/pages")
# Five pages, portrait, solid colours; page 3 a different size.  Named so a
# plain sort would put 10 before 2: the converter must sort them naturally.
colours = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255)]
names = ["p1.png", "p2.png", "p3.png", "p4.png", "p10.png"]
for i, (c, n) in enumerate(zip(colours, names)):
    size = (200, 300) if i != 2 else (100, 150)
    Image.new("RGB", size, c).save(f"{work}/pages/{n}")
PY
python3 - "$work" <<'PY'
import sys, os
from PIL import Image
work = sys.argv[1]
os.makedirs(f"{work}/wide")
# Page 3 is a two-page scan, wider than tall.
for i, c in enumerate([(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255)]):
    Image.new("RGB", (400, 300) if i == 2 else (200, 300), c).save(f"{work}/wide/w{i + 1}.png")
PY
(cd "$work/pages" && python3 -c "
import zipfile, glob
with zipfile.ZipFile('../book.cbz', 'w') as z:
    for n in sorted(glob.glob('*.png')): z.write(n)
")

mb="$root/tools/make_book.py"
python3 "$mb" "$work/pages" "$work/a.trb" --size 64x36 --title "Test Book" > /dev/null
python3 "$mb" "$work/book.cbz" "$work/b.trb" --size 64x36 --title "Test Book" > /dev/null
cmp "$work/a.trb" "$work/b.trb"
python3 "$mb" "$work/pages" "$work/c.trb" --size 64x36 --title "Test Book" --no-cover --no-labels > /dev/null
python3 "$mb" "$work/pages" "$work/d.trb" --size 64x36 --title "Test Book" --rtl --no-labels > /dev/null
python3 "$mb" "$work/pages" "$work/e.trb" --size 160x36 --title "Test Book" > /dev/null
python3 "$mb" "$work/wide" "$work/f.trb" --size 64x36 --title "Test Book" --no-labels > /dev/null

python3 - "$work" <<'PY'
import struct, sys
import numpy as np
work = sys.argv[1]
def load(name):
    d = open(f"{work}/{name}", "rb").read()
    h = struct.unpack("<4sHHHHHHIIHHI64s32s", d[:128])
    magic, ver, hb, w, ht, fmt, n, off, sb, pages, flags = h[:11]
    assert (magic, ver, hb, fmt) == (b"TRB1", 1, 128, 1), h[:4]
    assert (w, ht, sb) == (64, 36, 64 * 36 * 2)
    assert off % 4096 == 0 and len(d) == off + n * sb
    assert h[12].rstrip(b"\0") == b"Test Book"
    table = [struct.unpack("<HHI", d[128 + 8 * i:136 + 8 * i])[:2] for i in range(n)]
    spreads = [np.frombuffer(d[off + i * sb:off + (i + 1) * sb], ">u2").reshape(36, 64) for i in range(n)]
    return n, pages, flags, table, spreads
def c565(r, g, b):
    return ((r * 31 + 127) // 255) << 11 | ((g * 63 + 127) // 255) << 5 | ((b * 31 + 127) // 255)
RED, GREEN, BLUE, YELLOW, MAGENTA = (c565(*c) for c in
    [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255)])

# Cover alone, then 2-3 and 4-5, page 10 sorted last.
n, pages, flags, table, s = load("a.trb")
assert (n, pages, flags) == (3, 5, 1) and table == [(1, 0), (2, 3), (4, 5)], table
assert s[0][18, 32] == RED and s[0][18, 0] == 0 and s[0][18, 63] == 0      # centred, margins black
assert s[1][18, 20] == GREEN and s[1][18, 44] == BLUE                      # left, right
assert s[1][18, 31] == GREEN and s[1][18, 32] == BLUE                      # meeting in the middle
assert s[2][18, 20] == YELLOW and s[2][18, 44] == MAGENTA
# A label that does not fit the margin is left out, not drawn over a page.
assert (s[1][:, 56:] == 0).all()
# No cover: 1-2, 3-4, 5 alone.
n, pages, flags, table, s = load("c.trb")
assert (n, flags) == (3, 0) and table == [(1, 2), (3, 4), (5, 0)], table
assert s[0][18, 20] == RED and s[0][18, 44] == GREEN and s[2][18, 32] == MAGENTA
assert (s[0][:, 56:] == 0).all(), "a label despite --no-labels"
# Right to left: the first of each pair on the right.
n, pages, flags, table, s = load("d.trb")
assert flags == 3 and table == [(1, 0), (3, 2), (5, 4)], table
assert s[1][18, 20] == BLUE and s[1][18, 44] == GREEN
# A two-page scan is a spread of its own, as large as the screen allows; the
# page waiting before it is shown alone, and pairing starts again after it.
n, pages, flags, table, s = load("f.trb")
assert table == [(1, 0), (2, 0), (3, 0), (4, 5)], table
assert s[2][18, 32] == BLUE and s[2][18, 9] == BLUE and s[2][18, 54] == BLUE   # 48 wide
assert s[2][18, 4] == 0 and s[2][18, 60] == 0
assert s[1][18, 32] == GREEN and s[1][18, 10] == 0                         # alone, centred
# With room, the label sits grey in the right margin and the pages are untouched.
d = open(f"{work}/e.trb", "rb").read()
e = np.frombuffer(d[4096 + 160 * 36 * 2:4096 + 2 * 160 * 36 * 2], ">u2").reshape(36, 160)
assert e[18, 60] == GREEN and e[18, 100] == BLUE
assert (e[:, 112:] != 0).any(), "no page label"
lit = [int(v) for v in e[:, 112:][e[:, 112:] != 0]]
# Grey, allowing the rounding of an antialiased shade into 5 and 6 bits.
assert all((v >> 11) == (v & 31) and abs(((v >> 5) & 63) - 2 * (v >> 11)) <= 2 for v in lit), \
    "the label is not grey"
print("make_book: layouts, pixels and labels PASS")
PY

cc -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_tang_book.c" "$root/ports/bl616/tang_book.c" -o "$work/tb"
"$work/tb" "$work/a.trb" 3 64 36
