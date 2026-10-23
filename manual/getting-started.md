# Getting started

Power the Tang Console over USB-C. The board always starts in the
shell; the desktop is a program you launch from it.

## The two halves

The board is two things working together:

- the BL616, a small microcontroller that runs the console, the
  desktop, Bluetooth and the card
- the FPGA, which is empty until something loads a core into it

The desktop runs on the BL616, so it stays up when a core is
loaded and replaced. That is why you can press F12 mid-game.

## First boot

1. Put a FAT32 card in the slot with the board powered off.
2. Power up. The console appears as a serial port on your computer.
3. At the shell prompt, type:

     platform

   It answers with the firmware build it is running.
4. Type `osd desk on` to start the desktop, or press F12.

## The shell

The shell is the ground floor. A few commands worth knowing:

     platform        firmware identity
     tangini         what /tinytang.ini declares
     fpga            which core is loaded and answering
     tangload <bin>  load a core from the card
     help            everything else

Everything the desktop does, the shell can do too. The desktop is a
convenience, not a requirement.
