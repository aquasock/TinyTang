#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Make a TinyReader book (.trb) from a PDF, a CBZ or CBR, or a folder of images.

TinyReader shows a book as two-page spreads, full screen on the desktop core's
1280x720 pixel plane, and turns pages by switching between spreads it has
already loaded into the core's SDRAM (desktop ABI 1.11).  The board does no
decoding or scaling at all: this tool renders every spread at exactly the
plane's size, in the plane's own pixel format and byte order, so the BL616
copies the file's bytes straight into the link.

    tools/make_book.py "Player's Guide.pdf" zelda.trb
    tools/make_book.py comic.cbz comic.trb --title "Comic 1"
    tools/make_book.py scans/ book.trb --no-cover      # pages paired from page 1

Layout.  Like a printed book: the cover alone, then pages 2-3, 4-5 and so on,
even pages on the left (--no-cover pairs from page 1; --rtl puts the first of
each pair on the right, for manga; --single shows one page per screen).  A
page wider than it is tall is already a spread -- a fold-out, or two pages
scanned as one -- so it is shown alone, as large as the screen allows, a page
left waiting before it is shown alone too, and pairing starts again after it.  Each
spread is scaled to fill the height, the two pages meeting in the middle,
with a high-quality (Lanczos) filter, and rounded to RGB565.  Unless
--no-labels is given, the page numbers and the count go small and grey in the
black margin at the bottom right.

Needs Pillow and numpy; a PDF needs poppler's pdftoppm and pdfinfo, a CBR
needs 7z (p7zip).

File format (version 1).  Little-endian header fields; the pixels are
big-endian RGB565, as the link sends them.

    offset  size  field
         0     4  magic "TRB1"
         4     2  version, 1
         6     2  header bytes, 128
         8     2  width in pixels (1280 for the desktop core)
        10     2  height in pixels (720)
        12     2  pixel format: 1 = RGB565, big-endian, rows top to bottom
        14     2  spread count, N
        16     4  data offset: where spread 0 starts, a multiple of 4096
        20     4  spread bytes: width * height * 2
        24     2  source page count
        26     2  flags: bit 0 cover alone, bit 1 right to left, bit 2 single pages
        28     4  reserved, 0
        32    64  title, UTF-8, NUL padded
        96    32  reserved, 0
       128  8*N  spread table: left page, right page (u16 each, 1-based source
                 pages, 0 for none), 4 reserved bytes
    data offset  spread i at data offset + i * spread bytes

Every spread is a whole number of 512-byte sectors (1280 * 720 * 2 is 3600 of
them) and starts on a 4096-byte boundary, so the card is read in whole,
aligned sectors.
"""

import argparse
import concurrent.futures
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

try:
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("make_book needs Pillow and numpy: pip install pillow numpy")

MAGIC = b"TRB1"
VERSION = 1
HEADER_BYTES = 128
ENTRY_BYTES = 8
ALIGN = 4096
FORMAT_RGB565_BE = 1
FLAG_COVER, FLAG_RTL, FLAG_SINGLE = 1, 2, 4
IMAGE_EXT = (".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".tif", ".tiff")


def natural_key(name):
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", name)]


class Pages:
    """The source's pages, loaded one at a time when asked for."""

    def __init__(self, source, work, dpi):
        self.work = work
        self.dpi = dpi
        self.pdf = None
        self.files = []
        low = source.lower()
        self.sizes = {}
        if os.path.isdir(source):
            self.files = self._images_in(source)
        elif low.endswith(".pdf"):
            self.pdf = source
            info = subprocess.run(["pdfinfo", source], capture_output=True, text=True, check=True)
            m = re.search(r"^Pages:\s+(\d+)", info.stdout, re.M)
            if not m:
                sys.exit(f"pdfinfo found no page count in {source}")
            self.count = int(m.group(1))
            # Every page's size, for telling a two-page scan from a page.
            info = subprocess.run(["pdfinfo", "-f", "1", "-l", str(self.count), source],
                                  capture_output=True, text=True, check=True)
            for n, w, h, rot in re.findall(
                    r"^Page\s+(\d+) size:\s+([\d.]+) x ([\d.]+).*?\n(?:Page\s+\d+ rot:\s+(\d+))?",
                    info.stdout, re.M):
                self.sizes[int(n)] = ((float(h), float(w)) if rot in ("90", "270")
                                      else (float(w), float(h)))
            return
        elif low.endswith((".cbz", ".zip")):
            out = os.path.join(work, "src")
            with zipfile.ZipFile(source) as z:
                z.extractall(out)
            self.files = self._images_in(out)
        elif low.endswith((".cbr", ".rar", ".cb7", ".7z")):
            if not shutil.which("7z"):
                sys.exit("a CBR needs 7z (p7zip) to unpack")
            out = os.path.join(work, "src")
            subprocess.run(["7z", "x", "-y", f"-o{out}", source], check=True,
                           stdout=subprocess.DEVNULL)
            self.files = self._images_in(out)
        else:
            sys.exit(f"{source}: not a PDF, CBZ, CBR or folder of images")
        if not self.files:
            sys.exit(f"{source}: no pages found")
        self.count = len(self.files)

    @staticmethod
    def _images_in(folder):
        found = []
        for root, _dirs, names in os.walk(folder):
            for n in names:
                if n.lower().endswith(IMAGE_EXT) and not n.startswith("."):
                    found.append(os.path.join(root, n))
        return sorted(found, key=lambda p: natural_key(os.path.relpath(p, folder)))

    def size(self, n):
        """Page n's width and height, in whatever unit the source has."""
        if n in self.sizes:
            return self.sizes[n]
        if self.pdf is None:
            with Image.open(self.files[n - 1]) as im:
                self.sizes[n] = im.size
            return self.sizes[n]
        return (1.0, 1.0)

    def wide(self, n):
        w, h = self.size(n)
        return w > h

    def page(self, n):
        """Page n (1-based) as an RGB image."""
        if self.pdf is None:
            with Image.open(self.files[n - 1]) as im:
                return im.convert("RGB")
        prefix = os.path.join(self.work, f"p{n:05d}")
        subprocess.run(["pdftoppm", "-f", str(n), "-l", str(n), "-r", str(self.dpi),
                        "-png", "-singlefile", self.pdf, prefix], check=True)
        with Image.open(prefix + ".png") as im:
            img = im.convert("RGB")
        os.remove(prefix + ".png")
        return img


def plan_spreads(count, cover, rtl, single, wide=lambda n: False):
    """[(left, right)] in reading order, 1-based pages, 0 for none.  A wide
    page is a spread of its own, and pairing starts again after it."""
    if single:
        return [(n, 0) for n in range(1, count + 1)]
    spreads = []
    n = 1
    if cover and count:
        spreads.append((1, 0))
        n = 2
    waiting = 0
    for p in range(n, count + 1):
        if wide(p):
            if waiting:
                spreads.append((waiting, 0))
                waiting = 0
            spreads.append((p, 0))
        elif not waiting:
            waiting = p
        else:
            spreads.append((p, waiting) if rtl else (waiting, p))
            waiting = 0
    if waiting:
        spreads.append((waiting, 0))
    return spreads


def to_rgb565_be(img):
    a = np.asarray(img, dtype=np.uint16)
    r = (a[..., 0] * 31 + 127) // 255
    g = (a[..., 1] * 63 + 127) // 255
    b = (a[..., 2] * 31 + 127) // 255
    return ((r << 11) | (g << 5) | b).astype(">u2").tobytes()


def render_spread(pages, pair, w, h, labels, total):
    canvas = Image.new("RGB", (w, h), (0, 0, 0))
    imgs = [pages.page(p) for p in pair if p]
    if len(imgs) == 1:
        im = imgs[0]
        s = min(h / im.height, w / im.width)
        im = im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))), Image.LANCZOS)
        canvas.paste(im, ((w - im.width) // 2, (h - im.height) // 2))
        right_edge = (w + im.width) // 2
    else:
        # Each page fills the height of its half, so facing pages match on
        # screen even when the scans differ in resolution; they meet in the
        # middle.
        def fit(im):
            s = min(h / im.height, (w // 2) / im.width)
            return im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))),
                             Image.LANCZOS)
        left, right = [fit(im) for im in imgs]
        canvas.paste(left, (w // 2 - left.width, (h - left.height) // 2))
        canvas.paste(right, (w // 2, (h - right.height) // 2))
        right_edge = w // 2 + right.width
    if labels:
        nums = [str(p) for p in sorted(p for p in pair if p)]
        text = f"{'-'.join(nums)}/{total}"
        font = ImageFont.load_default()
        draw = ImageDraw.Draw(canvas)
        x0, y0, x1, y1 = draw.textbbox((0, 0), text, font=font)
        tw, th = x1 - x0, y1 - y0
        if w - right_edge >= tw + 8:
            draw.text((w - tw - 4 - x0, h - th - 4 - y0), text, fill=(110, 110, 110), font=font)
    return to_rgb565_be(canvas)


def header(w, h, spreads, pages, flags, title):
    n = len(spreads)
    data_offset = -(-(HEADER_BYTES + ENTRY_BYTES * n) // ALIGN) * ALIGN
    t = title.encode("utf-8")[:63]
    head = struct.pack("<4sHHHHHHIIHHI64s32s", MAGIC, VERSION, HEADER_BYTES, w, h,
                       FORMAT_RGB565_BE, n, data_offset, w * h * 2, pages, flags, 0,
                       t.ljust(64, b"\0"), b"\0" * 32)
    assert len(head) == HEADER_BYTES
    table = b"".join(struct.pack("<HHI", l, r, 0) for l, r in spreads)
    return (head + table).ljust(data_offset, b"\0")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", help="a .pdf, .cbz, .cbr or a folder of images")
    ap.add_argument("output", help="the .trb to write")
    ap.add_argument("--title", help="the book's title (default: the source's name)")
    ap.add_argument("--no-cover", action="store_true", help="pair pages from page 1")
    ap.add_argument("--rtl", action="store_true", help="right to left: the first of a pair on the right")
    ap.add_argument("--single", action="store_true", help="one page per screen")
    ap.add_argument("--no-labels", action="store_true", help="no page numbers in the margin")
    ap.add_argument("--size", default="1280x720", help="spread size (default 1280x720, the plane's)")
    ap.add_argument("--dpi", type=int, default=150, help="PDF render resolution (default 150)")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 2, help="spreads rendered at once")
    args = ap.parse_args()

    w, h = (int(v) for v in args.size.lower().split("x"))
    title = args.title or os.path.splitext(os.path.basename(args.source.rstrip("/")))[0]
    flags = ((0 if args.no_cover or args.single else FLAG_COVER) |
             (FLAG_RTL if args.rtl else 0) | (FLAG_SINGLE if args.single else 0))
    with tempfile.TemporaryDirectory() as work:
        pages = Pages(args.source, work, args.dpi)
        spreads = plan_spreads(pages.count, flags & FLAG_COVER, args.rtl, args.single, pages.wide)
        if len(spreads) > 0xFFFF:
            sys.exit("too many spreads for one book")
        tmp_out = args.output + ".part"
        with open(tmp_out, "wb") as out, \
                concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
            out.write(header(w, h, spreads, pages.count, flags, title))
            done = 0
            for data in pool.map(lambda pair: render_spread(pages, pair, w, h,
                                                            not args.no_labels, pages.count),
                                 spreads):
                out.write(data)
                done += 1
                if done % 10 == 0 or done == len(spreads):
                    print(f"  {done} of {len(spreads)} spreads", flush=True)
        os.replace(tmp_out, args.output)
    size = os.path.getsize(args.output)
    print(f"{args.output}: {title!r}, {pages.count} pages in {len(spreads)} spreads of "
          f"{w}x{h}, {size / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
