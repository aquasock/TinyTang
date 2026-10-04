# Carried patches — QMK keyboard side

Changes this project makes to QMK live here as patch files, not as commits in
the QMK checkout itself. The checkout stays pristine and in sync with upstream,
and every local change is one reviewable file with a recorded reference commit.

A patch is applied by `scripts/apply-qmk-tang-patches.sh`, which is idempotent
(a `git checkout .` or a fresh clone reverts it, so the script runs before the
firmware is built), and a patch that neither applies nor reverse-applies is
reported rather than written into the tree.

These two target **Keychron's QMK fork**,
<https://github.com/Keychron/qmk_firmware>, branch `2025q3`, at reference commit
**`c5d998442710b38650903937249144a0cd728ebe`** ("Add Keychron K10 Max V2"), which
is the tip of that branch — the working checkout has no commits beyond upstream,
so `git -C <qmk> checkout c5d9984` reproduces the base exactly.

## 0001-keychron-k2he-tang-mode.patch

Makes `TANG_MODE` a build-time choice that turns a Keychron K2 HE into the
Tang's input device instead of a USB keyboard, across four files:

- `keyboards/keychron/k2_he/board.h` leaves the USB data pins PA11 (D-) and
  PA12 (D+) as plain GPIOs at ChibiOS board init instead of alternate-function
  OTG pins. PA12 becomes the link's transmit line.
- `tmk_core/protocol/chibios/usb_main.c` returns from `init_usb_driver()` before
  `usbStart()`/`usbConnectBus()`, so the OTG peripheral never starts and never
  reclaims those pins.
- `keyboards/keychron/common/wireless/lpm.c` returns early from `lpm_task()`
  while USB power is present, for the same reason: its stock branch sees
  `USB_STOP` — exactly what `usbStop()` leaves behind — and calls
  `init_usb_driver()`, which would bring USB back up and take the pins with it.
- `keyboards/keychron/k2_he/k2_he.c` makes `lpm_is_kb_idle()` answer `false`, so
  the wireless task never parks the CPU in `lpm_standby()`. The link runs from
  the tail of the main loop and would otherwise stop about three seconds after
  boot.

The failure mode of getting any one of these wrong is the same and quiet: a pin
owned by the OTG core ignores writes to its output register, so the link goes
silent — from the Tang, indistinguishable from a keyboard that is not plugged
in. The comments in the patch say so at each site.

## 0002-keychron-k2he-tang-keymap.patch

Adds the keymap directory `keyboards/keychron/k2_he/ansi/keymaps/tang/` —
`keymap.c`, `rules.mk`, `tang_link.c`, `tang_link.h` — as new files. The layout
is the board's stock ANSI one; what differs is that the keymap drives the link
rather than a USB keyboard interface. `tang_link.c` bit-bangs the keyboard end
of the frame `A5 LEN payload[LEN] SUM` on PA12 at **281250 baud**, which both
ends divide exactly (72 MHz / 256 on the STM32F401, 74.25 MHz / 264 on the
Tang's pixel clock). The payload is the eight-byte HID boot keyboard report QMK
already maintains.

## Build

```
make keychron/k2_he/ansi:tang
```

from the root of the `qmk-keychron` checkout, after
`scripts/apply-qmk-tang-patches.sh` has run.

**The resulting firmware is not a USB keyboard.** While it runs, the keyboard
speaks only to a Tang over PA12 and enumerates as nothing. To use the board on a
PC again, flash the stock firmware back, or hold the `BOOTMAGIC` key at plug-in
to enter the STM32's own DFU bootloader, which does not depend on this firmware.
A build without `TANG_MODE` is the ordinary QMK build and is not affected by
these patches: every hunk of 0001 is guarded by `TANG_MODE` — inside an
`#ifdef TANG_MODE`, or through a macro that expands to upstream's own pin list
when it is undefined — and 0002 only adds the keymap directory the `:tang`
target selects.

## Conventions

- One patch per coherent change, `NNNN-short-slug.patch`, numbered in the order
  they apply.
- Mark patched regions in the source with a `TinyTang:` comment, so a reader
  landing in the file sees that it is not upstream's text.
- Record the reference commit in this file.
