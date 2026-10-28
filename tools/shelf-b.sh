#!/bin/bash
# Class b (philosophy and religion) -> a TinyGutenberg shelf, installed on the card if it is mounted.
#
# Same shape as the Wikipedia run: find the newest archive, verify its MD5,
# convert it with the book settings, check every record.  See the class list in
# manual/gutenberg.md for the other letters.
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CARD=${CARD:-/run/media/vash/TINYTANG} exec "$here/gutenberg_shelf.sh" b "$@"
