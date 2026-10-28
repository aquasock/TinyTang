#!/bin/bash
# Class e (United States history) -> a TinyGutenberg shelf.
# 3,743 books in 10.2 GB of archive, about 2.5 GB as a pack.
#
# Same four stages as the Wikipedia run: newest archive, MD5 check, convert
# with the book settings, verify the pack.  Installs into /root/books on the
# card when the card is mounted; otherwise the pack waits in the scratch folder.
# Any other Library of Congress letter works with tools/gutenberg_shelf.sh <class>.
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CARD=${CARD:-/run/media/vash/TINYTANG} exec "$here/gutenberg_shelf.sh" e "$@"
