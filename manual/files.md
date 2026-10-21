# Files, firmware and the card

## The card

Everything lives on one FAT32 card:

     /root/media/music     tracks for the player
     /root/media/nes       cartridges
     /root/cores/          FPGA cores
     /root/saves/nes       battery saves
     /root/manual          this manual
     /root/scripts         shell scripts
     /tang.ini             which PMODs are seated

## Putting files on the card

Over USB, with the console at a shell prompt:

     tools/tinytang_put.py <local file> /root/media/music/track.mp3

The board must be at the shell. If the desktop is running, press F12
first: bytes sent then are typed into whichever window has focus.

## PMODs

A core drives a socket only if /tang.ini says what is seated there,
because PMOD modules carry no identification. A missing file leaves
both sockets released, which is the safe state.

## Firmware

     tools/tinytang_flash.py build/build_out/tinytang_bl616.bin

Then power-cycle. The board reports its new identity with `platform`.

Never run `make flash`: it writes from address 0 and would overwrite
the board's own loader.
