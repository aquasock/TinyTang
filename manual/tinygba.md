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

## Input

Which GBA key each controller button presses. There is a row for each
of the controller's fourteen buttons; double-click a row, or press
Enter on it, to step it to the next GBA key, or to `none`. Several
buttons may press the same key. A star marks a map that has not been
saved. Restore defaults puts the standard map back, and Save writes
/tinygba.ini for the next game.

The map applies to the wired controller and Bluetooth Pad 1 alike; the
8BitDo has no ZL or ZR, so those two rows only move the wired
controller's. HOME and `-` with HOME work whatever the map says, so a
remapping cannot take away the way out of a game.

The map needs the TinyGBA core and the firmware built after
2026-11-22; an older core keeps the standard map.

## Video and Audio

These two pages are laid out as TinySNES's, but they change nothing
yet: the GBA core has no setting for the picture or the volume. Each
says so on its status line. The picture is the GBA's 240x160, scaled
by 4.5, between black bars.

## Controls

The controller is a Wii Classic Controller in a Wii Nunchuck adapter on
USB-A port 2. The standard map:

     d-pad, A, B, L, R      the same GBA buttons
     + / -                  START / SELECT
     X and Y                also A and B
     ZL and ZR              also L and R

Bluetooth Pad 1 plays the same way, beside the wired controller or
alone: its d-pad, A, B, L, R, START and SELECT are themselves, and its
X and Y also press A and B. Pad 2 does not reach the game, because the
GBA has one player.

## In a game

     Tap HOME          TinyDesk over the game, and back
     HOME with -       save the game and return to the desktop

Either pad does both, and HOME and `-` may even be on different pads.
With TinyDesk over the game, the wired Classic drives it as it does on
the desktop: the left stick is the pointer, A and B its buttons, and
the d-pad the arrow keys. This needs the TinyGBA core built after
2026-11-22; an earlier one moves the pointer with the d-pad.

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
