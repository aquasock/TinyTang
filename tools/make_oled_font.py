#!/usr/bin/env python3
"""Generate the OLED's three font sets -- TinyTang's original 4x4 and 6x8 glyphs
and the desktop's 8x8 font -- and native-size specimens."""
# SPDX-License-Identifier: MIT
from pathlib import Path

# Four hexadecimal rows, most significant bit at the left. Lowercase uses
# shorter forms where four rows permit them; this is a tiny display font,
# not a reduction of the desktop's 8x8 glyphs. DEL is the fallback box.
GLYPHS = {
    ' ': '0000', '!': '4404', '"': 'aa00', '#': 'afaf', '$': '6c36',
    '%': '9249', '&': '6cad', "'": '4400', '(': '2442', ')': '4224',
    '*': '0a4a', '+': '04e4', ',': '0048', '-': '00e0', '.': '0004',
    '/': '1248', '0': '6bd6', '1': '464e', '2': 'e168', '3': 'e262',
    '4': 'aaf2', '5': 'ec2c', '6': '68e6', '7': 'f124', '8': '6a6a',
    '9': '6ae2', ':': '0404', ';': '0408', '<': '2484', '=': '0e0e',
    '>': '8424', '?': 'e204', '@': '6bd6',
    'A': '69f9', 'B': 'eaea', 'C': '7887', 'D': 'e99e', 'E': 'f8cf',
    'F': 'f8c8', 'G': '78b7', 'H': '9f99', 'I': 'e44e', 'J': '322c',
    'K': '9aca', 'L': '888f', 'M': '9ff9', 'N': '9db9', 'O': '6996',
    'P': 'eae8', 'Q': '6997', 'R': 'eaca', 'S': '7c3e', 'T': 'f444',
    'U': '9996', 'V': 'aaa4', 'W': '999f', 'X': '9669', 'Y': '9644',
    'Z': 'f24f', '[': '6446', '\\': '8421', ']': '6226', '^': '4a00',
    '_': '000f', '`': '4200',
    'a': '06be', 'b': '8caa', 'c': '0787', 'd': '26aa', 'e': '06f7',
    'f': '64e4', 'g': '06e2', 'h': '8eaa', 'i': '4044', 'j': '202c',
    'k': '8acb', 'l': '4442', 'm': '0ffe', 'n': '0eaa', 'o': '0696',
    'p': '0ea8', 'q': '06a2', 'r': '06c8', 's': '07c3', 't': '4e46',
    'u': '0aa6', 'v': '0aa4', 'w': '0aaf', 'x': '0a4a', 'y': '0aa2',
    'z': '0e4e', '{': '64c6', '|': '4444', '}': 'c46c', '~': '05a0',
    chr(127): 'f99f',
}


# The 6x8 Compact set, also original: five drawn columns and a blank sixth
# for spacing, rows 0-6 for the body and row 7 for descenders, so lines of
# text keep one blank row between them.  Rows are listed top to bottom; a
# glyph lists only as many rows as it needs, and the rest are blank.  DEL is
# the fallback box, as in the 4x4 set.
COMPACT = {
    ' ': [],
    '!': ['..#..', '..#..', '..#..', '..#..', '..#..', '.....', '..#..'],
    '"': ['.#.#.', '.#.#.'],
    '#': ['.#.#.', '.#.#.', '#####', '.#.#.', '#####', '.#.#.', '.#.#.'],
    '$': ['..#..', '.####', '#.#..', '.###.', '..#.#', '####.', '..#..'],
    '%': ['##...', '##..#', '...#.', '..#..', '.#...', '#..##', '...##'],
    '&': ['.##..', '#..#.', '#.#..', '.#...', '#.#.#', '#..#.', '.##.#'],
    "'": ['..#..', '..#..', '.#...'],
    '(': ['...#.', '..#..', '.#...', '.#...', '.#...', '..#..', '...#.'],
    ')': ['.#...', '..#..', '...#.', '...#.', '...#.', '..#..', '.#...'],
    '*': ['.....', '..#..', '#.#.#', '.###.', '#.#.#', '..#..'],
    '+': ['.....', '..#..', '..#..', '#####', '..#..', '..#..'],
    ',': ['.....', '.....', '.....', '.....', '.....', '.##..', '..#..', '.#...'],
    '-': ['.....', '.....', '.....', '#####'],
    '.': ['.....', '.....', '.....', '.....', '.....', '.##..', '.##..'],
    '/': ['.....', '....#', '...#.', '..#..', '.#...', '#....'],
    '0': ['.###.', '#...#', '#..##', '#.#.#', '##..#', '#...#', '.###.'],
    '1': ['..#..', '.##..', '..#..', '..#..', '..#..', '..#..', '.###.'],
    '2': ['.###.', '#...#', '....#', '...#.', '..#..', '.#...', '#####'],
    '3': ['#####', '...#.', '..#..', '...#.', '....#', '#...#', '.###.'],
    '4': ['...#.', '..##.', '.#.#.', '#..#.', '#####', '...#.', '...#.'],
    '5': ['#####', '#....', '####.', '....#', '....#', '#...#', '.###.'],
    '6': ['..##.', '.#...', '#....', '####.', '#...#', '#...#', '.###.'],
    '7': ['#####', '....#', '...#.', '..#..', '.#...', '.#...', '.#...'],
    '8': ['.###.', '#...#', '#...#', '.###.', '#...#', '#...#', '.###.'],
    '9': ['.###.', '#...#', '#...#', '.####', '....#', '...#.', '.##..'],
    ':': ['.....', '.##..', '.##..', '.....', '.##..', '.##..'],
    ';': ['.....', '.##..', '.##..', '.....', '.##..', '..#..', '.#...'],
    '<': ['...#.', '..#..', '.#...', '#....', '.#...', '..#..', '...#.'],
    '=': ['.....', '.....', '#####', '.....', '#####'],
    '>': ['.#...', '..#..', '...#.', '....#', '...#.', '..#..', '.#...'],
    '?': ['.###.', '#...#', '....#', '...#.', '..#..', '.....', '..#..'],
    '@': ['.###.', '#...#', '....#', '.##.#', '#.#.#', '#.#.#', '.###.'],
    'A': ['.###.', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'],
    'B': ['####.', '#...#', '#...#', '####.', '#...#', '#...#', '####.'],
    'C': ['.###.', '#...#', '#....', '#....', '#....', '#...#', '.###.'],
    'D': ['###..', '#..#.', '#...#', '#...#', '#...#', '#..#.', '###..'],
    'E': ['#####', '#....', '#....', '####.', '#....', '#....', '#####'],
    'F': ['#####', '#....', '#....', '####.', '#....', '#....', '#....'],
    'G': ['.###.', '#...#', '#....', '#.###', '#...#', '#...#', '.####'],
    'H': ['#...#', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'],
    'I': ['.###.', '..#..', '..#..', '..#..', '..#..', '..#..', '.###.'],
    'J': ['..###', '...#.', '...#.', '...#.', '...#.', '#..#.', '.##..'],
    'K': ['#...#', '#..#.', '#.#..', '##...', '#.#..', '#..#.', '#...#'],
    'L': ['#....', '#....', '#....', '#....', '#....', '#....', '#####'],
    'M': ['#...#', '##.##', '#.#.#', '#.#.#', '#...#', '#...#', '#...#'],
    'N': ['#...#', '#...#', '##..#', '#.#.#', '#..##', '#...#', '#...#'],
    'O': ['.###.', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'],
    'P': ['####.', '#...#', '#...#', '####.', '#....', '#....', '#....'],
    'Q': ['.###.', '#...#', '#...#', '#...#', '#.#.#', '#..#.', '.##.#'],
    'R': ['####.', '#...#', '#...#', '####.', '#.#..', '#..#.', '#...#'],
    'S': ['.####', '#....', '#....', '.###.', '....#', '....#', '####.'],
    'T': ['#####', '..#..', '..#..', '..#..', '..#..', '..#..', '..#..'],
    'U': ['#...#', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'],
    'V': ['#...#', '#...#', '#...#', '#...#', '#...#', '.#.#.', '..#..'],
    'W': ['#...#', '#...#', '#...#', '#.#.#', '#.#.#', '#.#.#', '.#.#.'],
    'X': ['#...#', '#...#', '.#.#.', '..#..', '.#.#.', '#...#', '#...#'],
    'Y': ['#...#', '#...#', '.#.#.', '..#..', '..#..', '..#..', '..#..'],
    'Z': ['#####', '....#', '...#.', '..#..', '.#...', '#....', '#####'],
    '[': ['.###.', '.#...', '.#...', '.#...', '.#...', '.#...', '.###.'],
    '\\': ['.....', '#....', '.#...', '..#..', '...#.', '....#'],
    ']': ['.###.', '...#.', '...#.', '...#.', '...#.', '...#.', '.###.'],
    '^': ['..#..', '.#.#.', '#...#'],
    '_': ['.....', '.....', '.....', '.....', '.....', '.....', '.....', '#####'],
    '`': ['.#...', '..#..'],
    'a': ['.....', '.....', '.###.', '....#', '.####', '#...#', '.####'],
    'b': ['#....', '#....', '#.##.', '##..#', '#...#', '#...#', '####.'],
    'c': ['.....', '.....', '.###.', '#....', '#....', '#...#', '.###.'],
    'd': ['....#', '....#', '.##.#', '#..##', '#...#', '#...#', '.####'],
    'e': ['.....', '.....', '.###.', '#...#', '#####', '#....', '.###.'],
    'f': ['..##.', '.#..#', '.#...', '###..', '.#...', '.#...', '.#...'],
    'g': ['.....', '.....', '.####', '#...#', '#...#', '.####', '....#', '.###.'],
    'h': ['#....', '#....', '#.##.', '##..#', '#...#', '#...#', '#...#'],
    'i': ['..#..', '.....', '.##..', '..#..', '..#..', '..#..', '.###.'],
    'j': ['...#.', '.....', '..##.', '...#.', '...#.', '...#.', '#..#.', '.##..'],
    'k': ['#....', '#....', '#..#.', '#.#..', '##...', '#.#..', '#..#.'],
    'l': ['.##..', '..#..', '..#..', '..#..', '..#..', '..#..', '.###.'],
    'm': ['.....', '.....', '##.#.', '#.#.#', '#.#.#', '#...#', '#...#'],
    'n': ['.....', '.....', '#.##.', '##..#', '#...#', '#...#', '#...#'],
    'o': ['.....', '.....', '.###.', '#...#', '#...#', '#...#', '.###.'],
    'p': ['.....', '.....', '####.', '#...#', '#...#', '####.', '#....', '#....'],
    'q': ['.....', '.....', '.####', '#...#', '#...#', '.####', '....#', '....#'],
    'r': ['.....', '.....', '#.##.', '##..#', '#....', '#....', '#....'],
    's': ['.....', '.....', '.###.', '#....', '.###.', '....#', '####.'],
    't': ['.#...', '.#...', '###..', '.#...', '.#...', '.#..#', '..##.'],
    'u': ['.....', '.....', '#...#', '#...#', '#...#', '#..##', '.##.#'],
    'v': ['.....', '.....', '#...#', '#...#', '#...#', '.#.#.', '..#..'],
    'w': ['.....', '.....', '#...#', '#...#', '#.#.#', '#.#.#', '.#.#.'],
    'x': ['.....', '.....', '#...#', '.#.#.', '..#..', '.#.#.', '#...#'],
    'y': ['.....', '.....', '#...#', '#...#', '#...#', '.####', '....#', '.###.'],
    'z': ['.....', '.....', '#####', '...#.', '..#..', '.#...', '#####'],
    '{': ['...#.', '..#..', '..#..', '.#...', '..#..', '..#..', '...#.'],
    '|': ['..#..', '..#..', '..#..', '..#..', '..#..', '..#..', '..#..'],
    '}': ['.#...', '..#..', '..#..', '...#.', '..#..', '..#..', '.#...'],
    '~': ['.....', '.....', '.#...', '#.#.#', '...#.'],
    chr(127): ['#####', '#...#', '#...#', '#...#', '#...#', '#...#', '#####'],
}


def compact_rows(ch):
    """The glyph as eight 6-bit rows, most significant bit at the left."""
    art = COMPACT[ch]
    assert len(art) <= 8 and all(len(r) == 5 and set(r) <= {'.', '#'} for r in art), ch
    art = art + ['.....'] * (8 - len(art))
    return [int(r.replace('#', '1').replace('.', '0') + '0', 2) for r in art]


def standard_font(root):
    """The desktop's 8x8 font from font.vh: 128 glyphs of eight row bytes,
    least significant bit at the left, as textdisp_wide reads it."""
    import re
    text = (root / 'fpga/desktop/font.vh').read_text()
    rows = [[int(b, 16) for b in re.findall(r"8'h([0-9A-Fa-f]{2})", line)]
            for line in text.splitlines() if "8'h" in line]
    assert len(rows) == 128 and all(len(r) == 8 for r in rows)
    return rows


def main():
    root = Path(__file__).resolve().parent.parent
    assert set(GLYPHS) == {chr(c) for c in range(32, 128)}
    lines = ['// SPDX-License-Identifier: GPL-3.0-only',
             '// Generated by tools/make_oled_font.py; original TinyTang glyphs.',
             'function automatic [15:0] oled_glyph(input [7:0] ch);',
             '    case (ch)']
    for ch, rows in sorted(GLYPHS.items()):
        assert len(rows) == 4
        lines.append(f"        8'h{ord(ch):02x}: oled_glyph = 16'h{rows};")
    lines += ["        default: oled_glyph = 16'hf99f;", '    endcase', 'endfunction']
    (root / 'fpga/desktop/oled_font.vh').write_text('\n'.join(lines) + '\n')
    # Native 96x64 proof sheet, also used by the deterministic panel probe.
    specimen = ['OLED TERMINAL 24X16     ', 'ABCDEFGHIJKLMNOPQRSTUVWX',
                'YZ 0123456789 +-*/=      ', 'abcdefghijklmnopqrstuvwx',
                'yz !?.,:;()[]{}<>_       ', 'root@tinytang:~# ls      ',
                'echo hello OLED         ', 'The quick brown fox     ',
                'jumps over 0123456789    ', 'WHITE RED GREEN BLUE    ',
                'input edit wrap scroll  ', 'unsupported: ' + chr(127),
                '123456789012345678901234', 'abcdefghijklmnopqrstuvwx',
                'ABCDEFGHIJKLMNOPQRSTUVWX', '4x4 font readability    ']
    data = bytearray()
    for y in range(64):
        row = specimen[y // 4].ljust(24)
        for x in range(96):
            bits = int(GLYPHS[row[x // 4]], 16)
            data.append(255 if bits & (1 << (15 - (y % 4)*4 - x % 4)) else 0)
    out = root / 'build/oled-terminal'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'font-specimen.pgm').write_bytes(b'P5\n96 64\n255\n' + data)
    (out / 'font-specimen.txt').write_text('\n'.join(s.ljust(24) for s in specimen) + '\n')

    assert set(COMPACT) == {chr(c) for c in range(32, 128)}
    lines = ['// SPDX-License-Identifier: GPL-3.0-only',
             '// Generated by tools/make_oled_font.py; original TinyTang glyphs.',
             '// Eight 6-bit rows, row 0 in the top bits, column 0 the left.',
             'function automatic [47:0] oled_glyph_compact(input [7:0] ch);',
             '    case (ch)']
    for ch in sorted(COMPACT):
        value = 0
        for r in compact_rows(ch):
            value = (value << 6) | r
        lines.append(f"        8'h{ord(ch):02x}: oled_glyph_compact = 48'h{value:012x};")
    box = 0
    for r in compact_rows(chr(127)):
        box = (box << 6) | r
    lines += [f"        default: oled_glyph_compact = 48'h{box:012x};", '    endcase', 'endfunction']
    (root / 'fpga/desktop/oled_font_compact.vh').write_text('\n'.join(lines) + '\n')

    # The Standard set is the desktop's own 8x8 font, converted from font.vh
    # into the same case-function form as the other two sets (which every
    # simulator reads) with the rows turned most significant bit left, and
    # DEL, which font.vh leaves blank, drawn as the fallback box.
    standard = standard_font(root)
    box8 = [0xff, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xff]
    def msb_left(b):
        return int(f'{b:08b}'[::-1], 2)
    lines = ['// SPDX-License-Identifier: GPL-3.0-only',
             '// Generated by tools/make_oled_font.py from fpga/desktop/font.vh, the',
             "// desktop's 8x8 font; DEL is TinyTang's fallback box.",
             '// Eight 8-bit rows, row 0 in the top bits, column 0 the left.',
             'function automatic [63:0] oled_glyph_standard(input [7:0] ch);',
             '    case (ch)']
    for code in range(32, 128):
        rows = box8 if code == 127 else [msb_left(b) for b in standard[code]]
        value = int(''.join(f'{r:02x}' for r in rows), 16)
        lines.append(f"        8'h{code:02x}: oled_glyph_standard = 64'h{value:016x};")
    lines += [f"        default: oled_glyph_standard = 64'h{int(''.join(f'{r:02x}' for r in box8), 16):016x};",
              '    endcase', 'endfunction']
    (root / 'fpga/desktop/oled_font_standard.vh').write_text('\n'.join(lines) + '\n')

    # Specimens of the Standard (8x8, 12x8) and Compact (6x8, 16x8) modes.
    for name, cols, width, text in (
            ('standard', 12, 8, ['OLED 12x8   ', 'root@tinyta ', 'ng:~# ls    ',
                                 'ABCDEFGHIJKL', 'abcdefghijkl', '0123456789!?',
                                 '()[]{}<>+-*/', 'unknown: ' + chr(127) + '  ']),
            ('compact', 16, 6, ['OLED 16x8       ', 'root@tinytang:~#',
                                'ABCDEFGHIJKLMNOP', 'QRSTUVWXYZ 01234',
                                'abcdefghijklmnop', 'qrstuvwxyz 56789',
                                '!"#$%&()*+,-./:;', '<=>?@[]^_`{|}~' + chr(127) + ' '])):
        data = bytearray()
        for y in range(64):
            row = text[y // 8].ljust(cols)
            for x in range(96):
                c = x // width
                ch = row[c] if c < cols else ' '
                gx, gy = x % width, y % 8
                if width == 8:
                    bits = (box8 if ord(ch) == 127 else standard[ord(ch)])[gy]
                    lit = bits >> gx & 1
                else:
                    lit = compact_rows(ch)[gy] >> (5 - gx) & 1
                data.append(255 if lit else 0)
        (out / f'font-specimen-{name}.pgm').write_bytes(b'P5\n96 64\n255\n' + data)
    print('Generated oled_font.vh, oled_font_compact.vh, oled_font_standard.vh and native 96x64 specimens')


if __name__ == '__main__':
    main()
