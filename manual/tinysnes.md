# TinySNES: Super NES games

TinySNES runs Super NES cartridges from the card. The console itself
is in the FPGA, nand2mario's SNESTang, so a launch replaces the desktop
core with the SNES core and streams the cartridge into it. Its window
has five tabs: Library, Input, Video, Audio and Saves.

## Library

Browse /root/media/snes. Folders open, Up and `..` go up, Refresh
rescans. Choose a cartridge and press Launch.

Only raw `.sfc` and `.smc` images are listed. The SNES core cannot
unzip, so a `.zip` has to be extracted on a computer first. A file
without a valid SNES header is refused rather than started.

While a game is running, Launch is refused: end the game first.

## Input

Which controller button each SNES button answers to, for two players.
Player 1 beside the list switches to player 2 and back; double-click a
row, or press Enter on it, to step it to the next controller button, or
to `none`. Restore defaults puts this player's standard map back, and
Save writes /tinysnes.ini for the next game.

Player 1 is a Wii Classic Controller in a Wii Nunchuck adapter on
USB-A port 2, with the dock's two-wire mode on (in one-wire mode the
adapter is not powered and the pad reads as absent), or Bluetooth Pad 1,
or both at once. The map applies to either. Bluetooth Pad 2 is player 2,
but its map is not applied yet: it plays with the standard buttons
whatever this page says.

## Video

Two rows, each stepped with a double-click or Enter:

     Aspect   4:3, 8:7 or 1:1, the shape of a pixel
     Lines    cropped or shown

Lines are the fifteen past 224 that a few games draw. There is no
palette: SNES colour is digital. Save writes /tinysnes.ini, and both are
taken when the next game starts.

## Audio

One master volume and a mute, for HDMI and the Pmod I2S2 together.
Left and Right move the slider 5%. Save writes /tinysnes.ini.

## In a game

     Tap HOME           TinyDesk over the game, and back
     HOME with SELECT   save the game and return to the desktop

Either player 1 pad does both, and HOME and SELECT may even be on
different pads. SELECT is the Classic's `-` button.

With TinyDesk over the game, the wired Classic drives it as it does on
the desktop: the left stick is the pointer, A and B its buttons, and
the d-pad the arrow keys. This needs the TinySNES core built after
v0.2.1; v0.2.1's moves the pointer with the d-pad. There is no Stop button in the
window: a game ends from the pad, and the save is written on the way
out, which takes a second or two.

TinySNES has no save states.

## Saves

A cartridge with a battery keeps its save in /root/saves/snes, named
after the cartridge with `.sav` in place of its extension. HOME with
SELECT writes it, and the next launch puts it back before the game
starts.
The write goes through a temporary file that is read back and checked
before it replaces the old save.

The Saves page shows the cartridge selected in the Library: whether it
has a battery, when its save was last written (from the board's save
log, /root/saves/last.log), and the save's size against what the
cartridge's header asks for.

## Cartridges on the release card

/root/media/snes has two folders. `homebrew` holds eleven games by The
UnDisbeliever, all MIT licensed, as raw `.sfc`. `diagnostics` holds
Retro Regen's SNES test, extracted beside its unmodified archive. The
card's README lists them and carries their licences.
