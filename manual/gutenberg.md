# TinyGutenberg: books on the card

TinyGutenberg reads Project Gutenberg books from a shelf on the card. It is a
separate app from TinyKiwix because a book is not an article: you read one for
hours, so it needs a contents list, a place to keep your page, and a way to
find an author.

## Making a shelf

Books come from Kiwix's Gutenberg archives, which are cut by Library of
Congress class, so one archive is one shelf:

    tools/make_wiki_pack.py \
        --zim gutenberg_en_lcc-p_2026-03.zim \
        --out /run/media/vash/TINYTANG/root/wiki/gutenberg-p \
        --min-chars 2000 --max-article-bytes 2097152

Two of those settings differ from an encyclopedia. The article cap allows two
megabytes of text, because a novel is 300 to 700 KB and the default of 256 KB
would cut it mid-chapter. The size floor drops the archive's own navigation:
alongside each book a Gutenberg archive carries an author page, a bookshelf
page and a home page, each a paragraph or so of links.

The whole English library is 206 GB, because every book arrives with its scans
and audio. A single class is 13 MB to 2.5 GB, which is what makes shelves the
practical unit. The class letters are the Library of Congress classes: P is
literature, Q science, T technology, Z bibliography.

## What the shelf carries

Two files beyond the usual pack: `wiki.parts`, the chapters -- a book's record
number, the byte offset of the chapter and its title -- and `wiki.auth`, one
author per book ordered by surname. Both are written by the converter and
checked by `tools/check_wiki_pack.py`.

## Using the window

    Shelves    pick a shelf, as TinyKiwix picks a pack
    Titles     the shelf's books in title order
    Authors    the same books by surname; Enter lists that author's books
    Find...    type the first letters of a title, or of a surname
    Contents   the open book's chapters; Enter jumps to one
    Page Up    the page before, or the previous window of titles
    Page Down  the page after
    Back       out of a book, out of a shelf

Your place is kept in `/root/gutenberg.pos` -- the shelf, the book and the
page -- written when you leave a book and when the window closes. Reopening
the app opens that book at that page.

## What is not there

Pictures, because the display has none. Project Gutenberg's own header and
footer are stripped, so a book begins at its first word. Poetry keeps its line
breaks but not the layout a printed page had.
