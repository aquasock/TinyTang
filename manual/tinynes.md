# TinyNES: games

TinyNES runs NES cartridges from the card. Its window has four tabs.

## Library

Browse /root/media/nes. Folders open, `..` goes up, Refresh rescans.
Choose a cartridge and press Launch.

A `.zip` holding one `.nes` works the same as a `.nes`.

Launching reloads the core and starts the game. The window survives
that, because it lives on the BL616; F12 brings it back over the game.

## Input

Which controller button each NES button answers to, for two players.
Change steps the selected row; Save writes /nes.ini for the next game.

## Video

The pixel aspect and the palette. Both are taken when the next game
starts.

## Saves

Battery saves live in /root/saves/nes and are written when the game
ends. Save states are not implemented.

## Sound

Games are audible on the Pmod I2S2 and on HDMI, from the same
samples, at the I2S unit's own frame rate.
