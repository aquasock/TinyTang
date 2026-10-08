#!/usr/bin/env python3
"""Build a FAT32 image holding the Hungarian track names exactly as Linux
wrote them on the card in core-log entry 70: each file has its UTF-16 long
name and the 8.3 short name Linux chose (code page 437 bytes such as 0xA3 for
'u acute'), read back from the user's card image.  Contents are deterministic
pseudo-random bytes.

    make_fat_names_image.py IMAGE EXPECTED.tsv

EXPECTED.tsv lists each file's UTF-8 name, size and FNV-1a 32-bit hash for
tools/tests/check_fat_names.py.  SPDX-License-Identifier: MIT
"""
import hashlib
import struct
import subprocess
import sys

# Long name, the short name Linux stored for it, and a size.
FILES = [
    ('01 - Ha Újra Látom.flac', b'01-HA_~1FLA', 5100),
    ('02 - Támad A Szél.flac', b'02-T\xa0M~1FLA', 4700),
    ('03 - Elmúlt A Nyár.flac', b'03-ELM~1FLA', 6000),
    ('04 - Éjjeli Vadász.flac', b'04-\x90JJ~1FLA', 3900),
    ('05 - Híd A Folyón.flac', b'05-H\xa1D~1FLA', 4400),
    ('06 - Csillagokkal Szállsz.flac', b'06-CSI~1FLA', 5300),
    ('07 - Hol Vagy Nagy Szerelem.flac', b'07-HOL~1FLA', 4100),
    ('08 - Múló Idő.flac', b'08-M\xa3L~1FLA', 6200),
    ('09 - Álom.flac', b'09-_LO~1FLA', 3700),
    ('10 - Úgy Szeress.flac', b'10-_GY~1FLA', 4800),
    ('11 - Égj Velem.flac', b'11-\x90GJ~1FLA', 5600),
    ('12 - Stripped.flac', b'12-STR~1FLA', 4300),
]


def fnv1a(data):
    h = 0x811C9DC5
    for b in data:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def content(name, size):
    out, seed = b'', name.encode()
    while len(out) < size:
        seed = hashlib.sha256(seed).digest()
        out += seed
    return out[:size]


def checksum(sfn):
    s = 0
    for b in sfn:
        s = (((s & 1) << 7) + (s >> 1) + b) & 0xFF
    return s


def lfn_entries(name, sfn):
    units = list(name.encode('utf-16le'))
    words = [units[i] | units[i + 1] << 8 for i in range(0, len(units), 2)]
    count = -(-len(words) // 13)
    words += [0x0000] if len(words) % 13 else []
    words += [0xFFFF] * (count * 13 - len(words))
    entries, ck = [], checksum(sfn)
    for n in range(count, 0, -1):
        part = words[(n - 1) * 13: n * 13]
        e = bytearray(32)
        e[0] = n | (0x40 if n == count else 0)
        struct.pack_into('<5H', e, 1, *part[0:5])
        e[11], e[12], e[13] = 0x0F, 0, ck
        struct.pack_into('<6H', e, 14, *part[5:11])
        struct.pack_into('<2H', e, 28, *part[11:13])
        entries.append(bytes(e))
    return entries


def main():
    image, expected = sys.argv[1], sys.argv[2]
    with open(image, 'wb') as f:
        f.truncate(48 * 1024 * 1024)
    subprocess.run(['mkfs.fat', '-F', '32', '-S', '512', '-s', '1', image],
                   check=True, stdout=subprocess.DEVNULL)
    img = bytearray(open(image, 'rb').read())
    bps, spc = struct.unpack_from('<HB', img, 11)
    reserved, nfats = struct.unpack_from('<HB', img, 14)
    fatsz, root, fsinfo = struct.unpack_from('<I', img, 36)[0], struct.unpack_from('<I', img, 44)[0], \
        struct.unpack_from('<H', img, 48)[0]
    csize = bps * spc
    fat_off = reserved * bps
    data_off = fat_off + nfats * fatsz * bps
    fat = memoryview(img)[fat_off:fat_off + fatsz * bps].cast('I')
    next_free = [root + 1]

    def alloc(count):
        start = next_free[0]
        for i in range(count):
            fat[start + i] = start + i + 1 if i < count - 1 else 0x0FFFFFFF
        next_free[0] += count
        return start

    def write_chain(start, data):
        img[data_off + (start - 2) * csize: data_off + (start - 2) * csize + len(data)] = data

    # The root directory first, contiguous: its first cluster plus as many
    # more as the entries need.
    dir_len = 32 * sum(len(lfn_entries(n, s)) + 1 for n, s, _ in FILES)
    extra = -(-dir_len // csize) - 1
    if extra > 0:
        fat[root] = alloc(extra)
    entries = []
    with open(expected, 'w') as out:
        for name, sfn, size in FILES:
            data = content(name, size)
            first = alloc(-(-size // csize))
            write_chain(first, data)
            e = bytearray(32)
            e[0:11], e[11] = sfn, 0x20
            struct.pack_into('<H', e, 20, first >> 16)
            struct.pack_into('<H', e, 26, first & 0xFFFF)
            struct.pack_into('<I', e, 28, size)
            entries += lfn_entries(name, sfn) + [bytes(e)]
            out.write(f'{name}\t{size}\t{fnv1a(data):08x}\n')

    write_chain(root, b''.join(entries))
    # Copy FAT 1 to FAT 2; free count and next free are left for FatFs to work out.
    img[fat_off + fatsz * bps: fat_off + 2 * fatsz * bps] = img[fat_off: fat_off + fatsz * bps]
    struct.pack_into('<II', img, fsinfo * bps + 488, 0xFFFFFFFF, 0xFFFFFFFF)
    open(image, 'wb').write(img)


if __name__ == '__main__':
    main()
