#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Regenerate the Phosphor format corpus: one 10-second 440 Hz stereo tone at
# 44.1 kHz in each format the resident AE350 player decodes.
#
# This is Tang-Phosphor's entry 43 corpus, which that project regenerated with
# ffmpeg by hand and recommended making deterministic.  Ten files are encoded
# here, with the encoder settings of Tang-Phosphor's tools/rbhost_profile.py;
# test.wma and test.opus are the two originals that survived from the first
# corpus and are copied in rather than re-encoded, so pass their directory.
#
#   tools/make_codec_corpus.sh <out-dir> <dir-with-test.wma-and-test.opus>
#
# ffmpeg's bitexact flags drop the encoder-version tags, so one ffmpeg version
# always produces the same bytes; a different version may not, which is why the
# script prints each file's size and CRC-32 for the record.
set -euo pipefail

out="${1:?usage: make_codec_corpus.sh <out-dir> <originals-dir>}"
originals="${2:?usage: make_codec_corpus.sh <out-dir> <originals-dir>}"
mkdir -p "$out"

src="$out/.tone.wav"
ffmpeg -nostdin -loglevel error -y -fflags +bitexact \
    -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=10" \
    -ac 2 -c:a pcm_s16le -flags:a +bitexact -map_metadata -1 "$src"

# The bitexact flags go on the output: given before -i they apply to the input
# only, and the Ogg muxer then picks a random stream serial on every run.
enc() {   # enc <file> <codec args...>
    local name="$1"; shift
    ffmpeg -nostdin -loglevel error -y -i "$src" \
        -map_metadata -1 -fflags +bitexact -flags:a +bitexact "$@" "$out/$name"
}

enc test.wav  -c:a pcm_s16le
enc test.flac -c:a flac
enc test.m4a  -c:a alac
enc test.wv   -c:a wavpack
enc test.tta  -c:a tta
enc test.mp3  -c:a libmp3lame -b:a 320k
enc test.mp2  -c:a mp2 -b:a 256k
enc test.ogg  -c:a libvorbis -q:a 6
enc test.mp4  -c:a aac -b:a 256k
enc test.ac3  -c:a ac3 -b:a 448k
cp "$originals/test.wma" "$originals/test.opus" "$out/"
rm -f "$src"

for f in wav flac mp2 mp3 ogg mp4 m4a wv ac3 tta wma opus; do
    python3 -c 'import sys,zlib,os; p=sys.argv[1]; d=open(p,"rb").read(); print("%-10s %8d bytes  CRC-32 %08x" % (os.path.basename(p), len(d), zlib.crc32(d) & 0xffffffff))' "$out/test.$f"
done
