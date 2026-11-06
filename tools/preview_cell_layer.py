#!/usr/bin/env python3
"""Preview what the desk cell layer puts on a 1280x720 screen.

Renders the four questions worth asking about image support in TinyWiki:

  01-walkthrough   text and a line-art figure, as a walkthrough page
  02-photo         a photograph, three ways
  03-text          text at 1:1 and magnified, for judging sharpness
  04-screenshot    the worst case, where most pixels are text

Everything is derived from facts in this tree rather than assumed:

  * 80 cells by 45, each 16x16 pixels at 1280x720
    (fpga/desktop/textdisp_wide.sv: COLS=80, ROWS=45; 1280/80 = 16)
  * one glyph per cell, an 8x8 bitmap drawn at 2x
  * glyph bit 0 is the leftmost column
    (.ai/core-reference.md PROT-004)
  * foreground and background are each 15-bit RGB555, chosen per cell
    (fpga/ae350_soc/desk_link.sv command 0x14: char, fg, bg)

The block-glyph mode is a proposal, not an existing feature.  It spends the
33 blank glyph slots (0x00-0x1F and 0x7F) on the 16 quadrant patterns, which
gives a 160x90 image grid with two colours per cell.

Usage:  python3 tools/preview_cell_layer.py [--out DIR]
"""

import argparse
import re
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
COLS, ROWS = 80, 45
CELL = 16
W, H = COLS * CELL, ROWS * CELL          # 1280 x 720
IMG_W, IMG_H = COLS * 2, ROWS * 2        # 160 x 90 image grid

INK = (222, 218, 205)
PAPER = (16, 14, 13)
BAR = (44, 42, 40)
DIM = (150, 150, 150)
ACCENT = (120, 190, 240)


# --------------------------------------------------------------- font

def load_font():
    """The desktop's own glyph table, 128 glyphs of 8 rows."""
    src = (ROOT / "fpga/desktop/font.vh").read_text()
    body = src.split("'{", 1)[1]
    rows = [r for r in re.findall(r"'\{([^}]*)\}", body) if "8'h" in r]
    font = [[int(v, 16) for v in re.findall(r"8'h([0-9a-fA-F]{2})", r)]
            for r in rows]
    if len(font) != 128 or any(len(g) != 8 for g in font):
        raise SystemExit(f"font.vh: expected 128 glyphs of 8 rows, "
                         f"got {len(font)}")
    return font


FONT = load_font()


def q555(c):
    """Quantise to RGB555, the cell's real colour depth."""
    r, g, b = (v >> 3 for v in c)
    return ((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2))


# --------------------------------------------------------------- canvas

class Frame:
    def __init__(self):
        self.img = Image.new("RGB", (W, H), q555(PAPER))
        self.px = self.img.load()

    def _plot(self, cx, cy, bits, fg, bg):
        fg, bg = q555(fg), q555(bg)
        x0, y0 = cx * CELL, cy * CELL
        for y in range(8):
            row = bits[y]
            for x in range(8):
                col = fg if (row >> x) & 1 else bg
                for dy in range(2):
                    for dx in range(2):
                        self.px[x0 + x * 2 + dx, y0 + y * 2 + dy] = col

    def cell(self, cx, cy, ch, fg, bg):
        self._plot(cx, cy, FONT[ord(ch) & 0x7F], fg, bg)

    def text(self, cx, cy, s, fg=INK, bg=None):
        bg = PAPER if bg is None else bg
        for i, ch in enumerate(s):
            if cx + i >= COLS:
                break
            self.cell(cx + i, cy, ch, fg, bg)

    def fill(self, cx, cy, cw, ch, colour):
        colour = q555(colour)
        for y in range(cy * CELL, min(H, (cy + ch) * CELL)):
            for x in range(cx * CELL, min(W, (cx + cw) * CELL)):
                self.px[x, y] = colour


# the 16 quadrant patterns, as 8x8 bitmaps of 4x4 blocks
def _quad(mask):
    bits = []
    for y in range(8):
        row = 0
        for x in range(8):
            q = (0 if y < 4 else 2) + (0 if x < 4 else 1)
            if (mask >> q) & 1:
                row |= 1 << x
        bits.append(row)
    return bits


QUADS = [_quad(m) for m in range(16)]


def blit_two_colour(frame, img, cx, cy, cw, ch):
    """160x90 grid: the best two colours a cell can hold, plus a quadrant."""
    for j in range(ch):
        for i in range(cw):
            samples = []
            for dy in (0, 1):
                for dx in (0, 1):
                    sx, sy = i * 2 + dx, j * 2 + dy
                    samples.append(img.getpixel((sx, sy))
                                   if sx < img.width and sy < img.height
                                   else (0, 0, 0))
            lum = [0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2] for c in samples]
            lo, hi = min(lum), max(lum)
            if hi - lo < 6:
                flat = tuple(sum(s[k] for s in samples) // 4 for k in range(3))
                fg = bg = flat
                mask = 15
            else:
                on = [v >= (lo + hi) / 2 for v in lum]
                mask = sum(1 << k for k in range(4) if on[k])
                def mean(sel):
                    sel = [samples[k] for k in range(4) if sel[k]]
                    n = len(sel) or 1
                    return tuple(sum(s[k] for s in sel) // n for k in range(3))
                fg, bg = mean(on), mean([not o for o in on])
            frame._plot(cx + i, cy + j, QUADS[mask], fg, bg)


def blit_flat(frame, img, cx, cy, cw, ch):
    """80x45 grid: one RGB555 colour per cell, no glyph detail."""
    for j in range(ch):
        for i in range(cw):
            c = img.getpixel((i, j))
            frame._plot(cx + i, cy + j, [0xFF] * 8, c, c)


RAMP = " .:-=+*#%@"


def blit_ascii(frame, img, cx, cy, cw, ch, fg=(190, 190, 190)):
    """80x45 grid with today's 128-glyph font only."""
    for j in range(ch):
        for i in range(cw):
            c = img.getpixel((i, j))
            lum = 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]
            frame.cell(cx + i, cy + j,
                       RAMP[min(len(RAMP) - 1, int(lum / 256 * len(RAMP)))],
                       fg, (6, 6, 6))


# --------------------------------------------------------------- sources

def crop_to(im, box):
    return im.crop(box) if box else im


def dungeon_map(size):
    """A walkthrough figure of the kind this would carry: line art, flat fills.

    Drawn here rather than read from a game, so it is a stand-in for a real
    map and not a reproduction of one.
    """
    w, h = size
    im = Image.new("RGB", (w, h), (8, 8, 10))
    d = ImageDraw.Draw(im)
    sx, sy = w / 160.0, h / 90.0

    def box(x1, y1, x2, y2, fill=None, outline=(215, 210, 190)):
        d.rectangle([x1 * sx, y1 * sy, x2 * sx, y2 * sy],
                    fill=fill, outline=outline)

    box(6, 38, 44, 66, (26, 30, 40))
    box(52, 22, 92, 50, (26, 30, 40))
    box(100, 34, 138, 70, (26, 30, 40))
    box(58, 58, 96, 84, (26, 30, 40))
    for c in ((44, 48, 52, 54), (92, 34, 100, 40), (74, 50, 80, 58)):
        box(*c, (26, 30, 40))
    for y in range(60, 84, 3):
        d.line([(60 * sx, y * sy), (94 * sx, y * sy)], fill=(50, 90, 140))
    for cx, cy in ((16, 48), (30, 60), (70, 30), (120, 46), (128, 60)):
        d.rectangle([(cx - 2) * sx, (cy - 2) * sy,
                     (cx + 2) * sx, (cy + 2) * sy], fill=(220, 180, 60))
    for i in range(5):
        d.line([(64 + i * 3) * sx, (40 + i * 2) * sy,
                (66 + i * 3) * sx, (40 + i * 2) * sy], fill=(200, 120, 220))
    return im


def picture(name, size, box=None):
    im = Image.open(ROOT / "docs/images" / name).convert("RGB")
    return crop_to(im, box).resize(size, Image.LANCZOS)


# --------------------------------------------------------------- pages

def page_walkthrough(out):
    f = Frame()
    f.fill(0, 0, COLS, 1, BAR)
    f.text(1, 0, "TinyWiki", (250, 250, 250), BAR)
    f.text(11, 0, "- Zelda: A Link to the Past - Chapter 3", DIM, BAR)
    f.text(70, 0, "F1 Help  F11 Size", DIM, BAR)

    f.text(2, 2, "3.  THE EASTERN PALACE", ACCENT)
    f.text(2, 3, "======================", (70, 110, 140))

    for i, line in enumerate([
            "Take the chest by the entrance, then push the block",
            "on the left wall to open the passage north.  The map",
            "below is the ground floor; the stairs to the basement",
            "are in the far right room, past the water.",
            "",
            "Watch for the blue Darknuts in the east wing.  Their",
            "shield faces you until they turn, so bait the charge",
            "and strike from behind."]):
        f.text(2, 5 + i, line)

    f.fill(0, 14, COLS, 26, (0, 0, 0))
    blit_two_colour(f, dungeon_map((160, 52)), 0, 14, COLS, 26)

    f.text(2, 41, "Chests are yellow.  Stairs, purple.  Water blocks", DIM)
    f.text(2, 42, "the north exit until the level 2 raft is found.", DIM)

    f.fill(0, 44, COLS, 1, BAR)
    f.text(1, 44, "70 cols  |  8,425,137 articles  |  ENTER open  ESC back",
           DIM, BAR)
    f.img.save(out / "01-walkthrough.png")


def page_photo(out):
    f = Frame()
    f.fill(0, 0, COLS, 1, BAR)
    f.text(1, 0, "A photograph through the cell layer", (250, 250, 250), BAR)

    src = picture("setup.jpg", (IMG_W, IMG_H), (0, 380, 1446, 1194))
    f.text(2, 2, "Two colours per cell, 160x90", (200, 200, 200))
    blit_two_colour(f, src.resize((74, 38), Image.LANCZOS), 2, 4, 37, 19)
    f.text(2, 24, "One RGB555 per cell, 80x45", (200, 200, 200))
    blit_flat(f, src.resize((37, 19), Image.LANCZOS), 2, 26, 37, 19)

    f.text(42, 2, "The source at 160x90", (200, 200, 200))
    f.img.paste(src.resize((37 * CELL, 19 * CELL), Image.LANCZOS),
                (42 * CELL, 4 * CELL))
    f.img.save(out / "02-photo.png")


def page_text(out):
    f = Frame()
    f.text(1, 1, "HOW SHARP IS TEXT AT 1280x720?", (240, 240, 240))
    f.text(1, 3, "A row is 16 px tall on a 720 px screen, so there are 45")
    f.text(1, 4, "rows.  A glyph is an 8x8 bitmap drawn at 2x, so one glyph")
    f.text(1, 5, "pixel is 2x2 screen pixels.  Bit 0 is the left column.")
    f.text(1, 7, "Illegal1i  rn/m  O0  l1  B8  S5  Z2  gq  ,.  \"'",
           (200, 230, 255))
    f.text(1, 9, "The quick brown fox jumps over the lazy dog. 0123456789")
    f.text(1, 11, "Above is 1:1.  Below is 2x, of the same words.")
    f.img.paste(f.img.crop((0, 6 * CELL, 46 * CELL, 10 * CELL))
                .resize((46 * CELL * 2, 4 * CELL * 2), Image.NEAREST),
                (0, 13 * CELL))
    f.img.save(out / "03-text.png")


def page_screenshot(out):
    f = Frame()
    f.fill(0, 0, COLS, 1, BAR)
    f.text(1, 0, "Worst case: a screenshot, where most pixels are text",
           (250, 250, 250), BAR)
    blit_two_colour(f, picture("tinytang-desktop.png", (IMG_W, IMG_H)),
                    0, 1, COLS, 42)
    f.text(50, 43, "160x90 block glyphs", (200, 200, 200))
    f.img.save(out / "04-screenshot.png")


def page_ascii(out):
    """Today's font, on the same screenshot: the baseline it improves on."""
    f = Frame()
    f.fill(0, 0, COLS, 1, BAR)
    f.text(1, 0, "Today's font only: an ASCII ramp", (250, 250, 250), BAR)
    blit_ascii(f, picture("tinytang-desktop.png", (COLS, 44)), 0, 1, COLS, 43)
    f.img.save(out / "05-ascii-only.png")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", default=str(ROOT / "build/cell-preview"),
                    help="where to write the PNGs")
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    for page, name in ((page_walkthrough, "01-walkthrough"),
                       (page_photo, "02-photo"),
                       (page_text, "03-text"),
                       (page_screenshot, "04-screenshot"),
                       (page_ascii, "05-ascii-only")):
        page(out)
        print(f"wrote {out}/{name}.png")
    print(f"\n{W}x{H}, {COLS}x{ROWS} cells of {CELL}x{CELL} px")


if __name__ == "__main__":
    main()
