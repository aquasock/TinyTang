#!/usr/bin/env python3
"""Generate OLED Terminal graphics demos: pre-rendered 24x16 ANSI frames.

The OLED Terminal (desktop ABI 1.1) is 24x16 cells of 4x4 pixels, each cell a
glyph in one of 16 palette colours over another.  A cell can therefore show
more than 16 tones: a density glyph such as ':' or '#' in colour B over
colour A mixes the two in proportion to its lit pixels.  These demos build a
shade ramp from such mixes and render effects through it, one ANSI file per
frame, which `cat` writes into the OLED session:

    tools/make_oled_demo.py build/oled-demo
    # then copy build/oled-demo/<demo>/fN.ans, restore.ans and the players to
    # /demos/ and run one from the OLED Terminal, or from the USB console with
    # `oledterm run "tdsh run /demos/fire10.tdsh"`
    # boot-fire.tdsh is scripts/boot.tdsh plus a line that starts fire10 at
    # power-up; copy it to /scripts/boot.tdsh for a plug-in-and-film board.

A board script gets a 16 KB stack: `cat` alone uses about 9.6 KB, each loop or
`if` around it about 1.4 KB more and a function call about 5 KB, so players
stay within three nested blocks and use no functions.  Playback measured
about 17 frames a second.

Output is identical on every run.  SPDX-License-Identifier: MIT
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from make_oled_font import GLYPHS  # noqa: E402

COLS, ROWS, FRAMES = 24, 16, 48

# fpga/desktop/desktop_oled.sv palette(), RGB565.
PALETTE565 = [0x0000, 0xc800, 0x0660, 0xce60, 0x001d, 0xc819, 0x0679, 0xe73c,
              0x7bef, 0xf800, 0x07e0, 0xffe0, 0x5aff, 0xf81f, 0x07ff, 0xffff]


def rgb(c):
    v = PALETTE565[c]
    return ((v >> 11) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31)


def lit(ch):
    return bin(int(GLYPHS[ch], 16)).count('1') / 16


# Glyphs used as mixing patterns, ordered by coverage.
MIXERS = [' ', '.', ':', '+', '#']


def ramp(path):
    """Cells (glyph, fg, bg) stepping through palette colours in `path`,
    with every mixer between each neighbouring pair, darkest first."""
    cells = []
    for a, b in zip(path, path[1:]):
        steps = sorted({(lit(g), g) for g in MIXERS})
        for cover, g in steps:
            cells.append((cover, (g, b, a)))       # B dots over A
        for cover, g in reversed(steps[1:-1]):
            cells.append((1 - cover, (g, a, b)))   # A dots over B
    out = [cells[0][1]]
    for _, cell in cells[1:]:
        if cell != out[-1]:
            out.append(cell)
    out.append((' ', path[-1], path[-1]))
    return out


def sgr(fg, bg):
    f = 30 + fg if fg < 8 else 90 + fg - 8
    b = 40 + bg if bg < 8 else 100 + bg - 8
    return f'\x1b[{f};{b}m'


def frame(cells):
    """One frame: cursor hidden, every row positioned, colours only on change."""
    out, last = ['\x1b[?25l'], None
    for y in range(ROWS):
        out.append(f'\x1b[{y + 1};1H')
        for x in range(COLS):
            g, fg, bg = cells[y][x]
            if (fg, bg) != last:
                out.append(sgr(fg, bg))
                last = (fg, bg)
            out.append(g)
    return ''.join(out)


def plasma(t, shades):
    n = len(shades) - 1
    rows = []
    for y in range(ROWS):
        row = []
        for x in range(COLS):
            u, v = x / COLS * 2 * math.pi, y / ROWS * 2 * math.pi
            p = t * 2 * math.pi / FRAMES
            s = (math.sin(u * 1.3 + p) + math.sin(v * 1.7 - p * 2)
                 + math.sin((u + v) * 0.9 + p * 3)
                 + math.sin(math.hypot(u - math.pi, v - math.pi) * 1.4 - p * 2)) / 4
            row.append(shades[min(n, int((s + 1) / 2 * n + 0.5))])
        rows.append(row)
    return rows


def fire_frames(shades):
    """Classic fire: a hot bottom row, heat rising and cooling, wind drift.
    A fixed linear congruential generator keeps it identical on every run."""
    seed = 12345

    def rand():
        nonlocal seed
        seed = (seed * 1103515245 + 12345) & 0x7fffffff
        return seed >> 16

    n = len(shades) - 1
    cool = max(2, n * 5 // (ROWS * 2))            # fades out near the top
    heat = [[0] * COLS for _ in range(ROWS + 1)]
    frames = []
    for t in range(FRAMES + 32):
        for x in range(COLS):
            heat[ROWS][x] = n if rand() % 8 else n * 3 // 4
        for y in range(ROWS):
            for x in range(COLS):
                src = heat[y + 1][(x + rand() % 3 - 1) % COLS]
                heat[y][x] = max(0, src - rand() % cool)
        if t >= 32:                                # let it reach the top first
            frames.append([[shades[heat[y][x]] for x in range(COLS)] for y in range(ROWS)])
    return frames


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else 'build/oled-demo')
    demos = {
        # black, blue, magenta, red, bright red, yellow, bright yellow, white
        'plasma': [plasma(t, ramp([0, 4, 12, 13, 9, 11, 15])) for t in range(FRAMES)],
        'fire': fire_frames(ramp([0, 1, 9, 3, 11, 15])),
    }
    sizes = []
    for name, frames in demos.items():
        d = out / name
        d.mkdir(parents=True, exist_ok=True)
        for i, cells in enumerate(frames):
            data = frame(cells).encode('ascii')
            assert b'\0' not in data and b'\n' not in data   # `cat` uses fgets
            (d / f'f{i}.ans').write_bytes(data)
            sizes.append(len(data))
    # Default colours, a cleared screen and the cursor back for the shell.
    (out / 'restore.ans').write_bytes(b'\x1b[0m\x1b[2J\x1b[H\x1b[?25h')
    (out / 'play.tdsh').write_text(PLAY)
    (out / 'fire.tdsh').write_text(fire_player('# Fire only (tools/make_oled_demo.py frames).', 5))
    (out / 'fire10.tdsh').write_text(fire_player(
        '# Fire for about 10 minutes (48 frames at about 17 fps, 2.8 s a loop).', 215))
    boot = (Path(__file__).resolve().parent.parent / 'scripts/boot.tdsh').read_text()
    (out / 'boot-fire.tdsh').write_text(boot + BOOT_FIRE)
    print(f'{len(sizes)} frames in {out}, {min(sizes)}-{max(sizes)} bytes each')


# Plays each demo LOOPS times, about 17 frames a second (the OLED link sends
# the screen at most every 40 ms), then restores the screen and cursor.
PLAY = f'''# OLED Terminal graphics demos (tools/make_oled_demo.py).
LOOPS=3
for DEMO in plasma fire
    L=0
    while $L < $LOOPS
        I=0
        while $I < {FRAMES}
            cat /demos/$DEMO/f$I.ans
            sleep 0.04
            I=$((I + 1))
        endwhile
        L=$((L + 1))
    endwhile
endfor
cat /demos/restore.ans
'''



def fire_player(title, loops):
    return f'''{title}
L=0
while $L < {loops}
    I=0
    while $I < {FRAMES}
        cat /demos/fire/f$I.ans
        sleep 0.04
        I=$((I + 1))
    endwhile
    L=$((L + 1))
endwhile
cat /demos/restore.ans
'''


BOOT_FIRE = '''
# Filming demo: play the fire on the OLED for about 10 minutes.
# Restore with: cp /scripts/diagnostics/boot-before-fire.tdsh /scripts/boot.tdsh
oledterm run "tdsh run /demos/fire10.tdsh"
'''

if __name__ == '__main__':
    main()
