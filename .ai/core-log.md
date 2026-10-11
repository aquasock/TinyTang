## 1 COMMIT Unreleased 2026-10-10T14:20:43-07:00

#### Coming From:

Unreleased b0aea6f

#### Purpose:

Open the new active log with the live project state and the next build cycle's plan, after the user archived the previous log and cleared it.

#### Outcome:

The previous active log was archived and cleared on the user's direction, in commit `b0aea6f`, which added the archive and emptied `.ai/core-log.md` without an entry in the same commit; that log held 38 entries and its complete contents are preserved byte-identical in `.ai/archived_logs/core-log_2026-10-10T141714-0700.tar.gz`, a single-member `core-log.md` in the same gzip'd GNU-tar shape as the 2026-10-07 archive beside it, verified by extraction against both the working tree and the committed object before this entry was written. The archived log's last entry is the numbering correction the previous cycle made: the active log had run from 41 to 77 since the user-directed trim of 2026-10-07, the project's own `tools/check_core_log.py` rejected every entry, and the correction renumbered the headers to 1 to 37, carried 114 in-document citations with them, qualified five citations to sealed history as `archived entry 26`, `28`, `37`, `39` and `40`, and left 18 citations to the Tang-Phosphor and Tang-Build logs untouched after each was checked against those repositories' own logs, so the checker now passes. Its entries before that recorded the removal of the Sipeed schematics and the seven board photographs, and the PMOD socket-retry change in `ports/bl616/phosphor/pmod_sockets.cpp` that is committed but has never been built or flashed. The state this log opens on is otherwise unchanged and was verified this cycle: the repository is at `b0aea6f` and clean apart from a dirty `third_party/tinydesk` submodule, which is the carried patch `0001-settings-bluetooth-button.patch` applied in place by `scripts/apply-tinydesk-patches.sh` at every configure and is therefore a revert rather than unrecorded work; the board is attached in two-wire mode as `ffff:5454` on `/dev/ttyACM0`, so `/dev/ttyACM0` exists and the board is reachable with short single commands; and the toolchain the build needs is present, the Bouffalo SDK at `~/.cache/tangcore-dev/sdk`, `riscv64-unknown-elf-gcc` under `~/.cache/tangcore-dev/toolchain/bin`, and a nestang checkout at `../tangcore/nestang`. The desktop core runs with VGA and without HDMI, which is characterised and closed in the Tang-Build project and is not work to reopen here; an open-flow build carries no HDMI because the device database's clock model has no entry into any HCLK block, so no build here should be chosen in the expectation that one brings the display up. Nothing was built, deployed or tested, so all three statuses are not applicable, and the required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as entry 1 of the new active log under the archival procedure, with six canonical sections, prose in Outcome and Next Steps, an allowed Status set and a count of one within the 100-entry limit.

#### Next Steps:

The next build cycle is the one this log opens on, and its work is the socket-register retry that has been committed since 2026-10-10 without ever running: build the BL616 firmware with `make CHIP=bl616 BOARD=bl616dk`, confirm against the last qualified image that the only source change is the retry in `ports/bl616/phosphor/pmod_sockets.cpp`, where `read_reg` and `write_reg` now attempt three times at 20 ms after a single missed read used to return `nullptr`, skip the socket declaration, send no `/tang.ini` and leave the screen dark with nothing printed; flash it with `tools/tinytang_flash.py`; have the user power-cycle; and check that `platform` prints the new build identity rather than the previous one. Acceptance is the part still open and it should be settled before the cycle starts rather than after: the retry fires only on a miss, so a clean first read passes with or without the fix, and the log's own wording asks that a socket declaration survive a first-miss read, which needs either a deliberate way to force the miss or an agreed bound such as a stated number of load cycles with no dark screen, because a result that could not have failed must not be recorded as a pass. The second item is the dirty `third_party/tinydesk` submodule, resolved by a `git checkout` inside it once the patch script is confirmed to reproduce the change at configure time. No other work in this repository is pending, and the entries that carry the detail behind all of this are in the archive named above, which `.ai/core.md` says is not to be inspected or cited without the user's approval, so anything needed from them should be asked for rather than assumed.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 2 COMMIT Unreleased 2026-10-10T15:07:00-07:00

#### Coming From:

Unreleased b0aea6f

#### Purpose:

Build the PMOD socket-declaration retry that previous cycles left as source only, deploy it, and confirm on hardware that a socket declaration survives a core load.

#### Outcome:

The retry is now built, deployed and accepted on hardware rather than recorded as source. The firmware was built with `make CHIP=bl616 BOARD=bl616dk`, giving identity `b0aea6f-dirty.945fd1e` in `build/build_out/tinytang_bl616.bin`, 655008 bytes, MD5 `d4773d1e57693b18b8e693731aa9058b`, with no warnings or errors in the build log; the firmware source difference since the previous binary's base commit `eb3fbe1` is the retry in `ports/bl616/phosphor/pmod_sockets.cpp` together with the About-window line the previous cycle already qualified and accepted, so the retry is the only change in this image that was not already accepted on hardware, and the build's `-dirty` hash reflects the uncommitted log entry this cycle writes. It was installed with `tools/tinytang_flash.py --yes`, which uploaded 655008 bytes to `/tinytang-upload.bin` and staged and committed it, and after the user's power cycle `platform` reported `b0aea6f-dirty.945fd1e` where the board had run `eb3fbe1-dirty.71886e5` before. The board returned from the loader as `ffff:5454` on `/dev/ttyACM0` exactly as FLS-001 describes, and the previous binary in `build/build_out` was overwritten by this build, so the pre-fix image is not retained as a file. The acceptance evidence is 14 core loads with the declaration checked on every one: eight `tangload` runs of `/cores/console138k/desktop.bin` and six of `/cores/console138k/phosphortang.bin`, the core the dark screen was reported on, and all 14 printed `tang.ini: pmod0 vga_j2, pmod1 vga_j1; word 0x0230` followed by `tang.ini: sockets declared (0xc0 = 0x0230)`. The line's absence is the old defect's signature, because `tang_ini_core_loaded()` returns silently when the ABI read at `0x04` misses, so a skipped declaration would have appeared as a load with no declaration line and no error, and none occurred. That is a bound and not a proof, and it is recorded as one: nothing in the firmware reports how often `read_reg` had to retry, so a recovered first-miss is indistinguishable from a load that never missed, and the instrumented build that would count retries was offered and declined in favour of accepting the 14-load bound. The user confirmed the screen and the PmodVGA output as correct, which is this cycle's hardware acceptance, and the board was left in its normal state with the desktop core loaded and `tangini` reporting `core 0x54 (ABI 1.1) has 0xc0 = 0x0230, as declared`. One process finding is recorded rather than fixed: `tools/tinytang_flash.py` blocks on its `install ... ? [y/N]` prompt when stdin is not a terminal, which hung the first flash attempt with `/dev/ttyACM0` held open until the process was killed and re-run with `--yes`. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 2 of the new active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set and a count within the 100-entry limit.

#### Next Steps:

No engineering work remains on the retry. The one open item is the dirty `third_party/tinydesk` submodule, resolved by a `git checkout` inside it once `scripts/apply-tinydesk-patches.sh` is confirmed to reproduce the change at configure time, because the dirt is that patch applied in place and not unrecorded work. Two small follow-ups are named rather than done. `tools/tinytang_flash.py` should either pass `--yes` implicitly when stdin is not a terminal or say plainly in its usage that `--yes` is required without a TTY, since the prompt is invisible in an agent session and hangs while holding the console port. And if proof that the retry actually fires is ever wanted, the instrumented build that counts retries in `read_reg` and `write_reg` remains the way to get it, because the 14-load soak can only bound the miss rate rather than show a recovery. Nothing else in this repository is pending, and the detail behind all of it sits in the archived log, which is not to be inspected or cited without the user's approval.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 3 COMMIT Unreleased 2026-10-10T17:49:05-07:00

#### Coming From:

Unreleased 21d8a12

#### Purpose:

Record the Bluetooth controller cycle, which produced two firmware fixes in the BLE layer and a complete diagnosis of the Xbox pad's connection, up to the point where it refuses to stay attached.

#### Outcome:

The user asked for the Xbox Wireless Controller to work over Bluetooth, and the cycle produced two committed changes in `ports/bl616/tang_ble.c`, built as `21d8a12-dirty.8668742` and flashed to the board. The first writes Exit Suspend: HID over GATT lets a device hold its reports while it considers itself suspended, and nothing here had ever written the HID Control Point (`0x2A4C`) -- so `hid_setup_next` now writes `0x01` to it once the subscriptions are complete and logs `exit suspend -> 0x001A (0)`, and the pad accepts it. The pad's service map was captured in the process: the HID service is `0x0016`-`0x0023` with HID Information at `0x0018`, the Control Point at `0x001A`, the Report Map at `0x001C`, an input Report at `0x001E` (notify and read, CCC at `0x001F`) and an output Report at `0x0022` (write only). The CCC is correctly bound to its input, the subscription is accepted, and `0x001F` reads back `0x0001`, so the discovery, the binding and the subscription are all right and the pad simply sent nothing before this change. The second is the stale-key deadlock, which the board's own logs captured as six `security failed (level 1, err 8)` attempts and then an `err 2` in a row, each followed by `disconnected (HCI 0x3E)`: putting an Xbox pad into pairing mode makes it forget its host while this board still holds a key for it, so the stack tried to ENCRYPT against a key the pad no longer had and never re-paired, and a manual `forget` was the only escape -- which is exactly the "then you never can" the user had observed. A security failure at a bonded slot now drops the stored key, on the card and in the host's table, and leaves the slot armed so the next connect pairs afresh; `hid_drop_key` is that drop without the disconnect and the disarm, and `hid_forget` is now `hid_off` plus it. That fix was then narrowed in a second build after it proved too broad: `err 8` is UNSPECIFIED and is what this board reports on signal alone, as `BLE-001` says and as the reference warns not to read as the device refusing, so acting on it threw away a good pairing every time the link was marginal -- which it did, repeatedly, undoing the pairing while it looked like progress was being made. Only `PIN_OR_KEY_MISSING` is acted on now. What does not work yet is the whole of what the user asked for: the pad disconnects seconds after the subscription with `disconnected (HCI 0x13)`, the pad's own choice, so not one report has ever arrived and the controller does not work; and the automatic reconnect does not land it either, with a valid bond on the card, the pad awake and the whitelist initiator armed, while the K950 and M750 reconnect on the same board. The user's question about the desktop was settled on the way: the Bluetooth window calls the same `tang_ble_scan_start`, `tang_ble_pair_start` and `tang_ble_info`, shows the same slot state, and the user's own test through it showed no reports either, so there is no working path that the console lacks. Three errors of the agent's own are recorded so they cost nothing next time: `blescan` and `pair` both stop the whitelist initiator, which `BLE-011` states and which means every diagnostic scan was pausing the only reconnect mechanism this SDK has; overlapping console commands collided on the serial port, which showed as `multiple access on port?` and as spurious `none found`; and `tinytang_run.py --seconds N` streams for N seconds after the command, so long windows made every step a minute long for no reason. The board is left on the flashed build with the pairing on the card, and nothing else is pending in it.

#### Next Steps:

The post-subscribe disconnect is what stands between this and the report question, and connection parameters are the prime suspect: a HID peripheral that asks for a particular interval, latency and supervision timeout will terminate the link when the host does not grant it, and `HCI 0x13` seconds after the CCC write is what that looks like from this side. Log the parameter update request and adopt what the peripheral asks for, which is one small change. After that, the reconnect wants a plain `connect` verb that does an explicit connect without re-pairing, and a scan-based fallback for a waiting paired slot, because the whitelist initiator does not land this device. The pad's service map, the Exit Suspend behaviour and the stale-key deadlock belong in `.ai/core-reference.md` as records of their own, which this cycle did not add.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 4 COMMIT Unreleased 2026-10-10T18:25:56-07:00

#### Coming From:

Unreleased 1da7838

#### Purpose:

Read the Xbox pad's HID Report Map, which this firmware had never fetched, and record the ATT MTU exchange that was built to make that possible -- and disproved.

#### Outcome:

The pad's Report Map is now read: 283 bytes, arriving in 26 chunks. Two changes were needed in `ports/bl616/tang_ble.c`, and only the second of them was the one that worked.

The first negotiates an ATT MTU when a device connects: `bt_gatt_exchange_mtu` in the connected callback, and `bt_gatt_register_mtu_callback` at bring-up, which is available only because `BFLB_BLE_MTU_CHANGE_CB` is defined in the SDK's own `config.h`. The reasoning was that the board runs at the default MTU of 23 -- `grep mtu` across the whole port returned nothing before this change -- so a Report Map of tens of bytes could not be read in one request. That reasoning was wrong, and the log says so plainly: in a complete setup capture, from `connected; requesting encryption` through `ready`, neither a `mtu now` line nor a failure line ever appeared; the map's chunks come back 22 bytes at a time, which is MTU 23 minus one; and the pad's input report turns out to be 17 bytes, which fits in a 23-byte MTU notification with room to spare. So the exchange either never took or never reported, the MTU was never the obstacle to reports, and the claim that it was had been stated twice as a mechanism before anything was observed.

The second change is what actually worked, and it came from reading the SDK's documentation rather than from guessing. `gatt.h` states that a value longer than the ATT MTU is not continued by the stack: "the caller will need to read the remaining data separately using the handle and offset". Our single read at offset 0 was therefore everything we could ever get, and 22 bytes was not the pad being awkward -- it was the ceiling. `hid_read_done` now re-issues the read from `params->single.offset + length` whenever a chunk filled the MTU, and the map arrives whole. The read-back logger had to change with it: it printed two bytes, which describes a CCC correctly and a descriptor uselessly, so reads longer than four bytes now dump in 12-byte chunks carrying their absolute offset, because a log line is 96 bytes and the map is far longer.

The pad is now decoded rather than guessed at. It is a `Game Pad` (`09 05`) on Report ID 1, with a 17-byte input report: X, Y, Z and Rz as four 16-bit axes on a logical range of 0 to 65535; Brake and Accelerator as 10-bit fields on 0 to 1023, each followed by six bits of padding; one 4-bit Hat switch, logical 1 to 8 with a null state and a physical range in degrees; fifteen 1-bit buttons; and one Consumer-page bit followed by seven bits of padding. Report ID 3 is an output report on the PID usage page: four 8-bit motor-scale fields on 0 to 100 and three 8-bit fields on 0 to 255. The pad's GATT map also shows a 128-bit vendor-specific service at `0x0024`-`0x002A`, which our discovery has never walked because it only ever queries the HID service.

What still does not work is the whole of what the user asked for. The pad accepts the subscription (`0x001F` reads back `01 00`), takes Exit Suspend, and stays connected -- and sends not one notification, with buttons held down. `hid_notify` increments its report counter on any data before any other test, so a notification arriving cannot be missed; `0 report(s)` is therefore evidence that nothing was sent, not that something was dropped. The `HCI 0x13` disconnect is intermittent rather than deterministic: the pad completed the entire setup and sat `ready` at least once, and hung up at other times. Signal remains a live factor rather than a suspect that has been excluded -- the first pairing scan in this cycle found nothing while the next found the device at -73 dBm a minute later, and the board carries no antenna (board fact 17, and `BLE-001` on why a failure here is not the device refusing).

#### Next Steps:

The 128-bit vendor-specific service at `0x0024`-`0x002A` is the next surface to read: discovery only walks the HID service, and on Xbox pads the vendor service is where the handshake lives, which would explain a device that completes a standard HID setup and then declines to report. Connection parameters are the second candidate for the intermittent disconnect -- a HID peripheral that asks for a particular interval, latency and supervision timeout can terminate a link that does not grant it, and `HCI 0x13` shortly after the CCC write is what that looks like from this side. The `mtu now` line never firing is itself unexplained and worth one look: the callback registration point, not the exchange, is the thing to check.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 5 COMMIT Unreleased 2026-10-10T18:43:53-07:00

#### Coming From:

Unreleased 5ddeb99

#### Purpose:

Name the Xbox pad's vendor service, remove the ATT MTU exchange that was wrong twice, and record that the stale-key deadlock presents as a connection failure rather than a security error.

#### Outcome:

Three things came out of this cycle. Two of them are corrections to work already recorded.

The vendor service is named. Primary discovery had always seen a 128-bit service at `0x0024`-`0x002A`, but the log could only print `uuid (128-bit) 0x0000`, because the code pulled a 16-bit value out of a UUID and had nothing to show for a 128-bit one. It now prints the full value in both byte orders, so there is no doubt which end is which, and the first 128-bit service's range is recorded during primary discovery and its characteristics enumerated after the HID pass, which then carries on as before. The service is `00000001-5F60-4C4F-9C83-A7953298D40D`; a characteristic `00000002-5F60-4C4F-9C83-A7953298D40D` at value handle `0x0026` with properties `0x02` (read); `00000003-...` at value `0x0028`, properties `0x02` (read); and `00000004-...` at value `0x002A`, properties `0x08` (write). That is a vendor base UUID with Microsoft's own numbering rather than anything the Bluetooth SIG defines: two readable values and one writable, none of which this firmware has ever touched. Nothing is read or written in it yet -- describing an unknown vendor surface is safe, and poking one is how a working bond becomes a paperweight.

The ATT MTU exchange is removed, having been wrong twice. It was added in entry 4 on the theory that the default MTU of 23 could not carry the Report Map. Entry 4 already recorded that theory as dead, because the map is fetched in 22-byte pieces by `hid_read_done` and the pad's input report is 17 bytes, which fits a 23-byte MTU notification. This cycle showed the exchange was worse than useless on top of that: sent concurrently with the security procedure, the pad answered `BT_ATT_ERR_UNLIKELY` (ATT `0x0E`) and the link died immediately afterwards with `HCI 0x3E`, four attempts out of four, before discovery had begun -- so four pairing attempts produced nothing else at all. Removing it restored the earlier behaviour on the same pad at the same signal, and the next attempt completed the whole setup. `hid_mtu_changed` and its registration stay, because they cost nothing and would report an MTU negotiated from the device's side, but nothing is requested from ours.

The stale-key deadlock presents as a connection failure, not a security error, which is why entry 3's fix cannot see it. Four attempts in a row gave `connected; requesting encryption` and then `disconnected (HCI 0x3E)` with no `security failed` line between them. `blekbd forget`, which drops the stored key without disconnecting or disarming, ended it: the next connection ran the full setup. So the failure is exactly the deadlock entry 3 diagnosed and fixed, seen from a different angle -- the pad has forgotten its key entering pairing mode while the board still holds one, so every connect tries to ENCRYPT against a key the pad no longer has. Entry 3's fix keys on `PIN_OR_KEY_MISSING` arriving from the security callback, and in this failure mode that callback never fires, so the fix never reaches it and the slot loops. The trap is that `HCI 0x3E` is also what this board reports on signal alone (`BLE-001`), and entry 3 already paid an hour for acting on a code too broadly when it used `err 8`. The narrow trigger is a connect that reaches `requesting encryption` and then dies without ever logging `encrypted`, on a bonded slot -- a marginal link that never got that far would not fire it.

What still does not work is unchanged and now more precisely located. The pad connects, completes a textbook HID setup -- discovery, subscription, the full 283-byte Report Map, Exit Suspend -- reaches `ready`, and then hangs up with `HCI 0x13`. Not one report has ever arrived, with buttons held down. It is not the HID service: the pad does everything asked of it there and then leaves. The vendor service above is the unexplored surface, and its single write characteristic is where a device that behaves this way usually wants a handshake.

Two agent errors belong here so they cost nothing next time. The retry loop around `pair` is necessary because `HID_PAIR_SCAN_SECS` is a fixed 8 seconds and one attempt races the turn that starts it, but a pass still connecting wedges the slot and every later pass answers `already connecting; use 'blekbd off' first`, so the loop must clear the slot first or it wastes three passes in four. And `tail -N` was used twice on console output that contained the thing being looked for: the first time it cut the `mtu exchange failed` line out of a setup log, hiding that the exchange was failing rather than absent; the second time it cut the vendor discovery out of the very pass that produced it. Capture console output whole.

#### Next Steps:

Read `0x0026` and `0x0028` in the vendor service, which is safe and may name the handshake outright; the read machinery and its long-value continuation are already in place. Then `0x002A`, the write, which is the likely key to reports and deserves thought before it is poked. Separately, the stale-key trigger wants narrowing as described, because a pad put into pairing mode currently wedges the slot until `forget` is run by hand, and that is the user-visible behaviour that started this whole line of work.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 6 COMMIT Unreleased 2026-10-10T20:03:15-07:00

#### Coming From:

Unreleased 799c0ac

#### Purpose:

Record the cycle that proved the pad works over LE HID on a known-good host, fitted the antenna the board never had, reordered the GATT setup to match the hosts that work, and located the remaining failure at the link layer rather than in the HID conversation.

#### Outcome:

Four things were established, three of them corrections to what entries 3 to 5 believed.

The antenna was the missing hardware, and it fixed the connection. Board fact 17 says the board has no Bluetooth antenna, only a U.FL jack, and every cycle so far ran with nothing on it. A 2.4 GHz 6 dBi antenna on a U.FL/IPEX to RP-SMA pigtail was fitted -- the user had to seat a U.FL connector blind on the dock's underside, which is a job in itself. The pad read -70 dBm held against the board before, and -83 dBm further away; it now reads **-33 dBm**. Every connection failure on record -- the `HCI 0x3E` at `requesting encryption`, the wedged slots, the flaky pairing, the "then you never can" the user first reported -- stopped happening. The first pairing attempt after the antenna fitted encrypted, bonded and ran the entire setup. Signal was a far larger part of this problem than entries 3 to 5 credited, and `BLE-001`'s note that this board reports `0x3E` on signal alone was the right read of it.

The pad demonstrably works on a known-good host, which settles where the fault is. The pad was paired to the user's desktop -- Ubuntu 26.04, BlueZ 5.85, on an adapter that had never been used before -- and BlueZ built a HID device from the pad's own Report Map: the kernel logged `Bus=0005` with `Handlers=kbd event27 js1`, and with the user pressing buttons `/dev/input/js1` produced **782 live events**, axes swinging the full +/-32767 and buttons 0 through 14. So the pad is an ordinary LE HID device that reports over GATT, and the descriptor this project decoded in entry 4 was correct. The fault is in this port's GATT host. Two theories died with it: the pad advertises `UUID 0x1812` over LE and does **not** answer a classic BR/EDR inquiry, so the Bluetooth-Classic theory raised late in the cycle is dead; and Bluepad32's `uni_hid_parser_xboxone.c` performs no vendor write at all -- it hard-codes a known HID descriptor, never reads the pad's own map, and simply receives -- so the vendor service at `0x002A` that entry 5 named is real and worth knowing, but writing to it is not a prerequisite for this pad reporting.

The GATT setup was reordered to match the implementations that work. This firmware subscribed to the input report and read the Report Map afterwards. Both BlueZ and Bluepad32 have the map in hand before notifications are enabled, and BlueZ cannot build the HID device until it has parsed it. HID Information (`0x2A4A`, which this firmware had never once fetched) and the Report Map are now read before the subscriptions, with the CCC read-backs after them: `hid_setup_next` runs reads up to `setup_read_pre`, then subscriptions, then the rest. The change made no difference to the pad's behaviour, but it is the correct order and it stays.

The remaining failure is at the link layer, during discovery. With the correct order in place the pad pairs -- `encrypted (level 2)`, `paired (bonded; saving to the card)` -- and then comes `no HID service on this device` followed by `disconnected (HCI 0x08)`. `0x08` is a supervision timeout, and "no HID service" is this code's message for primary discovery completing with nothing recorded, which cannot mean the services are absent on a device whose six services BlueZ enumerated an hour earlier. The discovery is being run on a link that has already gone. Worse, entry 3's fix then drops the key and the slot re-pairs, so the whole thing repeats: the board churned through that loop hard enough that the console stopped answering the status probe and needed a power cycle to recover.

Four agent errors belong here. The MTU exchange removed in entry 5 was never the issue and its removal changed nothing. A full hour went into a Bluetooth-Classic theory that one `blescan` disproved. The user's PC was left bonded to the pad, and because BlueZ's HID profile auto-reconnects to a bonded device, the PC kept taking the controller away from the board mid-test. And `tail` was used on console output three times, twice cutting out the decisive line.

#### Next Steps:

Connection parameters are the lead. A peripheral that asks for a particular interval, latency and supervision timeout terminates a link the host does not configure, and a supervision timeout immediately after pairing is that fingerprint; entry 3 named these as the suspect and the antenna has since removed signal as the competing explanation. Log the connection parameter update request and grant what the pad asks for. Second: the `no HID service` path is being reached on a dying link and should say why -- an ATT error, a failed discovery, or a link that went away -- because as written it reports a device with no HID service when the truth is a device that had already left the building, and that misdirection cost time in this cycle.

Operating notes for whoever continues: the pad's pairing mode lasts only a few minutes and `HID_PAIR_SCAN_SECS` is a fixed 8 seconds, so an attempt has to be fired while the pad is still advertising, and a retry loop longer than the window wastes its later passes and can wedge the slot; `blekbd off` clears a wedged slot. `blekbd forget` clears a bad key by hand, though entry 3's fix now does it automatically on `PIN_OR_KEY_MISSING`. Capture console output whole. And the pad holds a key for this board from a pairing later in the cycle, while the desktop's bond was removed.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 7 COMMIT Unreleased 2026-10-10T20:37:46-07:00

#### Coming From:

Unreleased 0121f7c

#### Purpose:

Instrument the Bluetooth LE host so that the reason the user's Xbox Wireless Controller pairs with the board and then delivers no report could be read off the board rather than theorised, after five cycles of hypotheses that were stated before they were observed.

#### Outcome:

The port now records what the link actually does. `ports/bl616/tang_ble.c` gained a `le_param_updated` callback and a `bt_conn_get_info` call on connect and on disconnect that log the connection interval, latency and supervision timeout, a count of the primary services the discovery walk saw so that `no HID service on this device` can no longer be printed without saying how many were found, a decoded HCI reason on disconnect, and a log ring raised from 48 to 128 lines so one whole connect cycle survives; nothing else changed. It was built with `make CHIP=bl616 BOARD=bl616dk`, clean and warning-free, as identity `0121f7c-dirty.a1b71f2`, 657936 bytes, MD5 `3ae244e101d88f55841fcd861c74d91a`, deployed with `tools/tinytang_flash.py --yes`, and confirmed on the board by `platform` after the user's power cycle. The trace changed the picture in three ways. The link is created at interval 40 (50 ms), latency 0 and supervision timeout 400 (4 s), and `le_param_updated` never fired once across every attempt, so the pad never asks for different parameters; a working BlueZ host imposes nothing and takes the Linux kernel default of 0x0018 to 0x0028 (30-50 ms, `net/bluetooth/hci_core.c`), so connection parameters are eliminated as the cause and the previous cycle's plan to grant what the pad asks for would have been another wasted build. Every bonded pass ended `primary discovery done: 0 service(s) seen` followed by a supervision timeout, so that line reports a link that has already gone, not a device without a HID service. And two candidates were killed by reading rather than building: the post-pairing SD write cannot starve the link because the SDK's host and controller tasks run at priority 28 to 30 against this port's `ble_task` at 3, and no Microsoft handshake is missing because Bluepad32's `uni_hid_parser_xboxone.c` performs no vendor write at all. The cycle's real damage was the agent's, not the board's. `security failed (level 1, err 8)` followed by `HCI 0x3E` is already recorded in `.ai/core-reference.md` as BLE-001, BLE-009 and BLE-012: a benign first-attempt failure that clears on the next try, which entry 3's `hid_drop_key` handles by going back to waiting, and which BLE-012 says needs no special handling. The agent neither consulted that file nor retried the transient; it stopped after one attempt and ran `blekbd forget`, which deleted the board's stored key from RAM and from `/sd/ble/bonds.bin` while the pad still held its end, so the board can no longer encrypt to the pad without a fresh pairing. The detour's cause is that `.ai/core.md`'s recovery policy names only `core.md` and `core-log.md` and describes `core-reference.md` solely as a standards lookup, so the file that answers exactly this question was never opened; the user identified this and directed that it be read. Also recorded: the user corrected the agent's testing discipline, that a pad left idle, asleep or not in pairing mode makes every fired command meaningless and that `--seconds` is a streaming window rather than a delay; no reference capture was possible because `btmon` cannot bind without CAP_NET_ADMIN (`Failed to bind channel: Operation not permitted`); and the desktop's radio was powered off and soft-blocked so it would stop taking the pad. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 7 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

Re-bond the pad and watch for a report, which is the only engineering work left on this line: put the pad into pairing mode, run `blekbd pair Xbox`, and when the first attempt logs `security failed (err 8)` followed by `HCI 0x3E`, let it retry instead of stopping, because BLE-012 records that a following attempt is the one that encrypts, then confirm with `blekbd watch` whether any report arrives, since the controller has never produced one; the pad's state is invisible to the agent, which is why earlier attempts were noise, so the pad must be in pairing mode when the command is fired. Three records belong in `.ai/core-reference.md` and are still missing: the pad's GATT map (HID service 0x0016-0x0023, HID Information 0x0018, Control Point 0x001A, Report Map 0x001C, input Report 0x001E with CCC 0x001F, output Report 0x0022, and the 128-bit vendor service `00000001-5F60-4C4F-9C83-A7953298D40D` at 0x0024-0x002A), its 17-byte input report on Report ID 1 decoded from the 283-byte Report Map, and the pad-specific signature of a first encryption attempt failing with error 8 and HCI 0x3E. The detour should also be closed at its source: `.ai/core.md`'s recovery policy ought to direct an agent to read `.ai/core-reference.md` as this project's verified findings rather than only as a standards lookup, which the user has agreed with, but `.ai/core.md` is RESTRICTED and needs the user's explicit request before it is changed.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 8 COMMIT Unreleased 2026-10-10T23:44:45-07:00

#### Coming From:

Unreleased f6fe420

#### Purpose:

Find out why the user's Xbox pad, once paired, delivers no report, after the user called the whole controller line suspect and directed a revert and a series of instrumented builds rather than another theory.

#### Outcome:

The pad's failure was localised to the second after bonding, and none of the hypotheses the earlier cycles carried survived. `ports/bl616/tang_ble.c` was first reverted to `799c0ac` (entry 5), verified byte-identical, built as `f6fe420-dirty.8510c77`, 656896 bytes, MD5 `008ec5034c6888bc21b34e32625756c8`, and flashed; with the board's bond cleared by `blekbd forget` and the pad awake in pairing mode at -32 and -34 dBm, both attempts still died at `connected; requesting encryption` then `disconnected (HCI 0x3E)` with no `encrypted` line, which eliminates the entry-6 GATT reorder and the stale key alike, because the same firmware had been recorded reaching `ready`. A diagnostic build then added the service count, a decoded HCI reason, a 128-line ring and an explicit `bt_gatt_read` of handle `0x0003` whose callback reports an ATT error, and it returned `primary discovery done: 0 service(s) seen` with `probe read 0x0003 sent (-128)`; -128 is -ENOTCONN, checked against the toolchain's `sys/errno.h` (`ENOTCONN 128`) and `blestack/src/host/gatt.c`, so the host's ATT layer already held the link to be gone when discovery ran, and `no HID service on this device` had never been about services. A hands-off experiment then issued nothing at all for eight seconds after `encrypted` -- no discovery, no arming, no bond save -- and probed at 1, 2, 3, 5 and 8 seconds; the link was reported alive at every mark, yet died 8750 to 8801 ms after bonding on a 4 s supervision timeout, which places the pad's last packet at about 4.75 s, inside the hold, and not one of the five probes was ever answered. The decisive build probed `0x0003` on connect before any security was requested, and the pad answered twice, 22 bytes then 2, spelling `Xbox Wireless Controller`; after the security procedure the same read never came back at all. So the pad serves ATT perfectly and stops the instant encryption completes, and a bond captured on that build is a proper LE Secure Connections bond (`security level 2, enc_size 16, keys 0x0022, flags 0x10`, `key parts: LTK_P256 IRK; LE Secure Connections`), which eliminates the legacy-pairing theory too; the pad does answer ATT errors when it chooses to, `ATT 0x05` (Insufficient Authentication) to an unencrypted read once a bond exists, so its server is alive and the attribute is valid. Two other things came from the board: the controller needs fast blink for pairing mode and slow blink means idle, which several runs had read backwards, and the desktop does start and hold the console, where `tinytang_run.py` correctly refuses to send. Four errors of the night are recorded so they cost nothing next time: the SD-card test reported as exonerating the card write never ran, because the card's absence was assumed and never checked; the blink states were read backwards for hours; `tail` truncated console output again; and a full elimination list was published on top of the unchecked assumption. Six builds were made and flashed. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md`'s only change is the user's own edit to the AI Agent Recovery Policy, which adds "and open the record behind any hardware fact before acting on it" after the instruction to read this reference, and which entry 7's Next Steps had asked for, and validated this entry as number 8 of the active log with the six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

The next cycle has to explain the pad's post-encryption silence rather than theorise about it, because the pad demonstrably answers ATT before the bond and never after, and reports `err 2` (PIN_OR_KEY_MISSING) on the next connect, which is what two sides that disagree about a key look like. The instrumented build on the board already logs what was negotiated, so the cheapest next probes are the SMP parameters and a comparison against the desktop, which keeps this same pad connected; the committed `tang_ble.c` now carries the revert plus that instrumentation, and the port should be rebuilt and flashed from this commit before the next run. Three records entry 7 asked for are still owed to `.ai/core-reference.md` -- the pad's GATT map, its decoded 17-byte Report ID 1 report, and the first-encryption-fails signature -- and this cycle adds two more worth keeping as records: that the pad serves ATT before the bond and not after it, and that the controller's fast blink is pairing mode while slow blink is idle. The churn bound added here should stay, since a slot that never reaches ready now stops after three connections and waits for an explicit `on` instead of looping; and the SD-card test must actually be run before the card write is called exonerated.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 9 COMMIT Unreleased 2026-10-11T07:41:53-07:00

#### Coming From:

Unreleased 009368e

#### Purpose:

Record the user's revert of `ports/bl616/tang_ble.c` to the entry-7 firmware after entry 8's build stopped the Logitech M750 working, and bring the repository back in line with what the board runs.

#### Outcome:

Entry 8's firmware is dropped. Its `tang_ble.c` (the revert to `799c0ac`, the hands-off hold, the ATT probes and the churn bound) stopped the M750 working, so the user restored `tang_ble.c` from `286b133`, the entry-7 build, and built and flashed it after entry 8 was committed; that restore is the only source change in this commit, and the file is byte-identical to `286b133`'s. The image is `build/build_out/tinytang_bl616.bin`, built 2026-10-10T23:48 with `make CHIP=bl616 BOARD=bl616dk`, 657936 bytes, MD5 `bccd20bdcaf0e1a3b149fc0ea5c0a005`, identity `009368e-dirty.0a842a7`, and `platform` on the board reported that identity on 2026-10-11. The identity reads `009368e` only because that commit was checked out at build time; the firmware it carries is entry 7's. On that image `ble` showed the radio up, pairings loaded from `/sd/ble/bonds.bin`, 64,984 B of heap free with a 52,664 B largest block, the M750 paired and waiting, and the K950 slot unpaired. When the user moved the mouse it reconnected through the whitelist initiator without pairing, encrypted at level 2 on the first attempt, found its 7 services and 8 characteristics, read back all three CCCs as `01 00`, took Exit Suspend and reached ready in boot protocol with 188 reports counted. It also asked for and was granted new link parameters (interval 9, 11.25 ms; latency 44; timeout 216, 2160 ms), the request the Xbox pad has never made, which agrees with entry 7's elimination of connection parameters for the pad. The user did not report whether the pointer moved in the desktop, so the user test is not recorded as passed. The `third_party/tinydesk` submodule was reset with `git checkout -- apps/settings.c` after its diff was confirmed line for line equal to `third_party/patches/tinydesk/0001-settings-bluetooth-button.patch`, which `scripts/apply-tinydesk-patches.sh` reapplies at every configure, so the submodule shows dirty again after each build. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 9 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

The next cycle is project-control work: add to `.ai/core-reference.md` the records entries 7 and 8 left owed, namely the Xbox pad's GATT map, its 17-byte Report ID 1 input report decoded from the 283-byte Report Map, the first-encryption-fails signature, the pad answering ATT before the bond and not after it, and fast blink as pairing mode with slow blink as idle, each checked against the log entry that established it. The Xbox pad work resumes from this firmware afterwards, not from entry 8's, and any instrumentation added for it must leave the keyboard and mouse paths behaving as they do here. The user's confirmation that the pointer moves in the desktop on this image is still to be given.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 10 COMMIT Unreleased 2026-10-11T07:55:42-07:00

#### Coming From:

Unreleased 9268be7

#### Purpose:

Bring `.ai/core-reference.md` up to date and change it from an append-only record into a current one, at the user's direction, after its stale Bluetooth records misled the cycles of entries 7 and 8.

#### Outcome:

The user agreed that the reference, unlike this log, should hold what is true now and leave history to git and the log, and gave the agent licence to restructure it provided no current data was lost. Section 1's rule against rewriting a settled record was replaced by one that revises a record in place and ends its verification with a dated `Revised` note citing the log entry, the SUPERSEDED status and `superseded_by` field were removed, a rule was added to check that a record's cited code still exists before acting on it, and section 7 and the record template were brought into line. The 16 superseded records were folded into their successors, each successor rewritten to stand alone with every fact still true carried across, which a token comparison confirmed apart from figures that are no longer current (an abandoned 32-variable configuration's sizes, a pre-fix stack reading and two pre-fix heap readings, all still summarised in the new table and kept in git). Their ids, which code, `THIRD_PARTY.md` and the log still cite, are kept in a new section 8 that maps each retired id to the record holding its facts, with what it said. Twenty-one records were revised: DEV-006, FLS-002, PMOD-004, PMOD-006, TDSH-004, TDSH-008, TDESK-011, TDESK-013, PROV-005, TOOL-011, TOOL-016 and BLE-014 absorbed their predecessors; BLE-001 now records the 6 dBi antenna fitted on 2026-10-10; BLE-012 now records the stale-key failure and what `hid_security_changed()` does and does not handle, instead of saying a failed re-encryption needs no handling; BLE-002, BLE-006 and BLE-010 lost claims that pairings live in RAM only; BLE-005 names `hid_setup_reports()` and `hid_setup_next()` in place of three `kbd_*` functions that no longer exist; PMOD-002 and PMOD-004 no longer say the `/tang.ini` parser does not exist; and PROT-004 and TDSH-002 no longer cite `tang_osd_term.c`, removed on 2026-10-04. Seven other records had a citation of a retired id repointed and are otherwise byte-identical, and 102 records are untouched. Three records were added from entries 3 to 8: BLE-015, the Xbox Wireless Controller's pairing mode and GATT map; BLE-016, its 17-byte input report; and BLE-017, its open defect, recording both failure forms seen (a completed setup with no report and an `HCI 0x13` hang-up on the bare jack, and with the antenna no ATT answer after encryption, a supervision timeout and the key refused on the next connection), the causes ruled out, and key distribution labelled as a lead rather than a finding. Writing BLE-017 corrected entry 8's claim that the pad always stops answering when encryption completes, which entries 4 and 5 contradict. Records are now ordered by the topic catalog, the index was regenerated in that order with one line per live record, and routing gained four Bluetooth rows; the file is 133 records and parses as YAML in its rules, index and records blocks. Nothing was built or deployed. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled log entry was rewritten, and validated this entry as number 10 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

The Xbox controller work resumes from the entry-7 firmware on the board, starting from BLE-017: the cheapest next evidence is a btmon capture of the controller pairing with the user's desktop, run by the user with the privilege the agent lacks, compared against this board's pairing, with the desktop's bond removed and its radio blocked again afterwards. The user's confirmation that the M750's pointer moves in the desktop on this firmware is still owed from entry 9.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 11 COMMIT Unreleased 2026-10-11T08:28:15-07:00

#### Coming From:

Unreleased bb64cbb

#### Purpose:

Capture the Xbox Wireless Controller pairing with a working host, and test the one difference the capture pointed to, this board offering its own identity key, by building with host privacy off.

#### Outcome:

The user's desktop (BlueZ 5.85) was used as the reference host under `btmon`, run by the user because the agent lacks CAP_NET_ADMIN, and its radio was powered off and soft-blocked again afterwards with the controller's pairing removed. The first attempt failed on the agent's side, because `bluetoothctl` run non-interactively had no pairing agent and rejected the numeric-comparison step; even so the controller streamed input notifications on 0x001E over the unencrypted, unpaired link, 23 in two seconds, before any discovery or subscription. A second capture, `~/xbox-desktop2.snoop`, holds a successful pairing: LE Secure Connections Just Works, the controller asking for the host's IdKey but receiving nothing because BlueZ offered none, the controller then sending its IRK and identity address, reports starting 45 ms after encryption with nothing asked of the controller, 701 reports in all, and no ATT request ever sent by the controller. The capture also gave the controller's preferred connection parameters (7.5 to 15 ms, latency 0, 3 s), its PnP ID `045E:0B13` version `0x0509`, and that its report is 16 bytes over GATT, not the 17 BLE-016 had recorded. The one protocol difference from this board was that the SDK defaults `CONFIG_BT_PRIVACY` on, so the board offered and sent its own identity key and connected from a private address; `proj.conf` now sets `CONFIG_DISABLE_BT_HOST_PRIVACY 1`, the SDK's own switch, which the `blestack` compile flags and the map confirmed (`le_set_private_addr` no longer linked). The firmware built cleanly with `make CHIP=bl616 BOARD=bl616dk` as `bb64cbb-dirty.34f7c44`, 657936 bytes, MD5 `0bd5c660dc0c607bcf935dacba9d812f`, was installed with `tools/tinytang_flash.py --yes`, and `platform` reported it after the user's power cycle. It did not fix the controller: `blekbd pair Xbox` with the controller at -37 dBm paired, encrypted at level 2 and bonded, then found 0 services and ended in a supervision timeout, and the next connection got error 2 and re-paired into the same failure, twice before `blekbd off` stopped it, so the identity-key theory is ruled out. BLE-015, BLE-016 and BLE-017 in `.ai/core-reference.md` were revised with the capture, the corrected report length, the failed test, and the differences from BlueZ that remain untested, chief among them LE Secure Connections itself, which Bluepad32 5.0.0-beta0 disabled by default for controllers that fail key exchange. The M750 has not yet been checked on this image. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 11 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

Test LE Secure Connections off as the next single change, keeping privacy off so the board stays as close to BlueZ and Bluepad32 as possible: the SDK's `smp_set_auth()` (under `BFLB_BLE_SMP_LOCAL_AUTH`, which its `config.h` defines) overrides the pairing request's authentication flags, so calling it with bonding only when the stack starts makes the board request legacy pairing without patching the SDK. If that also fails, the remaining differences in BLE-017 are the link layer (data length, connection interval) and the pairing request's IO capability and MITM flag, and the M750 must be confirmed to reconnect on whichever image is kept.

#### Files Modified:

- proj.conf

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 12 COMMIT Unreleased 2026-10-11T08:39:24-07:00

#### Coming From:

Unreleased cb9c96c

#### Purpose:

Test LE legacy pairing in place of LE Secure Connections for the Xbox Wireless Controller, the change Bluepad32 made by default for controllers that fail key exchange.

#### Outcome:

Before the build the user moved the M750, and on the privacy-off image of entry 11 it reconnected from its saved bond and reached ready with 429 reports, so privacy off is safe for it. `ports/bl616/tang_ble.c` now calls the SDK's `smp_set_auth()` with Bonding only when the stack starts, so this board's pairing requests ask for legacy pairing; the function exists under `BFLB_BLE_SMP_LOCAL_AUTH`, which the SDK's `config.h` defines, and is declared locally because `host/smp.h` is not on this port's include path. The firmware built cleanly with `make CHIP=bl616 BOARD=bl616dk` as `cb9c96c-dirty.1055ce0`, 657952 bytes, MD5 `4db6191350ee0e7b95b63de203efc29d`, was installed with `tools/tinytang_flash.py --yes`, and `platform` reported it after the user's power cycle. It changed the controller's behaviour without fixing it. Over six pairings, the user repeating the experiment once to be sure, the controller encrypted, bonded and, for the first time on this board, answered ATT after encryption: discovery found all five primary services including HID at 0x0016-0x0023. Each time it then asked for its preferred parameters, which the board granted (interval 12, 15 ms; latency 0; timeout 300, 3 s), and the link ended in a supervision timeout about one timeout later, with `characteristic discovery failed (-128)` logged as the host learned of it; the next connection got error 2 and re-paired into the same failure until `blekbd off`. Two attempts in between ended in `HCI 0x3E` before encryption while the controller was still in pairing mode, and it then dropped to slow blink. The entry-11 BlueZ capture shows BlueZ moving the same controller to the same 15 ms and 3 s with its own LE Connection Update, after which 676 more reports arrived, so the parameters are not the fault; who starts the update, and how this board's controller carries it out, are. BLE-017 in `.ai/core-reference.md` was revised with this third failure form. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 12 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

Test the connection update as the single next change, keeping legacy pairing and privacy off: register an `le_param_req` callback that logs every parameter request with its slot and refuses it for the gamepad's slot only, so the M750's own update is untouched, and see whether the controller then stays connected and reports at the 50 ms interval. If it does, the follow-up is to reach the 15 ms the controller prefers the way BlueZ does, with a host-started update once setup is complete.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 13 COMMIT Unreleased 2026-10-11T09:06:15-07:00

#### Coming From:

Unreleased 9235969

#### Purpose:

Test whether the Xbox Wireless Controller's link dies because of the parameter switch it asks for, first by removing every other Bluetooth device from the board and then by refusing the request.

#### Outcome:

On the entry-12 image, with the M750's slot turned off so that nothing scanned in the background, the controller still died right after the 15 ms switch, so radio time shared with the reconnect scan is ruled out. The user then had every Logitech pairing removed, `blemouse forget` and `blekbd forget` followed by deleting `/ble/bonds.bin`, and from now on the controller is the only Bluetooth device while it is being worked on, at the user's direction; the earlier setup never tested more than two devices at once. A fresh reboot with the controller alone gave the same death after the switch, and in one session the link died at 50 ms just before the switch completed. `ports/bl616/tang_ble.c` then gained an `le_param_req` callback that logs every parameter request and refuses it for the gamepad's slot only, built cleanly as `9235969-dirty.ac83602`, 658160 bytes, MD5 `a3f2dbde4a7cac871bf72d22bcc4d3c0`, installed with `tools/tinytang_flash.py --yes` and confirmed by `platform` after a power cycle. The first run lost its decisive lines: the agent filtered the output of `blekbd off` with `grep -c`, and the board's log ring hands each line out once, so the lines could not be recovered; a memory rule now requires every console output to be saved whole first. After the user restarted the controller, the repeat was captured whole in a 45-second `blekbd watch`: three pairings, each followed by `link params asked: interval 6-12 latency 0 timeout 300; refused`, the link staying at 50 ms, discovery stalling after one or two answers, a supervision timeout, and error 2 on the next connection, with no report. The parameter switch is therefore ruled out, and BLE-017 records it. The host's own reply is not at fault either: the SDK's `le_conn_param_update_req()` allocates the response before calling the callback and sends it after. Since legacy pairing, most first connection attempts end in `HCI 0x3E` at -24 to -33 dBm, the controller never answering the board's connection, which BlueZ never saw; that is recorded as a second, separate fault. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 13 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

Stop testing single guesses and build the instrument: an HCI trace on the board, made by wrapping the host's `bt_recv` and `bt_send` at link time as `CMakeLists.txt` already does for the allocator, recording every event and every ACL packet in and out with a millisecond timestamp in a static ring and printing it with a shell command, so that one pairing on this board can be laid against `~/xbox-desktop2.snoop` line for line, including the controller's last packet and the `0x3E` connects. The parameter request should go back to being accepted, the SDK's behaviour, while it is still logged, so that the trace shows the board as it normally runs.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 14 COMMIT Unreleased 2026-10-11T09:40:18-07:00

#### Coming From:

Unreleased 68ca825

#### Purpose:

Build an HCI trace into the firmware so that a Xbox Wireless Controller pairing on this board can be read packet by packet and compared with the BlueZ capture, after three single-variable tests had each failed to find the fault.

#### Outcome:

The SDK host already calls `bt_monitor_send()` for every HCI command, event and ACL packet in both directions when `CONFIG_BT_DEBUG_MONITOR` is set, using the Linux monitor protocol's packet types; its own implementation prints to a debug UART this board does not bring out. `proj.conf` now sets that option, `CMakeLists.txt` wraps the function at link time, and the new `ports/bl616/tang_ble_trace.c` records each packet with a microsecond timestamp into a 16 KB linear capture, counting LE advertising reports instead of storing them, behind a new `bletrace [clear | dump [from [count]]]` command. The new `tools/ble_trace_to_btsnoop.py` turns a dump into a btsnoop file in the monitor datalink, which btmon 5.85 decodes; a synthetic HCI Reset round-tripped first. The controller's parameter request is accepted again, the SDK's behaviour, and still logged. The first build, `68ca825-dirty.eebf1ab`, captured a 627-packet session, but its dump came back cut off after about 4.5 KB and the capture was lost to the reflash that fixed it; the agent had not tried a full-size dump before asking for the session. The second build, `68ca825-dirty.c2a36fd`, 659568 bytes, MD5 `9b100e9fefceb82d6cac79cf0fd774e4`, added a one-tick pause per line and dump ranges; a 227-packet test trace of scans still came back cut at 137 lines unranged but complete in three ranges, identical where they overlapped and in time order, so one command's console output is capped at about 4.5 KB and dumps are fetched 80 packets at a time, which TOOL-006 now records. Both images were built with `make CHIP=bl616 BOARD=bl616dk`, installed with `tools/tinytang_flash.py --yes` and confirmed by `platform`; the full rebuild the configuration change forced shows the long-standing warnings in the vendored `tang_jtag_programmer.c` and a C++ flag warning, none from the new code. Two traced sessions followed with the controller alone. The first held three `0x3E` connects: the board's `LE Create Connection` matches BlueZ's apart from a 4 s supervision timeout against 420 ms, the controller reports the connection complete, and six connection events later it gives up, so the peer never answered at the link layer. The second, 409 packets, held four encrypted sessions: in each the controller's last data packet arrives about 0.15 s after its Identity Address Information, just after pairing completes on both sides, the link timing out about 8.7 to 9.7 s after connection, and at that same moment this board's host issues `LE Add Device To Resolving List`, `LE Set Privacy Mode` and `LE Set Address Resolution Enable` from `bt_id_add()`, which BlueZ never issued in its session. The agent first told the user the resolving-list commands were absent from the later sessions, which was a matching error in its own script against btmon's shortened command names, and corrected it. BLE-017 records the trace findings. No report arrived. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 14 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

Separate the two causes that coincide when the controller goes silent: this board's controller turning on address resolution for the controller's IRK, which BlueZ does not do, and anything in the controller itself at pairing completion. The test is to keep public-address peers out of the controller's resolving list, which the controller's public address never needs, by wrapping `bt_id_add()` and `bt_id_del()` at link time for public addresses only, and to trace one more pairing.

#### Files Modified:

- CMakeLists.txt
- proj.conf
- ports/bl616/tang_ble.c
- ports/bl616/tang_ble_trace.c
- tools/ble_trace_to_btsnoop.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 15 COMMIT Unreleased 2026-10-11T09:46:09-07:00

#### Coming From:

Unreleased 750bb0a

#### Purpose:

Test whether the Xbox Wireless Controller goes silent because this board's host turns on address resolution in its controller as pairing completes, by keeping public-address devices out of the controller's resolving list.

#### Outcome:

`ports/bl616/tang_ble.c` now wraps the host's `bt_id_add()` and `bt_id_del()` at link time (`-Wl,--wrap=bt_id_add -Wl,--wrap=bt_id_del` in `CMakeLists.txt`) and skips them, with a log line, for public-address devices, which a resolving list never serves; random-address devices keep the SDK's behaviour. The disassembly confirmed that the key-exchange handler `smp_ident_addr_info` and both key-clearing paths call the wrappers and that only the wrappers call the real functions. The firmware built with `make CHIP=bl616 BOARD=bl616dk` as `750bb0a-dirty.2c19591`, 660000 bytes, MD5 `6b553c5a34e6057caa96f648d969aacf`, was installed with `tools/tinytang_flash.py --yes` and confirmed by `platform` after the user's power cycle. A traced session with the controller alone, 374 packets fetched in ranges and decoded with btmon, held four encrypted sessions; every pairing logged `resolving list: 78:86:2E:BB:45:79 is public, not added`, no resolving-list, privacy-mode or address-resolution command reached the controller, and the controller still stopped sending data as pairing completed, with the same supervision timeout and error 2 on the next connection. Address resolution is therefore ruled out. The agent's first count of those commands also matched `LE Add Device To Accept List`, the reconnect allow-list, and was corrected before it was reported as a result. BLE-017 now lists what still differs from BlueZ at the moment of failure: an ATT MTU exchange as the first operation after encryption, which this board never makes, data length extension, and the interval and supervision timeout. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 15 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

Copy BlueZ's first move after encryption: send an ATT MTU exchange when the link reaches encryption and start discovery from its completion, keeping the resolving-list wrap and the trace, and trace one more pairing. Entry 5's exchange was sent during the security procedure and failed for that reason; this one is not. If the controller still goes silent, data length extension is the next difference.

#### Files Modified:

- CMakeLists.txt
- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 16 COMMIT Unreleased 2026-10-11T09:51:05-07:00

#### Coming From:

Unreleased 2a3e59b

#### Purpose:

Copy BlueZ's first operation after encryption, an ATT MTU exchange, to see whether the Xbox Wireless Controller waits for it before treating the host as set up.

#### Outcome:

`ports/bl616/tang_ble.c` now sends an ATT MTU exchange as soon as the link is encrypted and starts discovery from its completion, logging both; entry 5's exchange had gone out during the security procedure and drawn ATT 0x0E, which this one avoids. The firmware built with `make CHIP=bl616 BOARD=bl616dk` as `2a3e59b-dirty.e957079`, 660656 bytes, MD5 `23467e12adaed6d8676f93eea93d35e7`, was installed with `tools/tinytang_flash.py --yes` and confirmed by `platform` after the user's power cycle. In a traced session with the controller alone, 371 packets, four encrypted sessions each logged `mtu exchange done (err 0), mtu 23`, the same MTU BlueZ got, and the controller still stopped sending data as pairing completed, so the MTU exchange is ruled out. The trace makes the controller's last act precise: in every encrypted session its last two data packets are the GATT answer to the board's first request and an L2CAP Connection Parameter Update Request about 0.1 s after pairing completes, after which only the link layer stays up, for 4 to 5 s. The controller never sent that request to BlueZ, which moved it to 15 ms on its own about 2 s in, so it treats this board's link differently before any GATT difference arises; what still differs at the link is BlueZ's 45 ms interval and 420 ms supervision timeout at connection against this board's 50 ms and 4 s, and BlueZ's 251-octet data length, which this board never sets because `CONFIG_BT_DATA_LEN_UPDATE` is off in this build. BLE-017 records this. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 16 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

Make this board's link match BlueZ's: create the gamepad slot's connection at 45 ms with a 420 ms supervision timeout, and set the data length to 251 octets with `bt_le_set_data_len()` once connected. The two are changed together because each test costs the user a pairing session; if the controller then keeps talking, a following cycle separates them.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 17 COMMIT Unreleased 2026-10-11T09:56:41-07:00

#### Coming From:

Unreleased ba80bbf

#### Purpose:

Make this board's link to the Xbox Wireless Controller match BlueZ's, a 45 ms interval with a 420 ms supervision timeout and a 251-octet data length, since the controller asks this board for new parameters, never BlueZ, before falling silent.

#### Outcome:

`ports/bl616/tang_ble.c` now creates both the explicit connection and the whitelist reconnect with interval 0x24 and supervision timeout 42, BlueZ's values, and sends `bt_le_set_data_len(conn, 251, 2120)` on every connection; `bt_le_set_data_len()` is declared locally because `host/hci_core.h` is not on this port's include path, and the SDK's automatic data-length update is off in this build. Two separate variables are changed in one cycle because each test costs the user a pairing session. The firmware built with `make CHIP=bl616 BOARD=bl616dk` as `ba80bbf-dirty.8977791`, 661008 bytes, MD5 `2facf1a27ed6de689f9efc2938c84a5f`, was installed with `tools/tinytang_flash.py --yes` and confirmed by `platform` after the user's power cycle. In a traced session with the controller alone, 530 packets, every connection came up at interval 36 and timeout 42, and the controller still asked for interval 6-12 and fell silent as pairing completed, in three encrypted sessions, so BlueZ's link parameters are ruled out. The controller accepted all 40 `LE Set Data Length` commands, but no `LE Data Length Change` event ever arrived, where BlueZ's adapter reported 251 octets each way, so whether this board's controller completes the exchange is unknown; that and the frequent `0x3E` connects, which BlueZ never had, both point below the host. BLE-017 records this. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 17 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

The one remaining host-side difference from BlueZ is the pairing: legacy pairing here, where this board sends its own keys, against BlueZ's LE Secure Connections, where the host sends none. With everything else now matched, setting the pairing request to BlueZ's flags (`smp_set_auth(0x2d)`, Bonding, MITM, Secure Connections, CT2; still Just Works against the controller's NoInputNoOutput) makes this board's host traffic a replica of BlueZ's. If that also fails, the fault lies in the BL616's binary controller, and the choice between stopping, seeking a newer controller library, or using another radio for the controller is the user's.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 18 COMMIT Unreleased 2026-10-11T10:03:53-07:00

#### Coming From:

Unreleased b6a8490

#### Purpose:

Replicate BlueZ's pairing request to the Xbox Wireless Controller exactly, the last host-side difference left, so that a failure would place the fault below the host.

#### Outcome:

`ports/bl616/tang_ble.c` now passes `smp_set_auth()` BlueZ's flags, 0x2D (Bonding, MITM, Secure Connections, CT2), in place of legacy pairing, with privacy off, the resolving-list wrap, the MTU exchange, the 45 ms and 420 ms link and the data length request all kept. The firmware built with `make CHIP=bl616 BOARD=bl616dk` as `b6a8490-dirty.f783aa8`, 661008 bytes, MD5 `8031b0894f192083eb8cf1aa98c007e9`; the first install found no board on USB, neither the CDC nor the FT2232, and after the user's power cycle the board came back on the previous image with no crash record and a power-up as its last reset, so the board had lost power or its USB link rather than failed, and the second install with `tools/tinytang_flash.py --yes` succeeded and `platform` confirmed it after a power cycle. In a traced session with the controller alone, 393 packets, every encrypted session went exactly as BlueZ's had through the moment of failure: LE Secure Connections Just Works, encryption, this board's MTU Exchange Request, the controller's Identity Information and Identity Address Information, and no keys from the host. BlueZ's request was answered within 90 ms and the reports started; this board's was acknowledged at the link layer and never answered, the exchange ending with ATT error 14 when the link timed out, about 4.8 s of link-layer life after the last data. With the host's traffic now matching BlueZ's, the fault is placed below the HCI, in the BL616's binary controller library, which also fits the frequent `0x3E` connects that BlueZ's adapter never had and the data length change that is never reported. A check of upstream found that the SDK in use is nand2mario's fork at `7f44f9e` (2025-04-27), while `bouffalolab/bouffalo_sdk` has released `components/wireless/bluetooth/btblecontroller` about twenty times since, up to version 2.3.36 on 2026-10-03, without release notes saying what changed; the user was told, as `.ai/core.md` requires for a newer source. BLE-017 records all of this. The Logitech devices stay unpaired and the board keeps the diagnostic image until the user chooses a direction. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 18 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

The host-side search is exhausted, and the next step is the user's choice between stopping and documenting, trying a newer upstream controller library, which means first finding out whether the newer `libbtblecontroller_bl616_ble1m10s1bredr0.a` can be linked against this fork's host stack or needs the newer host as well, and getting a link-layer view with an over-the-air sniffer to see what this board's controller sends that BlueZ's adapter does not. Whatever is chosen, the diagnostic changes in `proj.conf`, `CMakeLists.txt` and `tang_ble.c` should be reviewed before the M750 and K950 are paired again.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 19 COMMIT Unreleased 2026-10-11T10:15:38-07:00

#### Coming From:

Unreleased 32299f4

#### Purpose:

Try upstream Bouffalo's newest BLE controller library in place of the SDK fork's, at the user's choice, since the Xbox Wireless Controller's failure had been placed in the controller.

#### Outcome:

Upstream `bouffalolab/bouffalo_sdk` at `8a3df34` (release 2.3.36) was read for `libbtblecontroller_bl616_ble1m10s1bredr0.a` (1,856,570 bytes, sha256 `cf8b688b694bb96225d736c412350ea3c737f7a872bb755bb37ef267a2e64e23`, version string `lib_version_btblecontroller_1.6.210`), its headers and its host glue. Against the fork's library it keeps `btble_controller_init` and the rest of the controller API and the onchip HCI packet enum and structures unchanged, renames `bt_onchiphci_hanlde_rx_acl` to `bt_onchiphci_handle_rx_acl`, and comes with host glue that sends every command with `dest_id` 0 where the fork's glue passes a connection handle for some, so a new `ports/bl616/tang_btble_compat.c` forwarded the old name and a link-time wrap of `bt_onchiphci_send` zeroed `dest_id` for commands, leaving the SDK's sources untouched; its other new needs (`arch_delay_ms`, `mfg_media_read_macaddr_with_lock`, `vprintf`) were already in the build. With the library copied over the SDK cache's own (a git checkout, so recoverable), the firmware linked cleanly as `32299f4-dirty.b71462e`, 663488 bytes, MD5 `0dc5b705f3ab4c72ae450662e3c7d83b`, and was installed and confirmed by `platform`, but the first command to start the radio, `blescan 3`, took the board off USB entirely, neither the CDC nor the FT2232, and after the user's power cycle `crash` held no record, so the board hung rather than faulted or reset. Upstream has also changed the BL616 PHY library three times and the BL616 linker scripts that size the controller's exchange memory four times since the fork, so the controller cannot be taken alone; using it means taking upstream's radio layer together, which the user had been told would come back to them as a larger job. `/ble/bonds.bin` was deleted so that no saved pairing could start the radio at boot on that image, the original library was restored with `git checkout` in the SDK cache (sha256 `d20d1b68ddea5cd83c5ba5e5edcb4b2c62fa6bf0ddcf5538c024d0408cfd3890`), the compatibility file and its wrap were removed, and the entry-18 code was rebuilt as `32299f4-dirty.17f81b2` and installed, awaiting the user's power cycle. The SDK cache already carried a modified `cmake/compiler_flags.cmake` and an untracked `tools/bflb_tools/.../img_create/` that were not made in this session and were left alone. BLE-017 records the result. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 19 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

The user chooses between porting upstream's radio layer (controller, PHY and RF libraries, the controller's memory layout in the linker script, and the host glue, which approaches moving the whole Bluetooth stack to the current upstream SDK and puts the working Logitech support at risk while it is done), getting a link-layer view with an over-the-air sniffer, and stopping with the defect documented. In every case the diagnostic changes still in the firmware should be reviewed before the M750 and K950 are paired again.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 20 COMMIT Unreleased 2026-10-11T10:21:34-07:00

#### Coming From:

Unreleased c441eaf

#### Purpose:

Size the move to upstream Bouffalo SDK 2.3.36, which the user chose to bring its newer radio layer and other fixes, by building TinyTang against it unchanged before touching the board.

#### Outcome:

The SDK in use was found to be a single shallow commit of nand2mario's fork, which descends from CherryUSB's fork of an older Bouffalo base (about 2.0, 2024) carrying CherryUSB 1.4.2 (January 2025), a USB Xbox 360 host driver and nand2mario's C++ build fixes, so the gap to upstream spans the whole SDK and its Bluetooth layer predates April 2025. Upstream `bouffalolab/bouffalo_sdk` was fetched shallow at `8a3df34cb73f459ac71deb1bdf0b33fa016ab6fb` (2.3.36, 1.6 GB) into `~/.cache/tangcore-dev/sdk-2.3.36`, leaving `~/.cache/tangcore-dev/sdk` untouched as the rollback, selected through the Makefile's `BL_SDK_BASE`. Upstream still builds with the Xuantie GCC 10.2.0 toolchain V2.6.1 already installed, and includes CherryUSB, but configures projects through Kconfig and a `defconfig` instead of `proj.conf`'s CMake settings, so the first build compiled with nothing enabled; a `defconfig` translating `proj.conf` was written, every option mapping by name except `BFLOG` (now `BFLB_LOG`) and the newlib switches, of which upstream keeps only the file ports. Built with `make -k`, the errors fall into five areas: the USB console and role switching, where CherryUSB's API now takes a bus id (about twenty calls in `usb_cdc_bl616.c` and `tang_usb_role.c`); the heap, where the allocator header `mem.h` is gone (`tang_heap.c`, `td_desktop_bl616.c`); the Bluetooth host's include layout (`uuid.h` in `tang_ble.c`); a C++ brace-initialisation error in `phosphor_player.cpp`; and two SDK-side configuration gaps, the Bluetooth CLI wanting the shell component and the SD card port a BSP header. More errors are expected behind these at link time. The largest risk is not the compile: TinyTang is reflashed only through its own USB console, so a new-SDK image that passes the boot-header checks and hangs before USB comes up would leave no console to reflash through, the board has no BOOT button, one-wire mode runs Sipeed's FT2232 firmware, and FLS-004's Dev Cube restore needs the BL616's ROM bootloader, which no record says how to reach on this board. The build directory was rebuilt against the current SDK, identity `c441eaf-dirty.3254be5`, matching the code on the board; the board was not touched. `defconfig` is committed; the current SDK does not read it. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 20 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

Before Phase 1, the recovery path has to be settled with the user: either a confirmed way into the BL616's ROM bootloader for a Dev Cube or blflash restore, or a way to make a new-SDK image unable to strand the board. Phase 1 then ports the five areas, builds, and boots with Bluetooth unused, checking the console, flashing, the SD card, the desktop, core loading and Phosphor.

#### Files Modified:

- defconfig

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 21 COMMIT Unreleased 2026-10-11T10:27:59-07:00

#### Coming From:

Unreleased 7a57868

#### Purpose:

Hand the move to upstream Bouffalo SDK 2.3.36 to the next agent at the user's request, part way through Phase 1, with the work done so far, the state of the board and the plan.

#### Outcome:

The move exists because the Xbox Wireless Controller's failure was placed in the BL616's binary BLE controller (entry 18) and upstream's newer controller could not be taken alone (entry 19); the user chose to move the whole SDK, expecting other fixes with it. The user approved this plan: Phase 0, sizing, done in entry 20; Phase 1, port and boot with Bluetooth unused, checking the USB console, flashing, the SD card, the desktop, core loading and Phosphor; Phase 2, Bluetooth on the M750 as a regression baseline; Phase 3, the controller, traced with `bletrace`; Phase 4, removing the diagnostic changes and updating TOOL-003, TOOL-016, the BLE records and `THIRD_PARTY.md`. Each phase is its own cycle, and a finding that changes the plan goes back to the user. The upstream SDK is at `~/.cache/tangcore-dev/sdk-2.3.36` (`8a3df34cb73f459ac71deb1bdf0b33fa016ab6fb`, shallow) and the old fork stays at `~/.cache/tangcore-dev/sdk` as the rollback; a build against the new one is `make CHIP=bl616 BOARD=bl616dk BL_SDK_BASE=$HOME/.cache/tangcore-dev/sdk-2.3.36`, best with `-k` to see every error, and the Makefile's default is to move to it only once Phase 1 boots. The new SDK reads `defconfig`, committed in entry 20, and ignores `proj.conf`; the old reads `proj.conf` and ignores `defconfig`. Before Phase 1 the user stated that the BL616 has a BOOT button inside the case and that firmware used to be written through its ROM bootloader, and declined a recovery test as unnecessary; BRD-009 now records that path, with the rule that a recovery writes only the application at 0x40000 with BLFlashCommand and never runs `make flash`, which would overwrite Sipeed's loader at 0x0. The agent had wrongly told the user that the reference says the board has no BOOT button; it says nothing either way, and BRD-009 fills that gap. Phase 1 has ported three things, none built yet. `usb_config.h` is replaced: the old file was a copy of CherryUSB v1.0.0's template, and the new one includes upstream's `cherryusb_config_template.h` and, as upstream's own BL616 USB console does, silences CherryUSB's prints and sets `CONFIG_USB_HS`. `ports/bl616/usb_cdc_bl616.c` uses CherryUSB's bus-id API on bus 0 with register base 0, descriptor callbacks with C strings in place of the raw descriptor array (VID, PID and the three strings unchanged, plus a device-qualifier descriptor for high speed), endpoint callbacks taking the bus id, and an event handler passed to `usbd_initialize()`. `ports/bl616/tang_usb_role.c` calls `usbd_deinitialize(0)`, `usbh_initialize(0, 0, NULL)` and `usbh_deinitialize(0)`; its header comments describe the old CherryUSB's re-initialisation behaviour (USB-004, USB-005) and must be re-checked against the new one. Still to port, from entry 20's list: the heap, where `mem.h` is replaced by `components/mm/mm.h` with a TLSF or heap5 backend, `kfree_size()` now taking a heap id, `kmax_free_size()` and `kmin_free_size()` added, and allocations that return NULL on failure rather than spinning, so `ports/bl616/tang_heap.c`'s wraps of `bflb_malloc` (BL6-008, BL6-009) may no longer be needed and must be re-derived from `mm.c` rather than carried over, with `td_desktop_bl616.c` and `tang_ble.c` also reading the heap; the Bluetooth host's include layout (`uuid.h` in `tang_ble.c`) and every host-internal call `tang_ble.c` makes (`smp_set_auth`, `bt_le_set_data_len`, `bt_keys`, the link-time wraps of `bt_id_add`, `bt_id_del` and `bt_monitor_send`); the C++ brace-initialisation error in `ports/bl616/phosphor/phosphor_player.cpp`; and two `defconfig` gaps, the Bluetooth CLI wanting the shell component and the SD card port wanting `sdh_sd.h`; link errors are expected after these. Risks for the first boot are USB enumeration (TOOL-001: with the old SDK newlib stopped it, and upstream keeps no newlib switch), whether upstream's image and flash layout still suit Sipeed's loader and FLS-002's slot, the crash recorder's wraps of `exception_entry` and `platform_reset`, and the UART fast path (BL6-007). The board runs `32299f4-dirty.17f81b2`, the entry-18 diagnostic image on the old SDK, with no pairings saved, so Bluetooth stays off at boot; that image still carries BlueZ's pairing flags, privacy off, the resolving-list wrap, the MTU exchange, BlueZ's 45 ms and 420 ms link, the data length request and the HCI trace, and the M750 and K950 are unpaired until Phase 4. When Bluetooth is tested, every `tinytang_run.py` output must be saved whole before it is filtered, because the board's Bluetooth log hands each line out once, and a long `bletrace dump` is fetched in ranges because one command's output is cut after about 4.5 KB (TOOL-006). This commit carries the three unbuilt Phase 1 files, so `main` builds against neither SDK until Phase 1 finishes; `7a57868` is the last commit that builds against the old SDK. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 21 of the active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

Continue Phase 1 from the remaining ports above, in this order: the heap, re-deriving `tang_heap.c` from upstream's `components/mm`; the Bluetooth includes and host-internal calls; the C++ error; the two `defconfig` gaps; then link errors. Build with `-k` against `sdk-2.3.36` after each. When it builds, compare the image's boot header and flash layout with FLS-002 before installing it with `tools/tinytang_flash.py --yes`, then have the user power-cycle and check the console, flashing, the SD card, the desktop, core loading and Phosphor. If the board does not come back, recover through BRD-009, writing only the application at 0x40000.

#### Files Modified:

- usb_config.h
- ports/bl616/usb_cdc_bl616.c
- ports/bl616/tang_usb_role.c

#### Status:

- Build: NOT RUN
- Deployment: NOT RUN
- User Test: NOT RUN

---
