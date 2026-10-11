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
