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
# Page 3 is a two-page scan, wider than tall: blue on the left, cyan on the right.
for i, c in enumerate([(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255)]):
    im = Image.new("RGB", (400, 300) if i == 2 else (200, 300), c)
    if i == 2:
        im.paste((0, 255, 255), (200, 0, 400, 300))
    im.save(f"{work}/wide/w{i + 1}.png")
# One page, red above its middle row and green below, for the zoom halves.
os.makedirs(f"{work}/split")
im = Image.new("RGB", (200, 300), (255, 0, 0))
im.paste((0, 255, 0), (0, 150, 200, 300))
im.save(f"{work}/split/s1.png")
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
python3 "$mb" "$work/pages" "$work/c.trb" --size 64x36 --title "Test Book" --no-cover --no-labels --no-zoom > /dev/null
python3 "$mb" "$work/pages" "$work/d.trb" --size 64x36 --title "Test Book" --rtl --no-labels > /dev/null
python3 "$mb" "$work/pages" "$work/e.trb" --size 160x36 --title "Test Book" > /dev/null
python3 "$mb" "$work/wide" "$work/f.trb" --size 64x36 --title "Test Book" --no-labels > /dev/null
python3 "$mb" "$work/split" "$work/g.trb" --size 64x36 --title "Test Book" --no-labels > /dev/null
python3 "$mb" "$work/split" "$work/h.trb" --size 64x36 --title "Test Book" --no-labels --zoom-overlap 0 > /dev/null

python3 - "$work" <<'PY'
import struct, sys
import numpy as np
work = sys.argv[1]
zoom = {}
def load(name):
    d = open(f"{work}/{name}", "rb").read()
    h = struct.unpack("<4sHHHHHHIIHHI64sIHH24s", d[:128])
    magic, ver, hb, w, ht, fmt, n, off, sb, pages, flags = h[:11]
    zoff, zn, zov = h[13:16]
    assert (magic, ver, hb, fmt) == (b"TRB1", 1, 128, 1), h[:4]
    assert (w, ht, sb) == (64, 36, 64 * 36 * 2)
    assert off % 4096 == 0 and off >= 128 + 8 * (n + zn)
    assert h[12].rstrip(b"\0") == b"Test Book"
    if zn:
        assert zoff % 4096 == 0 and zoff >= off + n * sb and len(d) == zoff + zn * sb
    else:
        assert zoff == 0 and len(d) == off + n * sb
    table = [struct.unpack("<HHI", d[128 + 8 * i:136 + 8 * i])[:2] for i in range(n)]
    spreads = [np.frombuffer(d[off + i * sb:off + (i + 1) * sb], ">u2").reshape(36, 64) for i in range(n)]
    zt = 128 + 8 * n
    zoom[name] = ([struct.unpack("<HBBHH", d[zt + 8 * i:zt + 8 * i + 8])[:4] for i in range(zn)],
                  [np.frombuffer(d[zoff + i * sb:zoff + (i + 1) * sb], ">u2").reshape(36, 64)
                   for i in range(zn)], zov)
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
assert s[2][18, 9] == BLUE and s[2][18, 54] == c565(0, 255, 255)   # 48 wide, both halves
assert s[2][18, 4] == 0 and s[2][18, 60] == 0
assert s[1][18, 32] == GREEN and s[1][18, 10] == 0                         # alone, centred
# Zoom views: every page's top then bottom, in reading order, each naming its
# spread; a page fills the screen's height at about twice a spread's scale.
views, z, zov = zoom["a.trb"]
assert zov == 50
assert views == [(1, 0, 0, 0), (1, 0, 1, 0), (2, 0, 0, 1), (2, 0, 1, 1), (3, 0, 0, 1),
                 (3, 0, 1, 1), (4, 0, 0, 2), (4, 0, 1, 2), (5, 0, 0, 2), (5, 0, 1, 2)], views
assert z[0][18, 32] == RED and z[1][18, 32] == RED and z[4][18, 32] == BLUE
assert z[0][18, 5] == 0 and z[0][18, 58] == 0                  # 46 wide, centred
assert (z[0][:, 12:52] != 0).all()                             # and the full height
# A wide page is two pages, its left half then its right, each split.
views, z, _ = zoom["f.trb"]
assert views[4:8] == [(3, 1, 0, 2), (3, 1, 1, 2), (3, 2, 0, 2), (3, 2, 1, 2)], views
CYAN = c565(0, 255, 255)
assert z[4][18, 32] == BLUE and z[6][18, 32] == CYAN and len(views) == 12
# --no-zoom: no views.
assert zoom["c.trb"][0] == []
# The overlap: with 5%, the top half reaches past the middle into the green
# and the bottom half back into the red; with none, each is one colour.
GREENc = c565(0, 255, 0)
load("g.trb"); load("h.trb")
_, z, zov = zoom["g.trb"]
assert z[0][2, 32] == RED and z[0][35, 32] == GREENc and z[1][0, 32] == RED and z[1][33, 32] == GREENc
_, z, zov = zoom["h.trb"]
assert zov == 0
assert set(np.unique(z[0][:, 20:44])) == {RED} and set(np.unique(z[1][:, 20:44])) == {GREENc}
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
"$work/tb" "$work/a.trb" 3 64 36 10
"$work/tb" "$work/c.trb" 3 64 36 0
