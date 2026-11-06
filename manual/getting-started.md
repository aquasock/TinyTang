# Getting started

Power the Tang Console over USB-C. The board starts in **TinyDesk**; the shell
is one keystroke away, and is where the board stops if you ask it to.

## The two halves

The board is two things working together:

- the BL616, a small microcontroller that runs the console, the
  desktop, Bluetooth and the card
- the FPGA, which is empty until something loads a core into it

The desktop runs on the BL616, so it stays up when a core is
loaded and replaced. That is why you can press F12 mid-game.

## First boot

1. Put a FAT32 card in the slot with the board powered off.
2. Power up. It loads the desktop core from the card and comes up in
   TinyDesk, on HDMI. Exit from the start menu returns to a shell prompt,
   and `desktop` brings the desktop back.
3. Pairing a keyboard and mouse is done from the desktop's Bluetooth window
   (or with `blekbd pair` and `blemouse pair` from a shell).

If you want the board to stop at the shell prompt instead -- while bringing a
card up, or before a keyboard is paired -- put an empty file at
`/root/scripts/no-desktop`. Remove it and the desktop comes back; nothing is
reflashed either way.

## The shell

The shell is the ground floor. A few commands worth knowing:

     platform        firmware identity
     tangini         what /tinytang.ini declares
     fpga            which core is loaded and answering
     tangload <bin>  load a core from the card
     help            everything else

Everything the desktop does, the shell can do too. The desktop is a
convenience, not a requirement.
