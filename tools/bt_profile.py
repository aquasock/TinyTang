#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Profile a Bluetooth input device on this PC, before the Tang has to.

The Tang's Classic HID host (BLE-021) has to know, per device, how it pairs,
what its reports look like and whether it needs a command before it reports.
BlueZ on a Linux PC learns all of that while pairing, so a device is paired
here first and what BlueZ and the kernel saw is recorded:

    tools/bt_profile.py scan [secs]          # what is in pairing mode nearby
    tools/bt_profile.py pair <addr|name>     # pair, trust and connect
    tools/bt_profile.py profile <addr> [secs] [--out DIR]
    tools/bt_profile.py remove <addr>        # unpair, so it can pair elsewhere

`pair` registers its agent as NoInputNoOutput -- the IO capability the Tang
has (BLE-021) -- so a device that pairs here without a PIN pairs with the
Tang the same way.  A legacy PIN request is answered with 0000 and reported:
type 0000 and Enter on a keyboard that asks.

`profile` records BlueZ's view (name, class, modalias with the USB-style
vendor and product, legacy pairing, service UUIDs), the kernel's HID device
for that address (the driver that took it and the report descriptor, decoded
into report IDs, sizes and usages) and its input nodes, then for `secs`
seconds prints every key, button and axis event they produce, so presses can
be matched to controls.  It needs no root: report descriptors are readable in
sysfs and /dev/input is readable by the `input` group.  The raw HID reports
themselves (/dev/hidraw*) are root-only and are not read.
"""

import argparse
import glob
import json
import os
import re
import select
import struct
import subprocess
import sys
import time

ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|\x01|\x02|\r")
ADDR = re.compile(r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")


def btctl(*args, scan_secs=None):
    """One bluetoothctl command, its output without escapes.  Only a scan
    takes --timeout: given it, bluetoothctl waits it out even after a command
    that is already done, which made one lookup per nearby device slow."""
    cmd = ["bluetoothctl"] + (["--timeout", str(scan_secs)] if scan_secs else []) + list(args)
    r = subprocess.run(cmd, capture_output=True, text=True,
                       timeout=(scan_secs or 0) + 15)
    return ANSI.sub("", r.stdout + r.stderr)


def info(addr):
    """bluetoothctl info as a dict; repeated keys (UUID) become lists."""
    out = {}
    for line in btctl("info", addr).splitlines():
        m = re.match(r"^\s+([A-Za-z]+):\s*(.*)$", line)
        if not m:
            continue
        k, v = m.group(1), m.group(2).strip()
        if k == "UUID":
            out.setdefault("UUID", []).append(v)
        else:
            out[k] = v
    return out


# ------------------------------------------------------------------ scan

def cmd_scan(args):
    before = set(re.findall(r"Device (\S+)", btctl("devices")))
    print(f"scanning {args.secs} s (Classic and LE)...")
    btctl("scan", "on", scan_secs=args.secs)
    found = re.findall(r"Device (\S+) (.*)", btctl("devices"))
    rows = []
    for addr, name in found:
        if addr in before:
            continue                      # only what appeared in this scan
        i = info(addr)
        cls = i.get("Class", "")
        rows.append((addr, name, cls, i.get("Icon", ""), i.get("RSSI", ""),
                     "Classic" if cls else "LE", addr not in before))
    print(f"{'address':17}  {'kind':7}  {'class':10}  {'icon':16}  rssi  name")
    for addr, name, cls, icon, rssi, kind, new in sorted(rows, key=lambda r: (not r[6], r[1])):
        rssi_v = re.search(r"-?\d+", rssi)
        print(f"{addr}  {kind:7}  {cls[:10]:10}  {icon[:16]:16}  "
              f"{rssi_v.group(0) if rssi_v else '':>4}  {name}{'' if new else '   (known before)'}")


# ------------------------------------------------------------------ pair

class Session:
    """An interactive bluetoothctl, read line by line."""

    def __init__(self):
        self.p = subprocess.Popen(["bluetoothctl"], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.buf = b""
        self.log = []

    def send(self, line):
        self.p.stdin.write((line + "\n").encode())
        self.p.stdin.flush()

    def lines(self, secs):
        end = time.time() + secs
        while time.time() < end:
            r, _, _ = select.select([self.p.stdout], [], [], 0.2)
            if r:
                chunk = os.read(self.p.stdout.fileno(), 4096)
                if not chunk:
                    return
                self.buf += chunk
                while b"\n" in self.buf or b"]# " in self.buf:
                    if b"\n" in self.buf:
                        raw, self.buf = self.buf.split(b"\n", 1)
                    else:
                        raw, self.buf = self.buf, b""
                    text = ANSI.sub("", raw.decode(errors="replace")).strip()
                    if text:
                        self.log.append(text)
                        yield text

    def wait(self, pattern, secs, on_line=None):
        rx = re.compile(pattern)
        for line in self.lines(secs):
            if on_line:
                on_line(line)
            if rx.search(line):
                return line
        return None

    def close(self):
        try:
            self.send("quit")
            self.p.wait(timeout=5)
        except Exception:
            self.p.kill()


def cmd_pair(args):
    addr = args.addr.upper()
    by_name = not ADDR.match(args.addr)
    s = Session()
    notes = {"pin_requested": False, "passkey": None}

    def agent(line):
        if re.search(r"Enter PIN code", line):
            notes["pin_requested"] = True
            print("pair: the device asked for a legacy PIN; answering 0000 "
                  "(on a keyboard: type 0000 and Enter)")
            s.send("0000")
        elif re.search(r"Confirm passkey|Request confirmation", line):
            s.send("yes")
        elif re.search(r"Passkey: *\d+", line):
            notes["passkey"] = re.search(r"\d{6}", line).group(0)
            print(f"pair: type {notes['passkey']} then Enter on the device")
        elif re.search(r"Authorize service", line):
            s.send("yes")
        if re.search(r"Failed|Error|error|NEW\]|CHG\].*(Paired|Bonded|Connected|ServicesResolved)|"
                     r"Pairing successful|Connection successful|agent", line):
            print("  " + line)

    s.wait(r"Agent registered|# ", 5)
    s.send("agent off")
    s.wait(r"Agent unregistered|# ", 3)
    s.send("agent NoInputNoOutput")
    s.wait(r"Agent registered", 5)
    s.send("default-agent")
    s.wait(r"Default agent", 5)
    if by_name:
        # A name to match: the first device that appears with it, or one
        # already known by it.  For pads that sleep soon after their sync
        # button, where a scan followed by a pair is too slow.
        rx = re.compile(args.addr, re.I)
        known = [a for a, n in re.findall(r"Device (\S+) (.*)", btctl("devices")) if rx.search(n)]
        print(f"pair: looking for a device named /{args.addr}/ (it must be in pairing mode)...")
        s.send("scan on")
        hit = None
        for line in s.lines(30):
            m = re.search(r"(?:NEW|CHG)\] Device (\S+) (?:Name: )?(.*)", line)
            if m and rx.search(m.group(2)):
                hit = m.group(1).upper()
                break
            m = re.search(r"CHG\] Device (\S+) RSSI", line)
            if m and m.group(1).upper() in known:
                hit = m.group(1).upper()
                break
        s.send("scan off")
        if not hit:
            print("pair: nothing by that name in 30 s")
            s.close()
            return 1
        addr = hit
        print(f"pair: found {addr}")
    elif addr not in btctl("devices").upper():
        print(f"pair: looking for {addr} (it must be in pairing mode)...")
        s.send("scan on")
        if not s.wait(re.escape(addr), 30, agent):
            s.send("scan off")
            print("pair: not found in 30 s")
            s.close()
            return 1
        s.send("scan off")
    s.send(f"pair {addr}")
    done = s.wait(r"Pairing successful|Failed to pair|AlreadyExists", 60, agent)
    if not done or "Failed" in done:
        print(f"pair: {done or 'no answer in 60 s'}")
        s.close()
        return 1
    s.send(f"trust {addr}")
    s.wait(r"trust succeeded|Changing .* trust", 5, agent)
    s.send(f"connect {addr}")
    s.wait(r"Connection successful|Failed to connect|ServicesResolved: yes", 30, agent)
    s.wait(r"$^", 3, agent)          # let the input nodes appear
    s.close()
    i = info(addr)
    print(f"pair: {i.get('Name', '?')}: paired {i.get('Paired')}, bonded {i.get('Bonded')}, "
          f"connected {i.get('Connected')}, legacy pairing {i.get('LegacyPairing')}"
          f"{', PIN asked' if notes['pin_requested'] else ''}")
    return 0


def cmd_remove(args):
    print(btctl("disconnect", args.addr).strip().splitlines()[-1:] or "")
    print(btctl("remove", args.addr).strip().splitlines()[-1:] or "")


# ------------------------------------------------------------- HID parsing

USAGE_PAGES = {0x01: "Generic Desktop", 0x02: "Simulation", 0x05: "Game", 0x06: "Generic Device",
               0x07: "Keyboard", 0x08: "LED", 0x09: "Button", 0x0C: "Consumer",
               0x0D: "Digitizer", 0x0F: "PID (force feedback)", 0x14: "Alphanumeric Display",
               0x84: "Power", 0x85: "Battery"}
GD_USAGES = {0x01: "Pointer", 0x02: "Mouse", 0x04: "Joystick", 0x05: "Gamepad", 0x06: "Keyboard",
             0x07: "Keypad", 0x08: "Multi-axis", 0x30: "X", 0x31: "Y", 0x32: "Z", 0x33: "Rx",
             0x34: "Ry", 0x35: "Rz", 0x36: "Slider", 0x37: "Dial", 0x38: "Wheel",
             0x39: "Hat switch", 0x80: "System Control", 0x90: "D-pad up", 0x91: "D-pad down",
             0x92: "D-pad right", 0x93: "D-pad left"}


def usage_name(page, u):
    if page == 0x01:
        return GD_USAGES.get(u, f"0x{u:02x}")
    if page == 0x09:
        return f"Button {u}"
    if page >= 0xFF00:
        return f"vendor 0x{u:02x}"
    return f"0x{u:02x}"


def parse_descriptor(data):
    """Report IDs with their input/output/feature bit sizes, the top-level
    application collections, and per report the fields (usage page, usages,
    count x size, flags)."""
    gstack, g = [], {"page": 0, "id": 0, "size": 0, "count": 0, "lmin": 0, "lmax": 0}
    local_usages, local_min, local_max = [], None, None
    depth, apps, reports = 0, [], {}
    i = 0
    while i < len(data):
        b = data[i]
        if b == 0xFE:                                 # long item
            i += 3 + data[i + 1]
            continue
        size = (0, 1, 2, 4)[b & 3]
        typ, tag = (b >> 2) & 3, b >> 4
        raw = data[i + 1:i + 1 + size]
        val = int.from_bytes(raw, "little") if raw else 0
        sval = int.from_bytes(raw, "little", signed=True) if raw else 0
        i += 1 + size
        if typ == 0:                                  # main
            if tag in (8, 9, 0xB):                    # input, output, feature
                kind = {8: "input", 9: "output", 0xB: "feature"}[tag]
                r = reports.setdefault(g["id"], {"input": 0, "output": 0, "feature": 0, "fields": []})
                r[kind] += g["size"] * g["count"]
                if local_min is not None and local_max is not None:
                    us = f"{usage_name(g['page'], local_min)}..{usage_name(g['page'], local_max)}"
                else:
                    us = ", ".join(usage_name(g["page"], u) for u in local_usages[:8]) or "-"
                    if len(local_usages) > 8:
                        us += f", +{len(local_usages) - 8}"
                flags = "const" if val & 1 else ("var" if val & 2 else "array")
                if not (val & 1) and val & 4:
                    flags += ",rel"
                r["fields"].append(f"{kind} {USAGE_PAGES.get(g['page'], hex(g['page']))}: {us} "
                                   f"[{g['count']} x {g['size']} bits, {flags}, "
                                   f"{g['lmin']}..{g['lmax']}]")
            elif tag == 0xA:                          # collection
                if depth == 0 and (local_usages or local_min is not None):
                    u = local_usages[0] if local_usages else local_min
                    apps.append(f"{USAGE_PAGES.get(g['page'], hex(g['page']))} / {usage_name(g['page'], u)}")
                depth += 1
            elif tag == 0xC:                          # end collection
                depth -= 1
            local_usages, local_min, local_max = [], None, None
        elif typ == 1:                                # global
            if tag == 0:
                g["page"] = val
            elif tag == 1:
                g["lmin"] = sval
            elif tag == 2:
                g["lmax"] = sval if sval >= g["lmin"] else val
            elif tag == 7:
                g["size"] = val
            elif tag == 8:
                g["id"] = val
            elif tag == 9:
                g["count"] = val
            elif tag == 0xA:
                gstack.append(dict(g))
            elif tag == 0xB and gstack:
                g = gstack.pop()
        elif typ == 2:                                # local
            if tag == 0:
                local_usages.append(val & 0xFFFF)
            elif tag == 1:
                local_min = val & 0xFFFF
            elif tag == 2:
                local_max = val & 0xFFFF
    out = {"applications": apps, "reports": {}}
    for rid, r in sorted(reports.items()):
        out["reports"][f"0x{rid:02x}"] = {
            "input_bytes": (r["input"] + 7) // 8, "output_bytes": (r["output"] + 7) // 8,
            "feature_bytes": (r["feature"] + 7) // 8, "fields": r["fields"]}
    return out


# ----------------------------------------------------------- the kernel side

def hid_devices(addr):
    """The kernel's HID devices whose HID_UNIQ is this address."""
    want = addr.lower()
    found = []
    for d in sorted(glob.glob("/sys/bus/hid/devices/*")):
        try:
            ue = open(os.path.join(d, "uevent")).read()
        except OSError:
            continue
        if f"hid_uniq={want}" not in ue.lower():
            continue
        drv = os.path.basename(os.path.realpath(os.path.join(d, "driver"))) \
            if os.path.exists(os.path.join(d, "driver")) else None
        try:
            desc = open(os.path.join(d, "report_descriptor"), "rb").read()
        except OSError:
            desc = b""
        name = re.search(r"HID_NAME=(.*)", ue)
        events, motion = [], []
        for p in sorted(glob.glob(os.path.join(d, "input", "input*", "event*"))):
            try:
                node_name = open(os.path.join(os.path.dirname(p), "name")).read().strip()
                props = int(open(os.path.join(os.path.dirname(p), "properties")).read().split()[-1], 16)
            except (OSError, ValueError, IndexError):
                node_name, props = "", 0
            # INPUT_PROP_ACCELEROMETER (0x06): a motion sensor node, which
            # reports continuously and would drown the controls in the watch.
            if props & (1 << 6) or "IMU" in node_name or "Motion" in node_name:
                motion.append(os.path.basename(p))
            else:
                events.append(os.path.basename(p))
        found.append({"sysfs": os.path.basename(d), "name": name.group(1) if name else "",
                      "driver": drv, "descriptor_hex": desc.hex(),
                      "descriptor_len": len(desc), "events": events, "motion_events": motion})
    return found


EV_NAMES = {1: "KEY", 2: "REL", 3: "ABS"}


def code_names():
    """Linux input code names from the kernel header, if present."""
    names = {1: {}, 2: {}, 3: {}}
    for path in ("/usr/include/linux/input-event-codes.h",):
        try:
            for m in re.finditer(r"#define\s+((KEY|BTN|REL|ABS)_\w+)\s+(0x[0-9a-fA-F]+|\d+)",
                                 open(path).read()):
                kind = {"KEY": 1, "BTN": 1, "REL": 2, "ABS": 3}[m.group(2)]
                names[kind].setdefault(int(m.group(3), 0), m.group(1))
        except OSError:
            pass
    return names


def watch(events, secs):
    """Print key, relative and absolute events from the input nodes."""
    names = code_names()
    fds = {}
    for ev in events:
        try:
            fds[os.open(f"/dev/input/{ev}", os.O_RDONLY | os.O_NONBLOCK)] = ev
        except OSError as e:
            print(f"watch: /dev/input/{ev}: {e}")
    if not fds:
        return []
    fmt = "llHHi"
    size = struct.calcsize(fmt)
    seen, last_abs = [], {}
    end = time.time() + secs
    print(f"watch: {secs} s on {', '.join(fds.values())}; press each control in turn")
    while time.time() < end:
        r, _, _ = select.select(list(fds), [], [], 0.2)
        for fd in r:
            try:
                data = os.read(fd, size * 64)
            except BlockingIOError:
                continue
            for off in range(0, len(data) - size + 1, size):
                _, _, typ, code, value = struct.unpack_from(fmt, data, off)
                if typ not in EV_NAMES:
                    continue
                name = names[typ].get(code, f"{EV_NAMES[typ]}_{code}")
                if typ == 3:
                    prev = last_abs.get((fd, code))
                    last_abs[(fd, code)] = value
                    # Axes: a line when one moves well, not for every jitter.
                    if prev is not None and abs(value - prev) < 64 and value not in (-1, 0, 1):
                        continue
                if typ == 1 and value == 2:
                    continue                          # autorepeat
                line = f"{fds[fd]} {name} {value}"
                print("  " + line)
                seen.append(line)
    for fd in fds:
        os.close(fd)
    return seen


def cmd_profile(args):
    addr = args.addr.upper()
    i = info(addr)
    if not i:
        print(f"profile: BlueZ does not know {addr}")
        return 1
    prof = {"address": addr, "when": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "bluez": i}
    mod = re.search(r"v([0-9A-Fa-f]{4})p([0-9A-Fa-f]{4})d([0-9A-Fa-f]{4})", i.get("Modalias", ""))
    print(f"name      {i.get('Name')}  (alias {i.get('Alias')})")
    print(f"kind      {'Classic' if i.get('Class') else 'LE'}  class {i.get('Class', '-')}  "
          f"icon {i.get('Icon', '-')}  address type {i.get('AddressType', '-')}")
    print(f"ids       {('vendor 0x' + mod.group(1) + ' product 0x' + mod.group(2) + ' version 0x' + mod.group(3)) if mod else i.get('Modalias', '-')}")
    print(f"pairing   paired {i.get('Paired')}  bonded {i.get('Bonded')}  "
          f"legacy pairing {i.get('LegacyPairing')}  connected {i.get('Connected')}")
    for u in i.get("UUID", []):
        print(f"service   {u}")
    hids = hid_devices(addr)
    # A driver such as hid-nintendo talks to the device before it registers,
    # so its HID device can appear seconds after the connection.
    for _ in range(20):
        if hids and all(h["events"] for h in hids):
            break
        time.sleep(0.5)
        hids = hid_devices(addr)
    prof["hid"] = hids
    if not hids:
        print("hid       no kernel HID device for this address (connected? an LE device's "
              "HID appears only once its services resolve)")
    events = []
    for h in hids:
        parsed = parse_descriptor(bytes.fromhex(h["descriptor_hex"]))
        h["parsed"] = parsed
        print(f"hid       {h['sysfs']}  '{h['name']}'  driver {h['driver']}  "
              f"descriptor {h['descriptor_len']} bytes  input nodes {', '.join(h['events']) or '-'}"
              f"{('  motion (not watched) ' + ', '.join(h['motion_events'])) if h['motion_events'] else ''}")
        for a in parsed["applications"]:
            print(f"  application  {a}")
        for rid, r in parsed["reports"].items():
            print(f"  report {rid}  in {r['input_bytes']} B  out {r['output_bytes']} B  "
                  f"feature {r['feature_bytes']} B")
            for f in r["fields"]:
                print(f"      {f}")
        events += h["events"]
    if args.secs > 0 and events:
        prof["events_seen"] = watch(events, args.secs)
    os.makedirs(args.out, exist_ok=True)
    stem = re.sub(r"[^A-Za-z0-9]+", "-", i.get("Name", addr)).strip("-").lower() or addr
    path = os.path.join(args.out, f"{stem}-{addr[-5:].replace(':', '').lower()}.json")
    with open(path, "w") as f:
        json.dump(prof, f, indent=1)
    print(f"profile: written to {path}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("scan"); p.add_argument("secs", nargs="?", type=int, default=15)
    p = sub.add_parser("pair"); p.add_argument("addr", help="address, or a name to match")
    p = sub.add_parser("profile"); p.add_argument("addr")
    p.add_argument("secs", nargs="?", type=int, default=0)
    p.add_argument("--out", default="bt-profiles")
    p = sub.add_parser("remove"); p.add_argument("addr")
    args = ap.parse_args()
    if hasattr(args, "addr") and args.cmd != "pair" and not ADDR.match(args.addr):
        sys.exit(f"{args.addr}: not an address like E4:17:D8:17:4D:7F")
    return {"scan": cmd_scan, "pair": cmd_pair, "profile": cmd_profile,
            "remove": cmd_remove}[args.cmd](args) or 0


if __name__ == "__main__":
    sys.exit(main())
