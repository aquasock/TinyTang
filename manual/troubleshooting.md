# When something goes wrong

## The console does not appear

No serial device means the board is not enumerating. Check the cable
and that the board is powered; try a different USB port. After a
firmware write the board shows up as an FT2232 until it is
power-cycled.

## The picture is black

The FPGA is empty until a core is loaded. From the shell:

     fpga

If it does not answer, nothing is loaded. Load something:

     tangload /root/cores/console138k/tinynes.bin

## A core loads but the app says it is not there

Some apps need a specific core. Phosphor refuses to play without the
Phosphor core; TinyNES needs its own. Load the right one and reopen
the app.

## No sound

- The Pmod I2S2 goes in the socket beside HDMI, right side up, and
  /tinytang.ini must say layout = i2s2_oled.
- Audio to HDMI and to the Pmod come from the same stream, so if one
  works and the other does not, the socket is the suspect.

## The launcher starts the wrong thing

Launching a cartridge reloads /root/cores/console138k/tinynes.bin
every time. An image you are testing has to be installed under that
name, not just loaded once.

## Out of memory in the desktop

The desktop, the shell and the Bluetooth stack share one heap. A
script launched from the desktop can be refused; the shell usually
succeeds because nothing else is holding memory. Close a window and
try again.

## Starting over

Power-cycle. Nothing on the card is changed by a crash, and the
firmware is not altered by loading cores.
