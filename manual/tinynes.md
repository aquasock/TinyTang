# TinyNES: games

TinyNES runs NES cartridges from the card. Its window has five tabs:
Library, Input, Video, Audio and Saves.

## Library

Browse /root/media/nes. Folders open, `..` goes up, Refresh rescans.
Choose a cartridge and press Launch.

A `.zip` holding one `.nes` works the same as a `.nes`.

Launching reloads the core and starts the game. The window survives
that, because it lives on the BL616; F12 brings it back over the game.

## Input

Which controller button each NES button answers to, for two players.
Player beside the list picks the player; double-click a row, or press
Enter on it, to step it to the next controller button. Restore defaults
puts the standard map back, and Save writes /tinynes.ini for the next
game.

Player one is the Wii Classic Controller on USB-A port 2 and player two
the one on PMOD0. A Bluetooth gamepad paired as Pad 1 or Pad 2 plays as
player 1 or 2 beside them.

## Video

The pixel aspect and the palette. Both are taken when the next game
starts.

## Audio

One master volume and a mute, for HDMI and the Pmod I2S2 together.

## In a game

     Tap HOME         TinyDesk over the game, and back
     HOME with L      save the game's state
     HOME with R      load that state
     HOME with -      end the game and return to the desktop

While TinyDesk is up over a game the wired Classic drives the desktop,
as on the desktop core.

## Saves

Battery saves live in /root/saves/nes and are written when the game
ends. Each cartridge also has one save state, in /root/states/nes,
taken with HOME and L and loaded with HOME and R. The Saves tab shows
when the state and the battery save were last written, with the
state's details below.

## Sound

Games are audible on the Pmod I2S2 and on HDMI, from the same
samples, at the I2S unit's own frame rate.

## Cartridges on the release card

A release card carries eight cartridges rather than nothing, because a
new board with no cartridge on it cannot be shown to work.  They sit
in two folders under /root/media/nes: `homebrew`, with six games, and
`diagnostics`, with two test suites.  All are redistributable and none
is a commercial title: supply your own cartridges for those.

- **AccuracyCoin.zip** - Chris Siebert, MIT. In `diagnostics`: a suite
  of NES accuracy tests in one cartridge, useful for showing what this
  emulator gets right and where a game might not behave.
- **RetroRegen-NES-Test-latest.zip** - Retro Regen, freeware. Also in
  `diagnostics`: a hardware test suite that checks a real console's
  logic, memory, video, sound and controllers and reports a pass or a
  reason code. Its licence allows redistribution only as the complete
  unmodified archive, so that is what is here; the archive carries its
  own licence and README. The same publisher's Game Boy, Game Boy
  Color and SNES suites are in those libraries' `diagnostics` folders.
- **Nova the Squirrel.zip** - NovaSquirrel, GPL-3.0. A full platformer
  and the largest of the set.
- **FamiDash.zip** - TFDSoft, MIT. A Geometry Dash clone with its own
  level tracks.
- **Coco Battle Royale II.zip** - psywave-games, GPL-3.0. A Japanese
  homebrew action game.
- **RoboRun.zip** - heaven-hm, GPL-3.0. A small platformer, in C.
- **Table Tennis.zip** - Michael Billington, MIT. A two-player paddle
  game.
- **Snake.zip** - Sebastian Dine, Zlib. The classic, on one screen.

The six games are in `homebrew` and the two test suites in
`diagnostics`.  Every one is shipped zipped, and this player inflates
a `.zip` and CRC-checks it itself, so dropping a zipped cartridge into
/root/media/nes, in a folder of your own or none, is the normal way to
add one.

The MIT and Zlib notices travel with these in the card's README, and
the three GPL-3.0 titles carry a full licence text in the card's
LICENSES folder, with their source at the repositories named in that
README. Copying a cartridge onto a card is redistribution, so the
notices matter: if you pass a card to somebody, pass those too.
