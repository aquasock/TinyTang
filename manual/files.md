# Files, firmware and the card

## The card

Everything lives on one FAT32 card:

     /root/media/music     tracks for the player
     /root/media/nes       cartridges
     /root/cores/          FPGA cores
     /root/saves/nes       battery saves
     /root/manual          this manual
     /root/scripts         shell scripts
     /tinytang.ini             which PMODs are seated

## Putting files on the card

Over USB, with the console at a shell prompt:

     tools/tinytang_put.py <local file> /root/media/music/track.mp3

The board must be at the shell. If the desktop is running, press F12
first: bytes sent then are typed into whichever window has focus.

## Syncing a folder

`tools/tinytang_sync.py` is the two-way version of that. It finds the board by
its USB identity, prints the card's tree, asks which folder to work on, and
compares it with a mirror kept beside the script in `tang/`. It shows what is
only on the board, only on the computer, or a different size, and asks which
way to move the files. Nothing is deleted, and files of the same size are
treated as equal because the card has no real timestamps.

     pip install pyserial
     python tools/tinytang_sync.py

## PMODs

A core drives a socket only if /tinytang.ini says what is seated there,
because PMOD modules carry no identification. It names one layout:

     layout = i2s2_oled    I2S2 beside HDMI, OLED beside USB-A
     layout = vga          the PmodVGA across both sockets

Every module is seated right side up. A missing file leaves both
sockets released, which is the safe state.

## Firmware

     tools/tinytang_flash.py build/build_out/tinytang_bl616.bin

Then power-cycle. The board reports its new identity with `platform`.

Never run `make flash`: it writes from address 0 and would overwrite
the board's own loader.
