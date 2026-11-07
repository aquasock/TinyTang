**Not posted.** Draft comment for
[tinydesk-project/tinydesk issue #7](https://github.com/tinydesk-project/tinydesk/issues/7),
answering the interface question the maintainer raised there. Post only with
the user's approval. The work it describes is in this repository as
`ports/bl616/tang_bt_ops.h` and `ports/bl616/tang_bt_port.c`, built and passing
the host gate; it has not been tested on the board yet, because the board was
not connected when the cycle ended.

Everything below the rule is the comment as it would appear.

---

The port built it, as you suggested, and I shaped the surface the way you asked
so it can become the upstream interface without renaming. It is in
`ports/bl616/tang_bt_ops.h` in TinyTang
(<https://github.com/aquasock/TinyTang>), and it is a small flat table with the
same asynchrony contract as `td_net_ops_t`: every call returns at once, slow
work runs in the background, and the outcome appears in `status()`.

Your five verbs, and what they are called:

- `status(td_bt_status_t *)` — the radio, every slot, and the last operation's
  message. Carries `busy`, `job_failed`, and `wifi_exclusive`.
- `scan()` → bool, with the refusal reason in `status().message`, the way
  `td_net_ops_t` reports a refused connect.
- `scan_results(td_bt_device_t *, int)` → devices found, strongest first, and
  **-1 while a scan is running** so a caller can tell when to collect without
  knowing anything about jobs.
- `pair(int slot, const td_bt_device_t *)` — the one place Bluetooth's nouns
  differ from Wi-Fi's: a device goes into a *slot*, and which slot is the
  caller's decision.
- `paired(td_bt_paired_t *, int)` — the stored pairings, one per slot.
- `forget(int slot)`.
- `disconnect(int slot)` — keeps the pairing and stops using the device, which
  is the difference between this and `forget`.
- `set_reconnect(int slot, bool)` — whether that slot's device comes back by
  itself.

`td_bt_status_t` carries `wifi_exclusive`, as you asked: this board has one
LE stack and no room for both, so it reports `true` rather than failing at
runtime when something tries to start Wi-Fi.

Two things I would ask for upstream, when it is convenient:

1. `const td_bt_ops_t *bt` beside `net` in `td_sysinfo_t`, so a desktop app
   gets it the way `network.c` gets its table (`td_sysinfo()->net`). Today the
   port publishes it as `td_bt_port_ops()` and the window calls that, which
   works but is a port detail.
2. Nothing else — the header is self-contained, and a port without Bluetooth
   returns NULL, which the window already handles by declining to open.

The keyboard and mouse reach TinyDesk through the normal input path, as you
said was the right way round: the window only manages pairing. The engine
behind the table is unchanged; this cycle put a shape in front of it and moved
the window onto that shape, so the change is provably behaviour-free — the host
test that drives the real window inside the real window manager now links
without the engine at all, which is what proves the window goes through the
interface.

Happy to send it as a patch to `td_sysinfo.h` if you want the member added
before this lands anywhere.
