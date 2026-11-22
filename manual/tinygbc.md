# TinyGBC: Game Boy and Game Boy Color games

TinyGBC runs Game Boy and Game Boy Color cartridges from the card, on
gambatte running on the TinyGBC core's AE350. Its window has five
tabs: Library, Input, Video, Audio and Saves.

## Library

The library opens at /root/media and shows only the Game Boy folders
there: `gb`, `gbc` and `gbcolor`. Folders open, Up and `..` go up,
Refresh rescans. Choose a cartridge and press Launch.

`.gb`, `.gbc` and `.zip` are listed. A `.zip` holding one cartridge
works the same as the cartridge itself.

Launching loads the TinyGBC core, starts the game and puts the desktop
core back when it ends.

## Input

Which controller button each Game Boy button answers to. Double-click a
row, or press Enter on it, to step it to the next controller button.
Restore defaults puts the standard map back without touching the
volume, and Save writes /tinygbc.ini for the next game.

The default is A for A, B for B, `+` for START, `-` for SELECT and the
d-pad for the d-pad. HOME cannot be chosen: it belongs to TinyDesk.

The controller is a Wii Classic Controller in a Wii Nunchuck adapter on
USB-A port 2. A Bluetooth gamepad paired as Pad 1 plays beside it, and
the map above applies to it too. Over a running game, the wired Classic
drives TinyDesk with the same controls as on the desktop.

This needs the TinyGBC core and player from after v0.2.1; the v0.2.1
card's TinyGBC takes neither.

## Video

Two rows, each stepped with a double-click or Enter:

     Palette  off, auto, dmg, pocket, light or gbc
     Colour   off, accurate or fast

The palette colours a monochrome cartridge. A Color cartridge draws its
own colours, so the row is greyed for a Color cartridge once it has
run. Colour is the correction for the Color's LCD. Off is the picture
as it was before these settings existed. Save writes /tinygbc.ini,
and both are taken when the next game starts.

## Audio

One master volume and a mute, for HDMI and the Pmod I2S2 together.
Left and Right move the slider 5%. It is taken when the next game
starts, not while one is playing.

## In a game

     Tap HOME         TinyDesk over the game, and back
     HOME with L      save the game's state
     HOME with R      load that state
     HOME with -      save the battery and return to the desktop

Only HOME with `-` writes the battery save. A game left any other way
keeps the save it started with.

## Saves

Battery saves live in /root/saves/gbc, one `.sav` per cartridge. A
cartridge with a clock, such as Pokemon Gold, also keeps a `.rtc` there,
so its clock moves on while the board is off. That needs the board's
own clock to be set in Settings; without it no clock file is kept.

Each cartridge also has one save state, in /root/states/gbc. The Saves
page shows, for the cartridge selected in the Library, when its state,
its battery save and its clock were last written, whether the board's
clock is set, and the state's details below.

## Cartridges on the release card

`gb/homebrew` holds three monochrome games and `gbc/homebrew` six Color
games, all zipped, and each folder has Retro Regen's test in
`diagnostics`. The card's README lists them with their authors and
licences.
