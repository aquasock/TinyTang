#!/usr/bin/env python3
"""Preview a real bitmap layer: every pixel of 1280x720, greyscale.

Not to be confused with tools/preview_cell_layer.py, which shows what the
existing 80x45 character-cell layer can do.  This one shows what a framebuffer
in the FPGA's block RAM would do, and it is bounded by measured resources:

  * the device has 340 BSRAM blocks of 18 Kbit = 765 KB on chip
    (.ai/core-reference.md PSX-002)
  * TinyNES builds to 66 of those 340, leaving 274 blocks = 616 KB
    (build/ae350_soc/nextpnr.log, Device utilisation)
  * the BL616 link is 2 Mbaud 8N1 = 200 KB/s at the wire, about 85 KB/s
    measured one-wire (PROT-003, PSX-006)

So 1280x720 at 4 bits per pixel is 460,800 bytes, 200 blocks: it fits.
1280x720 at 8 bits is 921,600 bytes, 400 blocks: it does not.

Every image here is dithered or quantised to the real depth and drawn at the
real 1280x720, so what you see is what the panel would show.

Usage:  python3 tools/preview_bitmap_layer.py [--out DIR]
"""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
W, H = 1280, 720

# Font files are not part of this repository, so resolve them at run time and
# fall back to Pillow's built-in bitmap font rather than failing.  The previews
# are for judging the layer, not for matching a particular typeface.
_FONT_CANDIDATES = {
    False: ["DejaVuSans.ttf", "LiberationSans-Regular.ttf",
            "NotoSans-Regular.ttf", "Arial.ttf"],
    True: ["DejaVuSansMono.ttf", "LiberationMono-Regular.ttf",
           "CourierNew.ttf"],
}


def _find(mono):
    import glob
    import os
    for name in _FONT_CANDIDATES[mono]:
        for root in ("/usr/share/fonts", "/usr/local/share/fonts",
                     os.path.expanduser("~/.fonts")):
            hits = glob.glob(os.path.join(root, "**", name), recursive=True)
            if hits:
                return hits[0]
    return None


_font_cache = {}


def font(size, mono=False):
    """A TrueType face if one is installed, else Pillow's default."""
    key = (size, mono)
    if key not in _font_cache:
        path = _find(mono)
        try:
            _font_cache[key] = (ImageFont.truetype(path, size) if path
                                else ImageFont.load_default())
        except OSError:
            _font_cache[key] = ImageFont.load_default()
    return _font_cache[key]

INK = (222, 218, 205)
PAPER = (16, 14, 13)
BAR = (44, 42, 40)
DIM = (150, 150, 150)


# ------------------------------------------------------------- quantisers

def to_grey(im):
    return im.convert("L")


def quantise(im, bits):
    """Reduce an 8-bit grey image to `bits` levels, per pixel."""
    levels = (1 << bits) - 1
    return im.point(lambda v: round(v / 255 * levels) * 255 // levels)


def floyd_steinberg(im):
    """1-bit with error diffusion: the classic newspaper halftone."""
    im = im.convert("L")
    px = im.load()
    w, h = im.size
    err = [[0.0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            old = px[x, y] + err[y][x]
            new = 255 if old >= 128 else 0
            px[x, y] = new
            e = old - new
            if x + 1 < w:
                err[y][x + 1] += e * 7 / 16
            if y + 1 < h:
                if x > 0:
                    err[y + 1][x - 1] += e * 3 / 16
                err[y + 1][x] += e * 5 / 16
                if x + 1 < w:
                    err[y + 1][x + 1] += e * 1 / 16
    return im


def bayer4(im):
    """1-bit ordered dither: a regular 4x4 matrix, crisper than FS."""
    m = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
    im = im.convert("L")
    px = im.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            thr = (m[y % 4][x % 4] + 0.5) * 255 / 16
            px[x, y] = 255 if px[x, y] > thr else 0
    return im


# ------------------------------------------------------------- sources

def photo():
    im = Image.open(ROOT / "docs/images/setup.jpg").convert("RGB")
    return im.crop((0, 380, 1446, 1194)).resize((W, H), Image.LANCZOS)


def desktop():
    return Image.open(ROOT / "docs/images/tinytang-desktop.png") \
                .convert("RGB").resize((W, H), Image.LANCZOS)


def dungeon_map(size):
    """A walkthrough figure: line art, so it survives quantisation intact."""
    w, h = size
    im = Image.new("RGB", (w, h), (10, 10, 12))
    d = ImageDraw.Draw(im)
    sx, sy = w / 640.0, h / 360.0

    def box(x1, y1, x2, y2, fill=None, outline=(225, 220, 200), wid=3):
        d.rectangle([x1 * sx, y1 * sy, x2 * sx, y2 * sy], fill=fill,
                    outline=outline, width=wid)

    rooms = [(24, 152, 176, 264), (208, 88, 368, 200),
             (400, 136, 552, 280), (232, 232, 384, 336)]
    for r in rooms:
        box(*r, fill=(30, 34, 46))
    for c in ((176, 192, 208, 216), (368, 136, 400, 160), (296, 200, 320, 232)):
        box(*c, fill=(30, 34, 46))
    for y in range(240, 336, 12):
        d.line([(240 * sx, y * sy), (376 * sx, y * sy)],
               fill=(50, 90, 140), width=2)
    for cx, cy in ((64, 192), (120, 240), (280, 120), (480, 184), (512, 240)):
        d.rectangle([(cx - 9) * sx, (cy - 9) * sy,
                     (cx + 9) * sx, (cy + 9) * sy], fill=(225, 185, 60))
    for i in range(7):
        d.line([(256 + i * 12) * sx, (160 + i * 8) * sy,
                (268 + i * 12) * sx, (160 + i * 8) * sy],
               fill=(200, 120, 220), width=3)
    return im


# ------------------------------------------------------------- pages

def page_dither(out):
    """1 bpp at full 1280x720: the cheapest way to own every pixel."""
    src = to_grey(photo())
    f = Image.new("L", (W, H), 0)
    half = H // 2
    f.paste(bayer4(src.crop((0, 0, W, half))), (0, 0))
    f.paste(floyd_steinberg(src.crop((0, half, W, H))), (0, half))
    f = f.convert("RGB")
    d = ImageDraw.Draw(f)
    face = font(20)
    d.rectangle([0, 0, W, 30], fill=PAPER)
    d.text((10, 5), "1280x720, 1 bit per pixel - 50 BSRAM blocks, 0.6 s to load",
           font=face, fill=INK)
    d.text((10, half + 8), "ordered 4x4 (top)", font=font(18),
           fill=INK)
    d.text((10, half + 30), "Floyd-Steinberg (bottom)",
           font=font(18), fill=INK)
    f.save(out / "01-dither-1bpp-full.png")


def page_grey(out):
    """4 bpp at full 1280x720: 16 greys, every pixel."""
    src = to_grey(photo())
    f = Image.new("RGB", (W, H))
    bands = [1, 2, 3, 4]
    bh = H // len(bands)
    for i, bits in enumerate(bands):
        q = quantise(src.crop((0, i * bh, W, (i + 1) * bh)), bits).convert("RGB")
        f.paste(q, (0, i * bh))
        d = ImageDraw.Draw(f)
        blocks = -(-(W * H * bits // 8) // 2304)
        d.rectangle([0, i * bh, 470, i * bh + 30], fill=PAPER)
        d.text((10, i * bh + 5),
               f"{bits} bpp - {1 << bits} levels - {blocks} blocks full frame",
               font=font(19), fill=INK)
    f.save(out / "02-grey-1-2-3-4bpp.png")


def page_text(out):
    """Does small text survive?  The point of owning every pixel."""
    f = Image.new("RGB", (W, H), PAPER)
    d = ImageDraw.Draw(f)
    d.text((28, 20), "Text at 1280x720 with a real font, 4 bpp greyscale",
           font=font(26), fill=INK)
    y = 70
    for size, note in ((11, "11 px - fine print"), (13, "13 px - body"),
                       (16, "16 px - comfortable"), (20, "20 px - heading"),
                       (28, "28 px - title")):
        face = font(size, mono=True)
        d.text((28, y), f"The quick brown fox jumps over the lazy dog. 0123456789",
               font=face, fill=INK)
        d.text((820, y + 2), note, font=font(12), fill=DIM)
        y += size + 20
    # the same text, antialiased off, to show what 1 bpp costs
    d.text((28, y + 10), "Same sizes, 1 bpp (aliased):",
           font=font(18), fill=(200, 200, 200))
    y += 44
    for size in (11, 13, 16):
        face = font(size, mono=True)
        d.text((28, y), "The quick brown fox jumps over the lazy dog.",
               font=face, fill=INK)
        y += size + 14
    f = quantise(to_grey(f), 4).convert("RGB")
    f.save(out / "03-text-4bpp.png")


def page_walkthrough(out):
    """The actual target: crisp small text and a figure on one screen."""
    f = Image.new("RGB", (W, H), PAPER)
    d = ImageDraw.Draw(f)
    body = font(15)
    small = font(12)
    head = font(20)

    d.rectangle([0, 0, W, 30], fill=BAR)
    d.text((10, 6), "TinyWiki", font=font(17), fill=(250, 250, 250))
    d.text((96, 7), "- Zelda: A Link to the Past, Chapter 3", font=small, fill=DIM)
    d.text((W - 210, 7), "F1 Help    F11 Size", font=small, fill=DIM)

    d.text((24, 44), "3.  The Eastern Palace", font=head, fill=(120, 190, 240))
    d.line([(24, 72), (250, 72)], fill=(70, 110, 140), width=1)

    lines = [
        "Take the chest by the entrance, then push the block on the left",
        "wall to open the passage north.  The map below is the ground",
        "floor; the stairs to the basement are in the far right room,",
        "past the water.",
        "",
        "Watch for the blue Darknuts in the east wing.  Their shield",
        "faces you until they turn, so bait the charge and strike from",
        "behind.",
    ]
    y = 86
    for ln in lines:
        d.text((24, y), ln, font=body, fill=INK)
        y += 20

    fig = dungeon_map((640, 360)).convert("RGB")
    fig = quantise(to_grey(fig), 4).convert("RGB")
    f.paste(fig, (W // 2 - 320, y + 14))
    d.text((24, y + 380), "Chests are yellow.  Stairs, purple.  Water blocks the north",
           font=small, fill=DIM)
    d.text((24, y + 398), "exit until the level 2 raft is found.",
           font=small, fill=DIM)

    d.rectangle([0, H - 26, W, H], fill=BAR)
    d.text((10, H - 21), "ENTER open    ESC back    F12 back to the game",
           font=small, fill=DIM)
    f.save(out / "04-walkthrough-4bpp.png")


def page_compare(out):
    """A 640x360 crop at 1:1, so one panel pixel is one framebuffer pixel."""
    src = to_grey(desktop()).crop((320, 180, 960, 540))
    f = Image.new("RGB", (W, H), (0, 0, 0))
    pw, ph = 640, 360
    panels = [(bayer4(src), "1 bpp, ordered dither", 50),
              (quantise(src, 2), "2 bpp, 4 levels", 100),
              (quantise(src, 3), "3 bpp, 8 levels", 150),
              (quantise(src, 4), "4 bpp, 16 levels", 200)]
    for i, (im, label, blocks) in enumerate(panels):
        x, y = (i % 2) * pw, (i // 2) * ph
        f.paste(im.convert("RGB"), (x, y))
        d = ImageDraw.Draw(f)
        d.rectangle([x, y, x + 300, y + 26], fill=PAPER)
        d.text((x + 8, y + 5), f"{label} - {blocks} blocks",
               font=font(15), fill=INK)
    f.save(out / "05-screenshot-ladder.png")


def page_palette(out):
    """4 bpp as a 16-colour palette, which is what a map actually wants."""
    src = photo()
    fig = dungeon_map((640, 360))

    out_img = Image.new("RGB", (W, H), PAPER)
    d = ImageDraw.Draw(out_img)

    # a figure in 16 colours, then the same figure in 16 greys
    for i, (im, label) in enumerate(((fig, "16 colours - keeps the map's coding"),
                                     (quantise(to_grey(fig), 4).convert("RGB"),
                                      "16 greys - loses it"))):
        q = im.quantize(colors=16, method=Image.MEDIANCUT,
                        dither=Image.FLOYDSTEINBERG).convert("RGB")
        out_img.paste(q, (40 + i * 640, 90))
        d.text((40 + i * 640, 60), label,
               font=font(18), fill=INK)

    d.text((40, 24), "4 bpp is a palette, not just greyscale",
           font=font(24), fill=(240, 240, 240))
    d.text((40, 470), "Both are 460,800 bytes, 200 BSRAM blocks, "
                      "about 2.3 s over the link.",
           font=font(17), fill=DIM)
    out_img.save(out / "06-palette-vs-grey.png")



def page_custom(out, path):
    """Any image, at 1280x720, through every depth that matters.

    Writes two files: the full frame at each depth, and a 1:1 crop so small
    text can be judged without the conversation viewer shrinking it.
    """
    src = Image.open(path).convert("RGB").resize((W, H), Image.LANCZOS)
    grey = to_grey(src)

    depths = [(bayer4(grey), "1 bpp, ordered dither", 50),
              (quantise(grey, 2), "2 bpp, 4 levels", 100),
              (quantise(grey, 4), "4 bpp, 16 levels", 200),
              (quantise(grey, 8), "8 bpp, 256 levels", 400)]

    # the full frame, four panels, one per depth
    f = Image.new("RGB", (W, H), (0, 0, 0))
    for i, (im, label, blocks) in enumerate(depths):
        f.paste(im.convert("RGB").resize((W // 2, H // 2), Image.LANCZOS),
                ((i % 2) * (W // 2), (i // 2) * (H // 2)))
    f.save(out / "custom-full.png")

    # 1:1 crop: 320x180 of the original, magnified x4, no resampling
    cx, cy, cw, ch = 180, 250, 400, 200
    zoom = 2
    tiles = []
    for im, label, blocks in depths:
        crop = im.convert("RGB").crop((cx, cy, cx + cw, cy + ch))
        tiles.append(crop.resize((cw * zoom, ch * zoom), Image.NEAREST))
    tile_h = ch * zoom + 34
    g = Image.new("RGB", (cw * zoom * 2, tile_h * 2), (0, 0, 0))
    d = ImageDraw.Draw(g)
    for i, (tile, (_, label, blocks)) in enumerate(zip(tiles, depths)):
        x, y = (i % 2) * (cw * zoom), (i // 2) * tile_h
        d.rectangle([x, y, x + cw * zoom, y + 30], fill=PAPER)
        d.text((x + 8, y + 6), f"{label} - {blocks} blocks",
               font=font(17), fill=INK)
        g.paste(tile, (x, y + 34))
    g.save(out / "custom-crop-1to1.png")


def page_custom_colour(out, path):
    """The same image at 4 bpp as a colour palette, which is what it is."""
    src = Image.open(path).convert("RGB").resize((W, H), Image.LANCZOS)
    f = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(f)

    for i, (label, q) in enumerate((
            ("4 bpp, 16 colours, dithered",
             src.quantize(colors=16, method=Image.MEDIANCUT,
                          dither=Image.FLOYDSTEINBERG).convert("RGB")),
            ("4 bpp, 16 colours, no dither",
             src.quantize(colors=16, method=Image.MEDIANCUT,
                          dither=Image.NONE).convert("RGB")))):
        f.paste(q.resize((W // 2, H // 2), Image.LANCZOS),
                ((i % 2) * (W // 2), 0))
        d.rectangle([(i % 2) * (W // 2), 0, (i % 2) * (W // 2) + 330, 28],
                    fill=PAPER)
        d.text(((i % 2) * (W // 2) + 8, 6), label,
               font=font(16), fill=INK)

    # and the same 16 colours at full frame, top half only, 1:1
    q = src.quantize(colors=16, method=Image.MEDIANCUT,
                     dither=Image.NONE).convert("RGB")
    f.paste(q.crop((0, 0, W, H // 2 - 20)), (0, H // 2 + 20))
    d.rectangle([0, H // 2 + 20, 400, H // 2 + 48], fill=PAPER)
    d.text((8, H // 2 + 26), "the same 16 colours at 1:1",
           font=font(16), fill=INK)
    f.save(out / "custom-colour.png")



def page_fits(out, path):
    """The two options that actually fit in 274 free blocks, head to head.

    4 bpp at 1280x720 is 460,800 bytes = 200 blocks.  Whole pixels, 16
    colours, but coarse tones.
    8 bpp at 960x540 is 518,400 bytes = 225 blocks.  Full tone, but the
    panel has to scale it, so every edge is resampled.
    Both are upscaled to the 1280x720 screen as the hardware would present
    them, and shown 1:1 so the difference is not hidden by the viewer.
    """
    src = Image.open(path).convert("RGB").resize((W, H), Image.LANCZOS)

    a = src.quantize(colors=16, method=Image.MEDIANCUT,
                     dither=Image.NONE).convert("RGB")

    small = src.resize((960, 540), Image.LANCZOS)
    b = small.quantize(colors=256, method=Image.MEDIANCUT,
                       dither=Image.NONE).convert("RGB")
    b = b.resize((W, H), Image.NEAREST)          # how the panel would scale it

    cx, cy, cw, ch = 60, 430, 400, 220
    zoom = 2
    tiles = [(a, "4 bpp at 1280x720 - 200 blocks - every pixel, 16 colours"),
             (b, "8 bpp at 960x540 scaled up - 225 blocks - full tone, resampled")]
    g = Image.new("RGB", (cw * zoom * 2, ch * zoom + 40), (0, 0, 0))
    d = ImageDraw.Draw(g)
    for i, (im, label) in enumerate(tiles):
        x = i * cw * zoom
        d.rectangle([x, 0, x + cw * zoom, 34], fill=PAPER)
        d.text((x + 8, 8), label, font=font(16), fill=INK)
        g.paste(im.crop((cx, cy, cx + cw, cy + ch))
                .resize((cw * zoom, ch * zoom), Image.NEAREST), (x, 36))
    g.save(out / "custom-fits-ab.png")

    # and each at full frame
    for im, name in ((a, "custom-4bpp-full.png"), (b, "custom-8bpp-960.png")):
        im.save(out / name)



def page_one_per_setting(out, path):
    """One full 1280x720 PNG per depth.  No combination panels."""
    src = Image.open(path).convert("RGB").resize((W, H), Image.LANCZOS)
    grey = to_grey(src)

    small = src.resize((960, 540), Image.LANCZOS)
    up8 = small.quantize(colors=256, method=Image.MEDIANCUT,
                         dither=Image.NONE).convert("RGB") \
               .resize((W, H), Image.NEAREST)

    settings = [
        ("a-1bpp-ordered", 50, bayer4(grey).convert("RGB")),
        ("b-1bpp-floyd", 50, floyd_steinberg(grey.copy()).convert("RGB")),
        ("c-2bpp-4level", 100, quantise(grey, 2).convert("RGB")),
        ("d-3bpp-8level", 150, quantise(grey, 3).convert("RGB")),
        ("e-4bpp-16grey", 200, quantise(grey, 4).convert("RGB")),
        ("f-4bpp-16colour", 200,
         src.quantize(colors=16, method=Image.MEDIANCUT,
                      dither=Image.FLOYDSTEINBERG).convert("RGB")),
        ("g-4bpp-16colour-nodither", 200,
         src.quantize(colors=16, method=Image.MEDIANCUT,
                      dither=Image.NONE).convert("RGB")),
        ("h-8bpp-960x540-scaled", 225, up8),
        ("i-8bpp-1280x720", 400, quantise(grey, 8).convert("RGB")),
    ]
    for name, blocks, im in settings:
        fits = "fits" if blocks <= 274 else "DOES NOT FIT"
        im.save(out / f"setting-{name}.png")
        print(f"  setting-{name}.png  {blocks} blocks  {fits}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", default=str(ROOT / "build/bitmap-preview"))
    ap.add_argument("--image", help="render this image instead of the stock set")
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    if args.image:
        page_custom(out, args.image)
        page_custom_colour(out, args.image)
        page_fits(out, args.image)
        page_one_per_setting(out, args.image)
        print(f"wrote {out}/custom-full.png")
        print(f"wrote {out}/custom-crop-1to1.png")
        print(f"wrote {out}/custom-colour.png")
        print(f"wrote {out}/custom-fits-ab.png")
        return

    for fn, name in ((page_dither, "01-dither-1bpp-full"),
                     (page_grey, "02-grey-1-2-3-4bpp"),
                     (page_text, "03-text-4bpp"),
                     (page_walkthrough, "04-walkthrough-4bpp"),
                     (page_compare, "05-screenshot-ladder"),
                     (page_palette, "06-palette-vs-grey")):
        fn(out)
        print(f"wrote {out}/{name}.png")


if __name__ == "__main__":
    main()
