# TinyKiwix: an encyclopedia on the card

TinyKiwix reads an offline encyclopedia: Kiwix's ZIM archives, converted on a
computer into a text pack that lives on the card. It is a reader, like
TinyHelp, and it holds no network code at all.

## Why the archive is converted first

A ZIM archive stores its articles in compressed blocks ("clusters") of about
one megabyte. Unpacking one block needs a whole megabyte of memory for the
decompressor's window, and this board has 320 KB in all. So the archive is
read where there is room for it -- on a computer -- and the card is given
plain text.

## Making a pack

You need the archive, Python 3.14 or later (no extra packages), and the card.

    tools/make_wiki_pack.py \
        --zim wikipedia_en-simple_all_nopic.zim \
        --out /run/media/vash/TINYTANG/root/wiki \
        --main-page "Main Page"

The tool writes three things into that folder: `wiki.meta` (title, language,
licence and counts), `wiki.idx` (the sorted title index) and `wiki.000` and up
(the article text). A 1 GB archive takes about ten minutes and leaves roughly
550 MB on the card. The 49 GB English archive needs no different command: it
writes several `wiki.NNN` files because FAT32 will not hold a single file
over 4 GB, and it takes some hours.

What the conversion keeps is what this screen can show: headings, paragraphs
and list items, wrapped to 78 columns, with accents and dashes folded to
ASCII. Images, tables and non-Latin scripts are left out, and an article
longer than the pack allows ends with a note saying so. A long article is read
one page at a time -- about four screens of text -- and the footer says which
page you are on. Redirects such as
"USA" are kept and point at the article they name, without storing its text
twice.

## Using the window

TinyKiwix opens on the pack's main page. The list is a window onto the
alphabetical index, not the whole of it:

    Find...   type the first letters of a title
    Page Up   the page before: of titles, or of the article
    Page Down the page after
    Enter     open the title the cursor is on, or the next page
    Back      return to the list of titles

A search reads the index directly, so it costs the same with a thousand
articles as with three hundred thousand.

## The licence

Wikipedia's text is CC BY-SA 4.0, and a pack is a derived work, so it keeps
those terms. The licence line in `wiki.meta` names them, and the pack's title
and count are shown in the window. If you pass a pack on, pass the licence
with it.

## If it says there is no pack

The window reads `/root/wiki`. If the folder is missing or holds no
`wiki.meta` with a matching `wiki.idx`, it says so rather than showing an
empty list. Put a pack there, or make one with the command above.
