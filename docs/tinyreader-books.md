# TinyReader books (`.trb`)

TinyReader shows a book as two-page spreads, full screen on the desktop core's
1280x720 pixel plane. The board decodes and scales nothing: a book is rendered
on the PC, once, into exactly what the plane shows, and the BL616 copies its
bytes from the card straight into the link.

## Making one

```
tools/make_book.py "Player's Guide.pdf" guide.trb
tools/make_book.py comic.cbz comic.trb --title "Comic 1"
tools/make_book.py scans/ book.trb --no-cover
```

The source is a PDF (poppler's `pdftoppm` and `pdfinfo`), a CBZ, a CBR (`7z`),
or a folder of images, sorted naturally. Pages are laid out as in the printed
book: the cover alone, then 2-3, 4-5 and so on. A page wider than it is tall
is a fold-out or two pages scanned as one, so it is shown alone, as large as
the screen allows, and pairing starts again after it. Each page fills its
half of the screen's height, scaled with a Lanczos filter and rounded to
RGB565; the file's page numbers go small and grey in the right margin when
there is room. `--no-cover` pairs from page 1, `--rtl` puts the first page of
each pair on the right, `--single` gives every page a screen of its own, and
`--no-labels` leaves the margins black.

Unless `--no-zoom` is given, every page is also rendered as two screens of
its own, its top half and its bottom half, each as large as the screen allows,
about twice a page's size in a spread. The halves overlap by `--zoom-overlap`
of the page's height (0.05 unless given) so a line cut at the middle is whole
in one of them, and a wide page is split into its left and right pages first.
Zoom views roughly quadruple the file: the 161-page Zelda guide is 162 MB
without them and 796 MB with its 344.

Put the file in `/root/media/reader` (`tools/tinytang_put.py`, about 1.3 MB/s
over the USB console, or an SD card reader). TinyReader keeps the spread a
reader left a book at in `<book>.trb.pos` beside it.

## Format, version 1

Little-endian header fields; the pixels are big-endian RGB565, as the link
sends them. `tools/make_book.py` writes it and `ports/bl616/tang_book.c` reads
it; `tools/tests/test_make_book.sh` checks the two against each other.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | magic `TRB1` |
| 4 | 2 | version, 1 |
| 6 | 2 | header bytes, 128 |
| 8 | 2 | width in pixels (1280 for the desktop core) |
| 10 | 2 | height in pixels (720) |
| 12 | 2 | pixel format: 1 = RGB565, big-endian, rows top to bottom |
| 14 | 2 | spread count, N |
| 16 | 4 | data offset: where spread 0 starts, a multiple of 4096 |
| 20 | 4 | spread bytes: width x height x 2 |
| 24 | 2 | source page count |
| 26 | 2 | flags: bit 0 cover alone, bit 1 right to left, bit 2 single pages |
| 28 | 4 | reserved, 0 |
| 32 | 64 | title, UTF-8, NUL padded |
| 96 | 4 | zoom offset: where zoom view 0 starts, a multiple of 4096; 0 for none |
| 100 | 2 | zoom view count, Z |
| 102 | 2 | zoom overlap, in thousandths of a page's height |
| 104 | 24 | reserved, 0 |
| 128 | 8 x N | spread table: left page, right page (u16 each, 1-based source pages, 0 for none), 4 reserved bytes |
| 128 + 8N | 8 x Z | zoom table, in reading order: page (u16, 1-based), part (u8: 0 whole page, 1 left half of a wide page, 2 its right half), half (u8: 0 top, 1 bottom), the spread it is in (u16), 2 reserved bytes |

Spread i starts at data offset + i x spread bytes, zoom view i at zoom offset
+ i x spread bytes. A book without zoom fields (all zero) is read as spreads
alone, so books made before zoom views still open. A 1280x720 spread is
1,843,200 bytes, 3,600 sectors, so every spread is whole, aligned sectors.

## On the board

Desktop ABI 1.11 keeps twelve 1280x720 pictures in the SDRAM. Buffer 0 is the
pictures'; a book uses the other eleven as a cache. A task loads the spreads
around the one being read in this order -- the current one, the next, the
previous, then two ahead for every one back -- in 16 KB reads, and a page turn
to a loaded spread is one switch of buffer at the next frame (link frames
0x1C and 0x1D, register 0x24). A spread takes about 2.0 s to load at the link's
10 Mbaud, 126 ms of it on the card. A spread not yet loaded is shown while it
loads, drawing in from the top. On a core before ABI 1.11 a book still opens,
each spread drawn in as it is turned to.

Zoom mode steps through a book's zoom views instead: page 1 top, page 1
bottom, page 2 top. The Zoom box in TinyReader's window chooses the mode a
book opens in, and X on the Wii pad or the Z key switches while reading,
staying on the same page; the cache holds spreads and views together, so
switching back finds the spreads still loaded. The place kept beside the book
records the mode too (`12` is spread 12, `40 z` zoom view 40).

`readerdraw <book.trb> [spread|z<view> [buffer]]` at the shell loads one
spread or zoom view and says where the time went, and `tools/plane_sweep.py
--book <book.trb> [--zoom]` checks loaded buffers against the file by the
core's frame checksum.
