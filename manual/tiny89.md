# Tiny89: the TI-89 Titanium

Tiny89 is a TI-89 Titanium calculator: TiEmu's emulator running on
the Tiny89 core's AE350, with the calculator's screen filling most of
the HDMI picture. Its window has two tabs: Library and Saves.

## Your own operating system

The calculator needs an operating system image, and TinyTang does not
ship one: it is Texas Instruments' software. Put your own in
/root/media/ti89, for example /root/media/ti89/TI89Titanium_OS.89u.
`.89u`, `.89t`, `.raw` and `.rom` files are listed. The release card
has the folder, empty.

## Library

Browse /root/media/ti89. Folders open, Up and `..` go up, Refresh
rescans. Choose an image and press Launch. The calculator's screen
replaces the desktop; TinyDesk can still be brought over it with F12.

## Keys

The calculator is driven by the Bluetooth keyboard. Letters, digits,
the arrows, Enter, Escape and Backspace are themselves, and the keys a
PC has spare carry the calculator's own:

     Tab              2nd
     F1 F2 F3 F4      diamond, ALPHA, MODE, CATALOG
     Shift+F1..F8     the calculator's F1 to F8
     F6 F7 F8         SIN, COS, TAN
     F9               LN
     Space            space
     Caps Lock        HAND
     Insert           APPS
     Home             HOME
     Page Up          STO
     Delete           CLEAR
     End              ON
     Page Down        EE
     [ ]              ( )
     '                |
     Shift+=          plus
     Shift+8          multiply
     Shift+6          the power key ^
     Shift+-          the negative sign

The keypad's / * - + and Enter work as themselves. Control is the
program's, not the calculator's:

     Ctrl+S           save the calculator to the card
     Ctrl+Alt+Q       end the session and return to the desktop

The calculator turns itself off after a few idle minutes. The picture
stops and no key seems to work; End is the ON key.

## Saves

Ctrl+S saves. The board reads the file back and checks it before it
replaces the old one, so a save that fails leaves the previous one
alone, and pressing Ctrl+S again costs only the time. Saves are kept
per image: the state in /root/states/ti89, and the calculator's RAM
and flash in /root/saves/ti89. A launch resumes from the state; with
no state, the calculator boots.

The Saves page shows, for the image selected in the Library, whether
each of the three files is on the card and what the board's save log
said about its last save. Refresh reads them again. If it says a save
did not verify, press Ctrl+S again in the session.

The 4 MiB flash save is a known problem in v0.2.1: it can stall with
no progress, and the session then never returns.
