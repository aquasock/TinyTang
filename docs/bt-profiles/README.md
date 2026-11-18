# Bluetooth device profiles

What a Linux PC's BlueZ and kernel saw when each of the operator's Bluetooth
input and audio devices was paired with it on 2026-11-18, recorded by
`tools/bt_profile.py` so the Tang's Classic HID host (BLE-021, BLE-022) is
designed from evidence. Each JSON file holds BlueZ's device properties, the
kernel HID device (driver and raw report descriptor, plus the descriptor
decoded into report IDs, sizes and fields) and the input events seen while
the operator pressed each control.

The PC paired with a NoInputNoOutput agent, the IO capability the Tang has,
and every device paired without a passkey. BlueZ's `LegacyPairing` property
is not a reliable indicator: the same Wiimote reported `no` once and `yes`
twice. Whether the Wii devices used a legacy PIN is open; BlueZ's wiimote
plugin answers the PIN itself, so only a `btmon` capture (root) settles it.

| File | Device | Link | Vendor:product | Linux driver | Input format | Before input |
|---|---|---|---|---|---|---|
| `pro-controller-4d7f.json` | 8BitDo pad, Switch mode | Classic | 057E:2009 | nintendo | report 0x3F, 11 bytes: 16 buttons, 4-bit hat, four 16-bit sticks | nothing (Linux switches it to the 0x30 full mode with motion) |
| `nintendo-rvl-cnt-01-uc-5c91.json` | Wii U Pro Controller | Classic | 057E:0330 | wiimote | Wii Remote protocol: vendor reports 0x10-0x1A out, 0x20-0x22 and 0x30-0x31 in | report mode (0x12) and extension initialisation |
| `nintendo-rvl-cnt-01-tr-66d1.json` | Wiimote with Motion Plus | Classic | 057E:0330 | wiimote | Wii Remote protocol, 0x30-0x37 data reports | report mode; extensions read through the Wiimote's registers |
| `nintendo-rvl-cnt-01-tr-66d1-nunchuk.json` | the same, nunchuck attached | Classic | 057E:0330 | wiimote | as above; the nunchuck is an extension (C, Z, stick, accelerometer) | as above |
| `nintendo-rvl-cnt-01-tr-66d1-classic.json` | the same, Classic Controller attached | Classic | 057E:0330 | wiimote | as above; the Classic Controller is an extension (12 buttons, two sticks, analog shoulders) | as above |
| `xbox-wireless-controller-4579.json` | Xbox Wireless Controller (Series), firmware 0x0509 | LE (HID over GATT) | 045E:0B13 | microsoft | report 0x01, 16 bytes: four 16-bit sticks, two 10-bit triggers, hat 1-8, 15 buttons, Share; 0x03 rumble out | nothing |
| `logi-m750-l-2b6e.json` | Logitech M750 L mouse | LE | 046D:B02C | hid-generic | report 0x02, 7 bytes: 5 buttons, 12-bit X and Y, wheel, pan; 0x11 is HID++ | nothing; not the boot-mouse layout |
| `logi-k950-da4e.json` | Logitech K950 keyboard | LE | 046D:B386 | hid-vivaldi | report 0x01, 7 bytes: modifiers and six keys with no reserved byte; 0x05 consumer keys; 0x11 HID++ | nothing; not the boot-keyboard layout |
| `k06-ble-keyboard-3aff.json` | Rii K06 keyboard and touchpad | LE | 04E8:7021 | samsung | report 0x01, 8-byte boot keyboard layout; 0x03, 4 bytes: 3 buttons, 8-bit X and Y, wheel | nothing |
| `8bitdo-cube-2-5fc5.json` | 8BitDo Cube 2 speaker | Classic | - | (audio) | A2DP sink with SBC and SBC-XQ only; AVRCP; Hands-Free | - |

Re-run a profile with:

    tools/bt_profile.py pair <addr|name>
    tools/bt_profile.py profile <addr> 30 --out docs/bt-profiles
    tools/bt_profile.py remove <addr>
