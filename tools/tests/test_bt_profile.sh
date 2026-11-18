#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# test_bt_profile.sh -- tools/bt_profile.py's HID report descriptor decoder,
# checked against descriptors recorded from real devices in docs/bt-profiles/
# (BLE-022): the report sizes it gives are the sizes the devices sent.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
python3 - "$root" <<'PY'
import json, os, sys
root = sys.argv[1]
sys.path.insert(0, os.path.join(root, "tools"))
import bt_profile

def report(name, rid):
    p = json.load(open(os.path.join(root, "docs", "bt-profiles", name)))
    parsed = bt_profile.parse_descriptor(bytes.fromhex(p["hid"][0]["descriptor_hex"]))
    return parsed, parsed["reports"][rid]

fails = 0
def check(ok, what):
    global fails
    if not ok:
        fails += 1
        print(f"test_bt_profile: FAIL {what}")

# The 8BitDo in Switch mode: the Tang received 0x3F as a1 3f + 11 bytes (BLE-021).
parsed, r = report("pro-controller-4d7f.json", "0x3f")
check(r["input_bytes"] == 11, "8BitDo report 0x3F is not 11 bytes")
check(parsed["applications"] == ["Generic Desktop / Gamepad"], "8BitDo is not a gamepad")
# The Rii K06: an 8-byte boot-compatible keyboard report, a 4-byte mouse report.
parsed, r = report("k06-ble-keyboard-3aff.json", "0x01")
check(r["input_bytes"] == 8 and r["output_bytes"] == 1, "K06 keyboard report is not 8 in, 1 out")
check(parsed["reports"]["0x03"]["input_bytes"] == 4, "K06 mouse report is not 4 bytes")
# The K950: a 7-byte key report, with no reserved byte.
_, r = report("logi-k950-da4e.json", "0x01")
check(r["input_bytes"] == 7, "K950 key report is not 7 bytes")
# The Xbox pad: one 16-byte input report and an 8-byte rumble output.
parsed, r = report("xbox-wireless-controller-4579.json", "0x01")
check(r["input_bytes"] == 16, "Xbox report 0x01 is not 16 bytes")
check(parsed["reports"]["0x03"]["output_bytes"] == 8, "Xbox rumble report is not 8 bytes")
# The M750 L: 5 buttons, 12-bit X and Y, wheel and pan in 7 bytes.
_, r = report("logi-m750-l-2b6e.json", "0x02")
check(r["input_bytes"] == 7, "M750 L report 0x02 is not 7 bytes")
check(any("rel" in f and "X, Y" in f for f in r["fields"]), "M750 L X and Y are not relative")

if fails:
    sys.exit(1)
print("test_bt_profile: descriptor decoder agrees with 5 recorded devices")
PY
