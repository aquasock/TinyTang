# TinyReader: pictures and books

TinyReader shows pictures and books full screen at the desktop core's
own pixel resolution, 1280x720, rather than in the desktop's block
characters. It is the app for anything with text small enough to
need it: scanned pages, comics, game guides.

## The list

The window lists BMP, PPM and PGM pictures and `.trb` books under
/root/media/reader. Folders open, Up and `..` go up, Refresh rescans.
Open, a double-click or Enter shows the one selected. The Zoom (X) box
opens books in zoom mode, below.

## Reading a book

A book opens as two-page spreads, as printed: the cover alone, then
2-3, 4-5 and so on.

     Right  Down  PgDn  Space  n    next spread
     Left  Up  PgUp  p  b           previous spread
     A on the pad, left click       next spread
     the wheel                      down is next, up is previous
     Home  0                        the cover
     End                            the last spread
     Z, or X on the wired Classic   zoom mode on and off
     Esc  q  Backspace              back to the list
     B on the pad, right click      back to the list

X on a Bluetooth pad types Escape, so it leaves the book rather than
switching zoom.

Zoom mode shows each page's top and bottom halves in turn, each
filling the screen at about twice a spread's size: page 1 top, page 1
bottom, page 2 top and so on. Switching keeps your place.

TinyReader keeps the place you left a book at beside it, as
`<book>.trb.pos`, and opens it there next time.

A page turn to a spread that is already loaded is immediate. The
reader loads the spreads around the one you are reading in the
background, about two seconds each; one not yet loaded draws in from
the top. Turning pages this fast needs desktop ABI 1.11 or later, and
the window says so on an older core.

## Viewing a picture

A picture opens full screen and draws in row by row as it is read.

     +  -                     zoom in and out, 1x to 8x
     the arrows               pan
     the wheel                pan up and down
     left click, A            zoom in on that point
     right click, B           zoom out; at 1x, back to the list
     PgDn  Space  n           next picture in the folder
     PgUp  p  b               previous picture
     Home  0                  the whole picture
     Esc  q  Backspace        back to the list

## Making a book

Books are made on a computer, from a PDF, a CBZ or CBR, or a folder of
scans, with `tools/make_book.py` from the TinyTang repository:

     tools/make_book.py "Player's Guide.pdf" guide.trb
     tools/make_book.py comic.cbz comic.trb --title "Comic 1"
     tools/make_book.py scans/ book.trb --no-cover

`--no-cover` pairs pages from page 1, `--rtl` reads right to left for
manga, `--single` gives every page a screen of its own, and
`--no-zoom` leaves out the zoom views, which makes a book about a
quarter the size. The board scales nothing, so a book is large: a
161-page guide is 162 MB without zoom views and 796 MB with them.
docs/tinyreader-books.md in the repository has the details.

Copy the `.trb` into /root/media/reader with a card reader, or over USB
with `tools/tinytang_put.py`.

## Needs

TinyReader needs the desktop core's pixel plane, desktop ABI 1.8 or
later. On a core without one it says so and opens nothing.

TinyReader is newer than the last release. The v0.2.1 card's desktop
core is ABI 1.6, which has no pixel plane, so it needs the firmware and
desktop core built from the repository's main branch.
