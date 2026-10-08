#!/usr/bin/env python3
"""Judge tb_fat_names output against make_fat_names_image.py's expectations.

    check_fat_names.py EXPECTED.tsv OUTPUT MODE

MODE `fixed`: every file is listed under its exact UTF-8 long name, can be
looked up, opened and read with the right size and contents, moves out and
back, and keeps its name, size and contents afterwards.
MODE `cp437`: the configuration the card ran with; the harness must reproduce
the card, where a name such as '08 - Múló Idő.flac' falls back to a short name
that cannot be looked up again (core-log entry 70).
SPDX-License-Identifier: MIT
"""
import sys


def main():
    expected_path, output_path, mode = sys.argv[1:4]
    expected = {}
    for line in open(expected_path, encoding='utf-8'):
        name, size, h = line.rstrip('\n').split('\t')
        expected[name] = (int(size), h)
    rows = {'LIST': [], 'MOVE': [], 'AFTER': []}
    for line in open(output_path):
        tag, *fields = line.rstrip('\n').split('\t')
        fields[0] = bytes.fromhex(fields[0])
        rows[tag].append(fields)
    failures = []

    def fail(msg):
        failures.append(msg)

    if mode == 'fixed':
        for tag in ('LIST', 'AFTER'):
            names = sorted(r[0].decode('utf-8', 'replace') for r in rows[tag])
            if names != sorted(expected):
                fail(f'{tag}: names {names} differ from {sorted(expected)}')
            for name, st, size, op, h in rows[tag]:
                text = name.decode('utf-8', 'replace')
                want = expected.get(text)
                if st != '0' or op != '0' or want is None or int(size) != want[0] or h != want[1]:
                    fail(f'{tag}: {text!r} stat {st} size {size} open {op} hash {h}, expected {want}')
        for name, out, back in rows['MOVE']:
            if out != '0' or back != '0':
                fail(f'MOVE: {name!r} out {out} back {back}')
    else:
        broken = [r for r in rows['LIST'] if r[1] != '0' or r[3] != '0']
        listed = [r[0] for r in rows['LIST']]
        if b'08 - M\xa3l\xa2 Id\x8b.flac' in listed or not broken:
            fail('the code page 437 build did not reproduce the card: every name could be looked up')
        print(f'cp437 build: {len(broken)} of {len(listed)} listed names cannot be looked up again: '
              + ', '.join(repr(r[0]) for r in broken))
    if failures:
        print('\n'.join('FAIL ' + f for f in failures))
        return 1
    print(f'{mode}: all checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
