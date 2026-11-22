#!/usr/bin/env python3
"""Render the TinyTang ecosystem's logo PNGs from the master SVG.

The master SVG is kept outside every repository, so it is passed in:

    tools/make_brand_pngs.py /path/to/cat_final.svg OUT_DIR

It writes, into OUT_DIR:

    tinytang-logo-640.png     the whole logo, transparent (README header)
    tinytang-social.png       1280x640 on paper (GitHub social preview)
    tinytang-og.png           1200x630 on paper (the site's Open Graph card)
    tinytang-mark-80.png      the cat's head, transparent (site header, 40 px)
    favicon-16.png, favicon-32.png, favicon-48.png, favicon.ico
    apple-touch-icon.png      180x180 head on paper (iOS fills transparency black)

The head is the master's ears (p1, p2), face (p3) and eyes (path1, path1-9)
alone: the whole logo is noise at favicon sizes.  It is cut from a scratch copy;
the master is only read.

The wordmark is set in AmsterdamTangram by Nick Curtis (1001Fonts Free For
Commercial Use licence).  Its licence forbids publishing the font, so only these
rendered images may be committed, never the .ttf, and never a webfont.  The
script refuses to run without the font installed, because Inkscape would
otherwise render the wordmark in a fallback face without complaint.

Needs Inkscape 1.x (an `inkscape` binary, or the org.inkscape.Inkscape flatpak)
and ImageMagick 7 (`magick`).  Output carries no timestamps or tool metadata, so
the same SVG, font and tools give byte-identical files.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

FONT_FAMILY = "AmsterdamTangram"
PAPER = "#e9eadf"            # the site's --paper
HEAD_IDS = {"p1", "p2", "p3", "path1", "path1-9"}


def inkscape_cmd():
    if shutil.which("inkscape"):
        return ["inkscape"]
    if shutil.which("flatpak"):
        r = subprocess.run(["flatpak", "info", "org.inkscape.Inkscape"],
                           capture_output=True)
        if r.returncode == 0:
            return ["flatpak", "run", "--filesystem=/tmp", "--filesystem=home:ro",
                    "org.inkscape.Inkscape"]
    sys.exit("make_brand_pngs: Inkscape not found (binary or flatpak)")


def check_font():
    r = subprocess.run(["fc-list", ":family=" + FONT_FAMILY, "family"],
                       capture_output=True, text=True)
    if FONT_FAMILY not in r.stdout:
        sys.exit("make_brand_pngs: font %s is not installed; the wordmark would "
                 "render in a fallback face" % FONT_FAMILY)


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("make_brand_pngs: failed: %s\n%s" % (" ".join(cmd), r.stderr))


def render(ink, svg, out, width=None, height=None, drawing=False):
    cmd = ink + [svg, "--export-type=png", "--export-filename=" + out]
    if width:
        cmd.append("--export-width=%d" % width)
    if height:
        cmd.append("--export-height=%d" % height)
    if drawing:
        cmd.append("--export-area-drawing")
    run(cmd)
    if not os.path.exists(out):
        sys.exit("make_brand_pngs: Inkscape wrote nothing for " + out)


# Metadata-free, so a rebuild is byte-identical.
CLEAN = ["-depth", "8", "-strip", "-define", "png:exclude-chunks=date,time,tIME,tEXt,zTXt,iTXt"]


def magick(*args):
    run(["magick"] + list(args))


def head_svg(master_text):
    """The master with the wordmark and every shape but the head removed."""
    s = re.sub(r"(?s)<text\b.*?</text>", "", master_text)

    def keep(m):
        i = re.search(r'\bid="([^"]*)"', m.group(0))
        return m.group(0) if i and i.group(1) in HEAD_IDS else ""

    s, n = re.subn(r"(?s)<(?:polygon|circle)\b.*?/>", keep, s)
    found = set(re.findall(r'\bid="([^"]*)"', s)) & HEAD_IDS
    if found != HEAD_IDS:
        sys.exit("make_brand_pngs: the master no longer has the head's shapes "
                 "%s" % sorted(HEAD_IDS - found))
    return s


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("svg", help="the master logo SVG")
    ap.add_argument("out", help="directory to write the PNGs into")
    a = ap.parse_args()

    check_font()
    ink = inkscape_cmd()
    master = os.path.abspath(a.svg)
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    with open(master, encoding="utf-8") as f:
        text = f.read()

    with tempfile.TemporaryDirectory(prefix="tt-brand-", dir="/tmp") as tmp:
        head = os.path.join(tmp, "head.svg")
        with open(head, "w", encoding="utf-8") as f:
            f.write(head_svg(text))

        def o(name):
            return os.path.join(out, name)

        def t(name):
            return os.path.join(tmp, name)

        # The whole logo, straight from the vector at each size it is used.
        render(ink, master, t("logo640.png"), width=640)
        magick(t("logo640.png"), *CLEAN, o("tinytang-logo-640.png"))

        # Social preview and Open Graph: the logo centred on paper with a
        # margin, inside the area the networks keep when they crop.
        for name, w, h, logo in (("tinytang-social.png", 1280, 640, 512),
                                 ("tinytang-og.png", 1200, 630, 500)):
            src = t("logo%d.png" % logo)
            render(ink, master, src, height=logo)
            magick("-size", "%dx%d" % (w, h), "xc:" + PAPER, src,
                   "-gravity", "center", "-composite",
                   "-alpha", "off", *CLEAN, o(name))

        # The head, square, rendered large once and reduced: at 16 px the
        # reduction's filter decides legibility, so it is fixed here.
        render(ink, head, t("head_raw.png"), height=1024, drawing=True)
        magick(t("head_raw.png"), "-background", "none", "-gravity", "center",
               "-extent", "1024x1024", t("head.png"))

        def head_at(size, dest, bg=None):
            args = [t("head.png"), "-filter", "Lanczos", "-resize",
                    "%dx%d" % (size, size)]
            if bg:
                args += ["-background", bg, "-flatten", "-alpha", "off"]
            magick(*args, *CLEAN, dest)

        head_at(80, o("tinytang-mark-80.png"))
        for s in (16, 32, 48):
            head_at(s, o("favicon-%d.png" % s))
        magick(o("favicon-16.png"), o("favicon-32.png"), o("favicon-48.png"),
               o("favicon.ico"))
        # 140 of the 180 for the head, so the rounded mask iOS applies does
        # not clip the ears.
        magick(t("head.png"), "-filter", "Lanczos", "-resize", "140x140",
               "-background", PAPER, "-gravity", "center", "-extent", "180x180",
               "-flatten", "-alpha", "off", *CLEAN, "PNG24:" + o("apple-touch-icon.png"))

    for n in sorted(os.listdir(out)):
        print("%8d  %s" % (os.path.getsize(os.path.join(out, n)), n))


if __name__ == "__main__":
    main()
