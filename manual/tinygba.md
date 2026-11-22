# TinyGBA: Game Boy Advance games

TinyGBA runs Game Boy Advance cartridges from the card. The console is
in the FPGA, nand2mario's GBATang, so a launch replaces the desktop
core with the GBA core and streams the cartridge into it. Its window
has the same five tabs as TinySNES's: Library, Input, Video, Audio and
Saves.

TinyGBA is newer than the last release. Its window is in firmware
built from the repository's main branch, and its core is not on a
release card yet: it goes on the card from the TinyGBA repository, at
/root/cores/console138k/tinygba.bin.

## Library

Browse /root/media/gba. Folders open, Up and `..` go up, Refresh
rescans. Choose a cartridge and press Launch.

Only raw `.gba` images are listed; a `.zip` has to be extracted on a
computer first. A file that is not a GBA cartridge is refused. The
loader finds the cartridge's save type itself while it streams the
image.

## Input, Video and Audio

These three pages are laid out as TinySNES's, but they change nothing
yet: the GBA core has no setting for a pad map, the picture or the
volume. Each says so on its status line. The picture is the GBA's
240x160, scaled by 4.5, between black bars.

## Controls

The controller is a Wii Classic Controller in a Wii Nunchuck adapter on
USB-A port 2:

     d-pad, A, B, L, R      the same GBA buttons
     + / -                  START / SELECT
     X and Y                also A and B
     ZL and ZR              also L and R

The core also takes Bluetooth Pad 1 and Pad 2 from the firmware, but
no GBA game has been played with a Bluetooth pad yet.

## In a game

     Tap HOME          TinyDesk over the game, and back
     HOME with -       save the game and return to the desktop

The exit takes a second or two, because the save is written to the
card before the desktop core is loaded again. TinyGBA has no save
states.

## Saves

A GBA save sits beside its cartridge, named after the whole file:
`game.gba` saves to `game.gba.sav`. A launch puts it back before the
game starts. A cartridge with no `.sav` starts with an empty save, as a
new cartridge would. The `.sav` files are the same format mGBA uses, in
both directions.

The Saves page shows the cartridge selected in the Library: its save
type and the size that type holds, whether a `.sav` is there, and when
it was last written, from the board's save log. A `.sav` whose size
differs from what the cartridge holds is flagged there, because it
will not load.

A cartridge started with `gbaload` at the shell is not watched for
HOME with `-`, so nothing saves it on the way out; `gbasave` at the
shell writes its `.sav` by hand.

## The card

/root/media/gba/diagnostics holds Retro Regen's GBA test, extracted
beside its unmodified archive. `homebrew` is empty.
