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

## 22 COMMIT Unreleased 2026-10-11T11:21:24-07:00

#### Coming From:

Unreleased 8cddc54

#### Purpose:

Record the user's cancellation of the SDK migration and restoration of the exact firmware running before that trial.

#### Outcome:

The SDK 2.3.36 trial built and flashed, but normal USB enumeration failed; omitting the early SD mount also failed. A full flash read confirmed the installed application matched its build and that the loader and vendor data were unchanged. The first persistent diagnostic recorded main, successful board and USB initialization, successful shell-task creation and entry to the scheduler, but no running-shell checkpoint. Source inspection found that the new FreeRTOS exception path passes masked mcause instead of the register-frame pointer expected by the existing crash recorder; a local guard and more scheduler checkpoints were built, but the later run left an erased evidence sector and did not establish the boot cause. The user stopped the work and requested restoration. The exact original image, `32299f4-dirty.17f81b2`, was located in the prior agent's saved artifacts and copied to `/run/media/vash/GIT/Tang-Console-138K-backups/tinytang_bl616_32299f4-dirty.17f81b2.bin`; its length is 661008 bytes and SHA-256 is `f8cb34557b0da70595954d756d833fb47c7c5d83637b0f22d954c1309fa137c2`. Its boot-header CRC and length were validated and BLFlashCommand wrote only the application at 0x40000, with the device's SHA-256 matching the original image. An initial connection showed the Sipeed debugger; the user then explained the power cable was unplugged during cable swapping. After reconnecting, USB enumerated as TinyTang CDC `ffff:5454` and a guarded `platform` command returned the exact restored identity and a shell prompt. VGA was unavailable with the board disassembled, so display acceptance was not tested. Migration and diagnostic source changes remain local and uncommitted; this commit records only the recovery. The core-syntax audit re-read core.md and core-syntax.md, reviewed the complete .ai diff, confirmed core.md and settled entries unchanged, and checked the canonical sections, numbering, statuses and active-log capacity.

#### Next Steps:

Keep the restored firmware installed and leave the migration stopped unless the user explicitly requests further work. The recovered USB console and identity are confirmed; do not request another BOOT cycle to repeat those checks. Local source still contains the uncommitted migration experiment and HEAD retains entry 21's partially ported source, so neither is a qualified old-SDK build baseline; the saved original binary is the verified restore artifact.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: NOT RUN

---

## 23 COMMIT Unreleased 2026-10-11T11:44:48-07:00

#### Coming From:

Unreleased b61d730

#### Purpose:

Record restoration of the original working firmware after the user stopped the resumed SDK migration trial.

#### Outcome:

After entry 22 the user authorized another attempt at the new Bluetooth stack. Local startup-recovery code was added to expose a USB console before the scheduler, save the previous crash sector, upload and verify an application into the existing staging slot, and commit it through the existing TCM copier; SD mounting was moved into the shell task and a boot watchdog was added for the scheduler attempt. Host tests exercised the actual C recovery and update code with the Python client, passing a complete update, write failure, invalid header, bad CRC, refusal to commit an unverified image and preservation of loader/vendor regions. The new-SDK image `b61d730-dirty.9986a06`, 611472 bytes, SHA-256 `d7f8de8778932918ac2f331a251551ecd2ac06fc739c89c140dd8dd61876096e`, was installed through the working old USB console, but its first hardware boot returned to the Sipeed loader instead of exposing recovery. The user confirmed the power LED on and both cables connected. Source review found that the SDK bare-metal trap vector's IRQ branch still calls default_interrupt_handler, which the FreeRTOS build aliases to freertos_risc_v_trap_handler; enabling USB interrupts before task creation therefore enters a task-dependent handler with no task. This recovery defect was not corrected or redeployed, and the original scheduler failure remains unresolved. The user stopped the work and requested restoration through ROM BOOT mode. BLFlashCommand wrote only the application at 0x40000 from `/run/media/vash/GIT/Tang-Console-138K-backups/tinytang_bl616_32299f4-dirty.17f81b2.bin`, 661008 bytes, and its device SHA-256 matched `f8cb34557b0da70595954d756d833fb47c7c5d83637b0f22d954c1309fa137c2`. After the user's power cycle, USB enumerated as TinyTang CDC and a guarded platform command returned `32299f4-dirty.17f81b2` and the shell prompt. Display acceptance was not tested. All experimental engineering changes and test files remain local and uncommitted; this commit records only the restoration. The core-syntax audit re-read core.md and core-syntax.md, inspected the complete .ai diff, confirmed core.md and settled entries unchanged, and validated the canonical entry structure, statuses, sequential numbering and capacity.

#### Next Steps:

Keep the original firmware installed and do not resume the migration without a new explicit user request. The USB console and exact restored identity are confirmed. The local source and build products still contain the failed migration and recovery experiment and must not be flashed as a working baseline; use the saved original binary for restoration.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: NOT RUN

---

## 24 COMMIT Unreleased 2026-10-11T12:18:22-07:00

#### Coming From:

Unreleased 02e68f4

#### Purpose:

Port TinyTang to upstream Bouffalo SDK 2.3.36 so the firmware links against the newer Bluetooth stack, prove the board's ROM-bootloader recovery, and take the new image to its first hardware boot.

#### Outcome:

The port links, the recovery path is proven, and the new image does not boot yet. The tree was first restored to the code the board runs, `32299f4`, with the `.ai` memory kept and the previous agent's uncommitted migration and startup-recovery experiment set aside verbatim in `/run/media/vash/GIT/Tang-Console-138K-backups/tinytang-preamigration-20261011T115054-0700.tar.gz` and its `.diff`. The port then re-derived the areas entry 20 named. `defconfig` carries the new SDK's Kconfig choices, mapping `proj.conf` by name (`CONFIG_BFLB_LOG` for `BFLOG`) and adding `CONFIG_CHERRYUSB_DEVICE_CDC_ACM` (the ACM class's own gate, not the base `CONFIG_CHERRYUSB_DEVICE_CDC`), `CONFIG_BSP_SDH_SD` (the SD driver's include directory is gated on that and not on the `BSP_SDH_SDCARD` legacy alias), `CONFIG_BT_STACK_CLI=n` (the stack's CLI wants the SDK shell, and this firmware's console is TinyDesk Shell) and `CONFIG_MM_ENABLE_MIN_FREE_TRACKING`. The heap was re-derived rather than carried: `components/mm` replaces the old TLSF, `tang_heap.c` now wraps `kmalloc` and `krealloc` only to record refused requests, since upstream already returns NULL where the old `bflb_malloc` spun with interrupts off (BL6-008), and `tang_heap_info` reports through `kfree_size(0)`, `kmax_free_size(0)` and `g_mem_manager`, taking the mm lock only for the heap-list walk so the SDK's own locking getters are not called inside it; `td_desktop_bl616.c` and `tang_ble.c` follow, the latter also moving `uuid.h` to `bt_uuid.h`. `CMakeLists.txt` splits the language standard into `-std=gnu11` for C and `-std=gnu++17` for C++, which is what the SDK's `phosphor_player.cpp` needs, and renames the allocator wraps to `--wrap=kmalloc` and `--wrap=krealloc`, and `FreeRTOSConfig.h` guards the MTIME addresses that upstream's FreeRTOS target now supplies. It built clean with `make -k CHIP=bl616 BOARD=bl616dk BL_SDK_BASE=$HOME/.cache/tangcore-dev/sdk-2.3.36` as identity `02e68f4-dirty.4fb2d07`, 607,312 bytes, valid boot header, the only warnings the vendored programmer's and the SDK's own. The recovery path was then proven without a write: with BOOT held at power-up the BL616 presents its own USB CDC `349b:6160 Bouffalo CDC DEMO` on `/dev/ttyACM0`, not the FT2232 and needing no `ttyUSB` or sudo, `BLFlashCommand` handshook, and a `--read` of 4096 bytes at `0x40000` came back byte-identical to the saved image's first 4 KiB (SHA-256 `dc438973ea671c4a54eb9f82257bae7a32a3a65787061adeef116179921c6fdc`), recorded as `rom_read_20261011_0x40000_len0x1000.bin`; the chip UID is `e8ca50f49d69` and the flash `c86016`, 4 MiB. The new image was then installed with `tools/tinytang_flash.py --yes`, staged and commit-verified, but the board did not run it: after a cold power cycle in two-wire mode it came up as the SIPEED `USB_Debugger` FT2232 (`0403:6010`, serial `2025041420`) with no CDC, where the same cabling had shown `ffff:5454` before the flash, the same signature as entries 21 to 23. It was recovered through the ROM bootloader with `BLFlashCommand --write` of the saved image at `0x40000`, whose device SHA-256 matched `f8cb34557b0da70595954d756d833fb47c7c5d83637b0f22d954c1309fa137c2`, and after a power cycle `platform` reported `32299f4-dirty.17f81b2` again, which also confirms the two-wire setup itself is sound and isolates the new image as the fault. The required core-syntax audit re-read `core.md` and `core-syntax.md`, inspected the complete `.ai` diff, confirmed `core.md` unchanged and no settled entry rewritten, and validated this entry as number 24 with six canonical sections, prose, allowed Status values and a count within the 100-entry limit.

#### Next Steps:

Diagnose why the new-SDK image does not come up, keeping the flash-then-recover loop this cycle proved cheap. Build the smallest new-SDK image first with `TINYTANG_USB_ONLY=1` (TOOL-005): if that enumerates as `ffff:5454`, USB bring-up is sound and the fault is in what runs above it, so add the shell and then storage back one at a time; if it does not, the bring-up itself is at fault, and the prime suspect is entry 23's finding that the new SDK routes IRQs to the FreeRTOS trap handler, so a USB interrupt that fires before `vTaskStartScheduler()` enters a task-dependent handler with no task and hangs, which can be tested by starting the USB but enabling its interrupt only once the scheduler runs. Whichever it is, the diagnostic changes belong in `defconfig` and `CMakeLists.txt` and must be removed before Phase 4. Separately, `tools/tinytang_flash.py`'s closing hint still tells the user to reflash with `make flash`, which BRD-009 forbids, because the file was restored to `32299f4` and lost the BRD-009 wording; restore it in the cycle that next touches the file. The Bluetooth regression of Phase 2 and the controller of Phase 3 wait on a booting image.

#### Files Modified:

- CMakeLists.txt
- FreeRTOSConfig.h
- defconfig
- ports/bl616/tang_ble.c
- ports/bl616/tang_heap.c
- ports/bl616/tang_heap.h
- ports/bl616/td_desktop_bl616.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 25 COMMIT Unreleased 2026-10-11T13:33:32-07:00

#### Coming From:

Unreleased 7b12792

#### Purpose:

Take the SDK 2.3.36 image to its first hardware boot under the USB gate, bisect why it never enumerates, and record the root cause before fixing it.

#### Outcome:

The image links, the recovery path works, and the boot failure is now diagnosed to a single mechanism. The full firmware built with `make CHIP=bl616 BOARD=bl616dk BL_SDK_BASE=$HOME/.cache/tangcore-dev/sdk-2.3.36` as `7b12792-dirty.c402c31`, 607,312 bytes, and was flashed with the ROM bootloader; the board never enumerated as `ffff:5454`. Everything around the fault was eliminated first. The loader is not at fault: the image's first 4 KiB, including its embedded flash and clock config, is byte-identical to the working `32299f4` image apart from the body length and header CRC, and the SDK's own device CDC example, refreshed into `ref/` for the new SDK and built by the `TINYTANG_REF` path with this project's `defconfig` and `usb_config.h`, enumerated as `ffff:ffff CherryUSB_CDC_DEMO` on this board, so the SDK, the configuration and the clocks are sound and the fault is in this project's code. The ROM bootloader path was proven and used repeatedly: with BOOT held at power-up the BL616 presents `349b:6160 Bouffalo CDC DEMO` on `/dev/ttyACM0`, `BLFlashCommand` reads and writes flash there, a 4 KiB read at `0x40000` was byte-identical to the saved image, and every write verified by the device's own SHA-256, so no attempt could strand the board. A bisect of the app then narrowed the fault: with the crash recorder present the board lands in the Sipeed loader, and with it removed the app runs far enough to claim the USB port (the FT2232 disappears) but still does not enumerate; removing the link wraps, skipping the SD mount and skipping the shell each left the board still not enumerating. The decisive evidence came from reading the crash recorder's sector `0x3FF000` through the ROM bootloader after the exception-handler fix entry 23 named was applied, so that a fault is recorded instead of faulting again inside the recorder: the record is `kind=TRAP`, `mcause=0x30000005` (load access fault), `task="(no task)"` — that is, before `vTaskStartScheduler` — with `mepc` resolving exactly, against the recorded build's own ELF, first to `xTaskCheckForTimeOut` at `tasks.c:3631` with `mtval=0x18`-family offsets of the null `pxCurrentTCB`, and after `INCLUDE_xTaskAbortDelay` was returned to the SDK's default of 0, to `vTaskPlaceOnEventList` and `vListInsert` instead. The mechanism is therefore a blocking take or receive with a timeout made before the scheduler starts, which has no current task to block and so dereferences a null `pxCurrentTCB`; the working reference build never makes such a call, which is why it enumerates and this firmware does not. Three fixes were applied and kept — the exception-handler masked-mcause guard (entry 23's; it is what made the fault readable), `INCLUDE_xTaskAbortDelay` back to the SDK's default of 0, and the SD mount moved out of `main` into the shell task because `f_mount` takes the FatFS reentrancy mutex with a timeout (TOOL-015) — and the app still faults, so at least one more pre-scheduler blocking call remains, in `tdsh_bl616_console_init` or in the CherryUSB OSAL it reaches. The required core-syntax audit re-read `core.md` and `core-syntax.md`, inspected the complete `.ai` diff, confirmed `core.md` unchanged and no settled entry rewritten, and validated this entry as number 25 with six canonical sections, prose, allowed Status values and a count within the 100-entry limit.

#### Next Steps:

Read `0x3FF000` through the ROM bootloader once more and resolve the new `mepc` and `ra` against this build's ELF to name the remaining pre-scheduler blocking call; if it lands in the CherryUSB OSAL reached from `tdsh_bl616_console_init`, bring the USB console up from a task after `vTaskStartScheduler` rather than in `main`, which removes every pre-scheduler blocking call at once. Once the board enumerates, carry out the Phase 1 checklist that entries 20 and 21 set out — the console, flashing, the SD card, the desktop, core loading and Phosphor — and then Phase 2, Bluetooth on the M750 as a regression baseline. The diagnostic scaffolding added here (the `TINYTANG_NO_CRASH`, `TINYTANG_NO_FS`, `TINYTANG_NO_WRAPS`, `TINYTANG_UART_TRACE` guards, the UART trace in `main.cpp`) is for this diagnosis and should be removed before Phase 4, and `tools/tinytang_flash.py`'s closing hint still names `make flash`, which BRD-009 forbids, because the file was restored to `32299f4`; restore the BRD-009 wording in the cycle that next touches it. The board is left running the failed image and is recovered with `BLFlashCommand --write` of `tinytang_bl616_32299f4-dirty.17f81b2.bin` at `0x40000`.

#### Files Modified:

- CMakeLists.txt
- FreeRTOSConfig.h
- main.cpp
- ports/bl616/tang_crash.c
- ports/bl616/usb_cdc_bl616.c
- ref/cdc_acm_template.c
- ref/ref_rtos_main.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 26 COMMIT Unreleased 2026-10-11T13:42:13-07:00

#### Coming From:

Unreleased bf42518

#### Purpose:

Fix the last boot fault the bisect of entry 25 left, so the SDK 2.3.36 image boots to its USB console on this board.

#### Outcome:

The move to upstream Bouffalo SDK 2.3.36 now boots, and entry 25's diagnosis is confirmed to its last detail. The remaining fault was a watchdog device rename: the SDK's own name macro `BFLB_NAME_WDT` is `"watchdog"` in the forked SDK and `"watchdog0"` in 2.3.36, and upstream's `bflb_device_get_by_name()` calls `LHAL_PARAM_ASSERT(0)` and `bflb_lhal_assert_func` when a name is not found, where the fork only returned NULL, so `wdt_start()` in `ports/bl616/tang_crash.c` looked up a name that no longer existed and the `ebreak` assert killed the boot inside the watchdog task, which is exactly what the crash record showed once entries 23 and 25 had made a fault readable. The lookup now uses `BFLB_NAME_WDT` with `bflb_name.h` included, so it follows the SDK rather than spelling a name that can move again, and the other two lookups in the port, `"gpio"` and `"uart1"`, were checked against the 2.3.36 device table and are unchanged. The image built with `make -k CHIP=bl616 BOARD=bl616dk BL_SDK_BASE=$HOME/.cache/tangcore-dev/sdk-2.3.36` as `bf42518-dirty.475b83d`, 607,312 bytes, was written at `0x40000` through the ROM bootloader and verified, and after a cold power cycle the board enumerated as `ffff:5454 TinyTang CDC` and `platform` answered `bl616/freertos, tinytang bf42518-dirty.475b83d` with the TinyDesk Shell 0.1.5 prompt, and `help` listed the full command set, so the console, the shell, the SD mount that now runs in the shell task and the FreeRTOS scheduler are all live on the new SDK. The user reported that the HDMI screen no longer shows the factory TangCore splash, which is the expected signature of this firmware running: `boot.tdsh` reached `tangload` and replaced the FPGA's stored core with this project's VGA-only desktop core, where the splash had been the factory core showing precisely because the BL616 never booted. The three fixes of this migration are now all in: the crash recorder's masked-mcause guard, the SD mount moved out of `main`, and this device name; the diagnostic scaffolding committed in `bf42518` (the `TINYTANG_NO_*` and `TINYTANG_UART_TRACE` guards, the UART trace in `main.cpp`) is no longer needed and should be removed before Phase 4. The required core-syntax audit re-read `core.md` and `core-syntax.md`, inspected the complete `.ai` diff, confirmed `core.md` unchanged and no settled entry rewritten, and validated this entry as number 26 with six canonical sections, prose, allowed Status values and a count within the 100-entry limit.

#### Next Steps:

Run the rest of the Phase 1 checklist that entries 20 and 21 set out, which this cycle did not cover: the SD card, the desktop, core loading and Phosphor, and then the flashing path. Phase 2 follows with Bluetooth on the M750 as a regression baseline, and Phase 3 the Xbox controller under the newer controller library, which is what the move to 2.3.36 was for. Before Phase 4, remove the diagnostic scaffolding committed in `bf42518` and update TOOL-003, TOOL-016 and the BLE records, and restore the BRD-009 wording in `tools/tinytang_flash.py`, which still names `make flash`. A record is also owed for the two porting hazards this cycle found, that upstream asserts on an unknown `bflb_device_get_by_name()` name and that its FreeRTOS trap handler passes `mcause` rather than a register frame to `exception_entry`, so neither costs a session again.

#### Files Modified:

- ports/bl616/tang_crash.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 27 COMMIT Unreleased 2026-10-11T14:00:22-07:00

#### Coming From:

Unreleased 397fa20

#### Purpose:

Record that the move to Bouffalo SDK 2.3.36 is complete and working, and hand the Bluetooth radio-start hang that blocks the Xbox controller line to the next agent.

#### Outcome:

The migration is done and banked; the Bluetooth line has a new, sharply defined blocker. The board now runs 2.3.36: after entry 26's watchdog-name fix it enumerated as `ffff:5454 TinyTang CDC`, `platform` reported `bf42518-dirty.475b83d` with the TinyDesk Shell 0.1.5 prompt, `help` listed the full command set, `boot.tdsh` ran and `fpga` answered `core 84` (0x54, the desktop core), and the user accepted the VGA display, so the console, the shell, the SD mount, core loading and the display all work on the new SDK, which was the whole point of the move. The three fixes that got it there are the crash recorder's masked-mcause guard (entry 23's finding, which is what made any of the later faults readable), the SD mount moved out of `main` into the shell task so no blocking `f_mount` runs before the scheduler, and the watchdog device name taken from the SDK's own `BFLB_NAME_WDT` because 2.3.36 renamed it from `watchdog` to `watchdog0` and its `bflb_device_get_by_name()` asserts on an unknown name. For the handoff: the migration replaced far more than Bluetooth, so a future agent should not read it as a library swap. CherryUSB went from v1.0.0 to v1.6.1 with the bus-id API (`usbd_initialize(busid, reg_base, cb)`), the FreeRTOS port changed (its trap handler passes masked `mcause` in `a0`, and `xTaskCheckForTimeOut` dereferences `pxCurrentTCB` under `INCLUDE_xTaskAbortDelay`), and the lhal/BSP changed (device names, and the assert on an unknown name), while the point of the exercise was only the newer controller library. The Bluetooth line was then taken first because the user wants the Xbox controller. `ble` reports the stack reachable and the heap healthy on the new image (`79604 bytes free of 121720`, largest block `29504`) and the board stays up, so the ported `tang_ble.c` and the `mm`-based heap reporting both work; but **starting the radio takes the board off USB entirely** — no CDC, no FT2232, no reset, no crash record — twice, on `blekbd pair Xbox` and on `blescan 3`. That kills every Bluetooth device, so the Logitech keyboard and mouse cannot connect either, and it is not the entry-19 mismatched-library failure: `libbtblecontroller_bl616_ble1m10s1bredr0.a` is shipped by 2.3.36 and is what this build linked. A direct console trace was added to `ble_start_ex` in `ports/bl616/tang_ble.c`, bypassing `ble_say`, which buffers into the drained-once ring when it runs on the ble task, but not one `ble: start:` line reached the host, so the hang is at or before the radio start or the USB dies before the first flush. The recovery path is proven and cheap: with BOOT held at power-up the BL616 presents `349b:6160` on `/dev/ttyACM0` and `BLFlashCommand` reads and writes flash there, and because the firmware now boots, ordinary reflashes go over the console with `tools/tinytang_flash.py` without opening the case; the board is left needing a power cycle after the last radio-start attempt. The required core-syntax audit re-read `core.md` and `core-syntax.md`, inspected the complete `.ai` diff, confirmed `core.md` unchanged and no settled entry rewritten, and validated this entry as number 27 with six canonical sections, prose, allowed Status values and a count within the 100-entry limit.

#### Next Steps:

Find where the radio start dies, and do it with an instrument that survives the hang: write a breadcrumb to a spare flash sector at each step of `ble_start_ex` in `ports/bl616/tang_ble.c` and read `0x3FF000`-style after the hang through the ROM bootloader, since console output does not survive it — the last breadcrumb names the step, exactly as the crash record named the boot fault. Prime suspects, in order: the RF and PHY parameters (`rfparam_init`), which are where the newer controller and the board's own radio meet; an interrupt or trap interaction the way the boot had one; and the `hci_driver_init` and `bt_enable` host handshake. Once the radio starts, the Xbox controller is the next step and BLE-017 applies to the newer controller library, whose behaviour was never observable before because the radio could not run; the operating notes there still hold — the pad must be awake and in pairing mode, the first pairing attempt commonly fails with error 8 and HCI 0x3E and the retry is the one that encrypts, every command's console output must be saved whole because one command's output is cut after about 4.5 KB, and `blekbd off` clears a wedged slot. The rest of the Phase 1 checklist follows the radio work — the SD card, `desktop`, core loading beyond the boot script, Phosphor and the flashing path — as does the Phase 2 regression on the M750, which is blocked by the same radio-start fault. Three housekeeping items are owed before Phase 4: remove the diagnostic scaffolding committed in `bf42518` (the `TINYTANG_NO_*` and `TINYTANG_UART_TRACE` guards, the UART trace in `main.cpp`) and the `BLE_STEP` trace added here, restore the BRD-009 wording in `tools/tinytang_flash.py`, and add the two porting-hazard records this migration found, that upstream asserts on an unknown `bflb_device_get_by_name()` name and that its FreeRTOS trap handler passes `mcause` rather than a register frame to `exception_entry`.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 28 COMMIT Unreleased 2026-10-11T15:35:55-07:00

#### Coming From:

Unreleased 47d2622

#### Purpose:

Close the connection-reference underflow that reset the board on every Xbox Wireless Controller pairing attempt, so Bluetooth work could continue on Bouffalo SDK 2.3.36.

#### Outcome:

The board runs Bouffalo SDK 2.3.36 with Bluetooth working, the crash that blocked it is fixed, and the Xbox Wireless Controller still does not work. The radio start was found to take the BL616's USB device down while the CPU kept running, which the flash-breadcrumb and heartbeat diagnostic proved by showing `ble_start_ex` completing all its steps while the crash recorder's watchdog task was still being fed: `ports/bl616/usb_cdc_bl616.c` gained `tdsh_bl616_console_restart()`, which re-asserts the USB clock with `GLB_Set_USB_CLK_From_WIFIPLL` and re-initialises the device, and `ble_start_ex()` calls it once the radio is up, after which the console re-enumerates as `ffff:5454` with no power cycle, a `blescan` found 11 devices, and the Logitech M750 paired and delivered 218 reports, so the migration did not break the working Logitech support. The controller pairs, encrypts, bonds and then fails exactly as BLE-017 records -- `mtu exchange done (err 14)`, `service discovery failed (-57)`, a supervision timeout and a reconnect loop with error 8 and HCI 0x3E -- so the newer controller library the migration was made for did not fix it, and the SDK's heap cost (about 1 KB more for the stack and 3 KB less total heap than the fork) was measured and accepted before deciding to keep it. Separately, a FreeRTOS assert in `recv_thread` kept resetting the board; the failing address resolved to `BT_ASSERT(old > 0)` in `bt_conn_unref` called from `hci_disconn_complete`, and a link-time wrap of `bt_conn_ref` and `bt_conn_unref` that reads `conn->ref` showed two references and three releases, so this port's `hid_disconnected()` and `hid_connected()`'s error path were releasing connection references that the SDK's own `bt_conn_set_state(BT_CONN_DISCONNECTED)` teardown already drops, which the old SDK's assert-free `bt_conn_unref` tolerated silently. Removing both releases closed it, verified by two pairing attempts, one of them a full pair, encrypt, bond and disconnect cycle, leaving the board up with no crash record where every previous attempt had reset it into the vendor loader. The diagnostic scaffolding added across this cycle -- the flash crumbs, `blestart` and `blecrumb`, the PLL probes, the heartbeat, the reference ring and the `bt_conn_ref`/`bt_conn_unref` wraps -- is still in the tree and is to be removed before the port is finished. The build identity at this commit is `47d2622-dirty.18bd979`, and the required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 28 with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

Promote the USB revive out of the diagnostic code into a clean, commented call and remove the diagnostic scaffolding -- the flash crumbs, `blestart`, `blecrumb`, the PLL probes, the heartbeat, the reference ring and the `bt_conn_ref`/`bt_conn_unref` wraps, together with the `TINYTANG_NO_*` and UART-trace guards committed in `bf42518` -- before the port is finished. The Xbox Wireless Controller is the remaining work and BLE-017 still applies: the cheapest next test is to stop sending the ATT MTU exchange while the pad is still completing pairing, because this build sends it immediately after encryption and the pad answers ATT `0x0E`, after which BLE-017's remaining list applies, with the honest expectation that the fault is in the BL616's binary controller and may only be documentable. The M750's pointer movement in the desktop was never confirmed and should be checked on the image that is kept. `tools/tinytang_flash.py` still names `make flash` in its closing hint, which BRD-009 forbids, and the two hardware facts this cycle established -- that starting the radio takes the USB clock down while the CPU lives, and that `bt_conn_unref` asserts on an over-release -- are owed to `.ai/core-reference.md`.

#### Files Modified:

- CMakeLists.txt
- ports/bl616/tang_ble.c
- ports/bl616/tang_ble.h
- ports/bl616/tang_crash.c
- ports/bl616/tdsh_bl616.h
- ports/bl616/usb_cdc_bl616.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 29 COMMIT Unreleased 2026-10-11T16:31:07-07:00

#### Coming From:

Unreleased 0c53bd6

#### Purpose:

Promote the working USB-clock revive out of the diagnostic scaffolding into a permanent documented fix, and establish how input actually reaches this board.

#### Outcome:

The revive that keeps the console alive across the Bluetooth radio start is now a plain call in `ble_start_ex()` -- `tdsh_bl616_console_restart()`, with a comment saying what it does and why the radio's bring-up takes the USB clock down -- instead of sitting between the PLL crumbs that were only there to investigate it; the three USB-investigation crumbs (`BLE_CRUMB_PLL_BEFORE`, `BLE_CRUMB_PLL_AFTER`, `BLE_CRUMB_USB_REVIVE`), the raw-value helper `ble_crumb_put()` and the now-unused `bl616_glb.h` include are gone, and the rest of the diagnostic scaffolding (the radio-start crumbs, `blestart`, `blecrumb`, the heartbeat, the reference ring and the `bt_conn_ref`/`bt_conn_unref` wraps) is deliberately left for the cleanup cycle. Built against `sdk-2.3.36` as `0c53bd6-dirty.57f0e39` and installed with `tools/tinytang_flash.py --yes`; after the user's power cycle `platform` reported that identity and `ble` reported the radio up with the stack's 16,860 bytes taken, which is the boot path exercising the revive -- the M750's saved pairing starts the radio at boot, taking the USB clock down, and the console is alive because the revive ran. The cycle also settled a question that had been costing the controller work: the BL616 has no route into a core's input at all. `ports/bl616/tang_fpga_link.h` carries commands only for the core ID, the OSD, the desktop layer and cartridge loading (0x01, 0x04, 0x05, 0x08, 0x13, 0x14, 0x15, 0x06, 0x07), while the input it carries runs the other way -- `tang_fpga_joypad()` and `tang_fpga_keyboard()` read what the core already has. A core takes its controller from the FPGA's own USB host on the two USB-A ports (BRD-003), so no BLE device, working or not, could ever have reached a core; the Xbox pad's failure was therefore never on the path to a controller for a core, which this entry records so the work is not resumed as if it were. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 29 with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

The BLE controller line is closed as far as this board allows: the pad is the BL616's binary controller, and the BL616 cannot feed a core, so no further pairing sessions are owed. What remains is the cleanup cycle the diagnostics were always waiting for -- removing the radio-start crumbs, `blestart`, `blecrumb`, the heartbeat, the reference ring and the two `bt_conn_ref`/`bt_conn_unref` wraps, and the `TINYTANG_NO_*` and UART-trace guards committed in `bf42518` -- along with the three records this project is owed: the radio start taking the USB clock down (with `tdsh_bl616_console_restart()` as the fix), `bt_conn_unref` asserting on an over-release where the old SDK was silent, and the OTG port supplying no VBUS, which USB-003 already holds but which the controller work re-learned. `tools/tinytang_flash.py` still names `make flash` in its closing hint, which BRD-009 forbids. The M750's pointer movement in the desktop was never confirmed and is not required.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 30 COMMIT Unreleased 2026-10-11T16:31:28-07:00

#### Coming From:

Unreleased 6edf305

#### Purpose:

Find why the desktop core's HDMI output was dark when nestang's own core drives the same monitor and cable, and fix it.

#### Outcome:

The desktop core now drives HDMI. The fault was localised by building the reference: nestang's *stock, unmodified* `console138k` core, built from the pinned commit `c2450818` with nestang's own `build.tcl` (device revision B, no patches), uploaded to the card and `tangload`ed, drove the user's primary monitor -- it drew the nestang splash and offered `boot castlevania` -- which placed the monitor, the cable and the board's HDMI hardware beyond doubt and confined the fault to this project's core build, since the factory splash had already shown the same port working (DEV-006). The clock monitor then ruled out the clocking: `fmon` read the core's counters as `clk` 21,722,222 Hz and `pix` 74,249,611 Hz, so the 720p pixel clock was running and reaching both probe regions, and the earlier note that an open-flow build carries no HDMI because the chipdb leaves HCLK unreachable was not what this bitstream was doing. The difference is one block of the constraint file: `fpga/desktop/desktop.cst` pinned all three PLLs with `INS_LOC` lines whose own comment recorded that they are "the flattened cell names nextpnr reports", an open-flow-era workaround whose comment also carried the failure verbatim -- `Net hclk5 has no route to the FCLK of u_hdmi.hdmi.serializer.gwSer0` -- while the working reference, nestang's `console.cst`, pins no PLL at all and lets the vendor's place-and-route choose. Removing the three `INS_LOC` lines and letting Gowin place the PLLs, at the board's own revision C, produced a build with `timing PASS: setup +1.957 ns, hold +0.166 ns` and ten clock/analysis rows with zero violations; loaded on the board, the user confirmed HDMI on their primary monitor. One intermediate attempt is worth recording: building at revision B to match nestang's declaration failed outright, because the `INS_LOC` object names do not resolve under that device revision (`CT1135: Can't find object named 'pll_hdmi.PLL_inst'`), which is itself evidence that the pins were the fault and not the revision; `build.tcl` is unchanged from revision C. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 30 with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

Install this build as the card's `/cores/console138k/desktop.bin` so `boot.tdsh` loads the HDMI-capable core at boot, and keep the stock nestang core on the card as the HDMI reference. The HDMI work is then closed: the fix is a constraint-file change, the RTL was never at fault, and no FPGA patch is owed. The cleanup cycle entry 29 named is still owed -- the radio-start crumbs, `blestart`, `blecrumb`, the heartbeat, the reference ring, the two `bt_conn_ref`/`bt_conn_unref` wraps and the `TINYTANG_NO_*` and UART-trace guards -- together with the three records (the radio start taking the USB clock down, `bt_conn_unref` asserting on an over-release, and USB-003's no-VBUS finding, which the controller work re-learned), the BRD-009 wording in `tools/tinytang_flash.py`, and the BLE-017 update that closes the Xbox pad as unsupported on this board. The build tools' `DESKTOP_BUILD_DIR` reconstruction is also worth keeping in mind: it clones from `NESTANG_DIR`, which is an empty submodule in this workspace and had to be cloned from GitHub for this cycle.

#### Files Modified:

- fpga/desktop/desktop.cst

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 31 COMMIT Unreleased 2026-10-11T16:37:16-07:00

#### Coming From:

Unreleased 79b9295

#### Purpose:

Hand the project to the next agent with HDMI working on the board, the card carrying the fixed core, and the remaining cleanup named.

#### Outcome:

The desktop now appears on both displays -- the user reports the console mirrored on the VGA and the HDMI screens together -- and the card carries the fixed core, so this is a handoff rather than a result to re-derive. The state to pick up from is: repository at `79b9295`, working tree clean apart from the carried `third_party/tinydesk` patch dirt, `tools/check_core_log.py` passing at 31 entries, and the board running firmware `0c53bd6-dirty.57f0e39`, which is the BL616 source as committed in entry 29; `/cores/console138k/desktop.bin` on the card is now the HDMI-capable build (4,495,050 bytes, sha256 `95e9b3e1cb884d99e4b6073651c2ba167798a6b92c8869eb499a392ca29549e3`) rebuilt from the committed `desktop.cst`, loaded and confirmed answering core 84, with the previous HDMI-dark build kept beside it as `desktop.bin.pre-hdmi-fix` and the stock nestang core kept as `nestang-stock.bin` for HDMI reference. What is true now, in brief: the board runs upstream Bouffalo SDK 2.3.36 and boots to the shell and TinyDesk on the SD card; cores load from the card and the desktop core now drives HDMI; Bluetooth works -- radio, scan, and both a BLE keyboard and mouse -- after the radio start was found to take the USB clock down and `tdsh_bl616_console_restart()` was made to bring it back, and after the connection-reference underflow that reset the board on every controller attempt was fixed by no longer releasing references the SDK's own teardown drops; the Xbox Wireless Controller remains unsupported, the fault being the BL616's closed controller library, and -- the finding that matters more than the pad itself -- the BL616 has no command in the core link to send input into a core, so a BLE pad could never have driven one. HDMI's fault is worth recording as an agent lesson: it was three `INS_LOC` PLL placement pins in `fpga/desktop/desktop.cst`, named with nextpnr's flattened cell names from the open-flow era, which stopped `hclk5` reaching the HDMI serializer's FCLK; the monitor, cable, port, pixel clock and RTL were all sound, and what found it in one cycle was building stock nestang on the same board as a reference and reading the clock monitor with `fmon`. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 31 with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

The cleanup cycle both entry 29 and entry 30 name is the next work: remove the diagnostic scaffolding -- the radio-start flash crumbs, the `blestart` and `blecrumb` commands, the heartbeat, the reference ring and the two `bt_conn_ref`/`bt_conn_unref` wraps in `ports/bl616/tang_ble.c`, `ports/bl616/tang_ble.h`, `ports/bl616/tang_crash.c` and `CMakeLists.txt`, and the `TINYTANG_NO_*` and UART-trace guards committed in `bf42518` -- then restore the BRD-009 wording in `tools/tinytang_flash.py`, which still names `make flash`. `.ai/core-reference.md` is owed three things this cycle established or re-learned: that starting the radio takes the BL616's USB clock down while the CPU lives, with `tdsh_bl616_console_restart()` as the fix; that 2.3.36's `bt_conn_unref` asserts on an over-release where the fork was silent, so a connection reference must be released only when it was taken; and that HDMI needed nestang's own `console.cst` PLL placement, not the open-flow pins, which belongs with PROT-009's look-ahead note. BLE-017 should be closed with the Xbox pad recorded as unsupported on this board for the reason its own ruled-out list now supports, and the two further consequences noted: BLE-HID gamepads are untested here because none was available, and the BL616 cannot feed a core in any case. Finally, building any core in this workspace needs `NESTANG_DIR` pointed at a real nestang checkout -- the local `../tangcore/nestang` submodule is empty, and this cycle cloned `nand2mario/nestang` to `/tmp/nestang` to build the stock reference and reconstruct the desktop core, which is not a durable location.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 32 COMMIT Unreleased 2026-10-11T16:49:57-07:00

#### Coming From:

Unreleased 6715393

#### Purpose:

Remove the diagnostic scaffolding the migration and the Bluetooth investigation accumulated, restore the BRD-009 recovery wording, record what the cycle established, and close the Bluetooth controller as unsupported.

#### Outcome:

The tree is clean of diagnostics and the project is closed out. Removed: the radio-start flash crumbs and their whole facility, the `blestart` and `blecrumb` commands and their table entries, the heartbeat that the crash recorder's watchdog task called, the `bt_conn_ref`/`bt_conn_unref` reference ring with its link-time wraps in `CMakeLists.txt`, the `BLE_STEP` console trace, the now-unused `bflb_flash.h` include, and the migration-debug guards `TINYTANG_NO_FS`, `TINYTANG_NO_SHELL`, `TINYTANG_NO_CRASH`, `TINYTANG_NO_WRAPS` and `TINYTANG_UART_TRACE` together with `main.cpp`'s UART trace and its `bflb_uart.h`/`bflb_gpio.h` includes -- 436 lines out against 66 in, the latter almost all reference records. The TOOL-001 reference-build switches (`TINYTANG_USB_ONLY`, `TINYTANG_MIN`, `TINYTANG_RTOS`, `TINYTANG_NONEWLIB`, `TINYTANG_REF`) stay, because TOOL-001 documents them and the reference example is still built from them. `tools/tinytang_flash.py`'s closing hint no longer names `make flash`: it now says to hold BOOT at power-up, where the BL616 presents its ROM bootloader as `349b:6160` on `/dev/ttyACM0`, and to write only the application at `0x40000` with `BLFlashCommand --write`, never `make flash`, which BRD-009 forbids. The firmware built clean with `make -k CHIP=bl616 BOARD=bl616dk BL_SDK_BASE=$HOME/.cache/tangcore-dev/sdk-2.3.36` as `6715393-dirty.a597e26`, 607,312 bytes -- the size it was before the diagnostics went in -- and the desktop core was rebuilt clean from the committed sources with `tools/build_desktop_core.sh`, which reported `timing PASS: setup +1.957 ns, hold +0.166 ns` and produced a bitstream whose SHA-256, `95e9b3e1cb884d99e4b6073651c2ba167798a6b92c8869eb499a392ca29549e3`, is identical to the one already on the card and verified on the monitor, so the core build is reproducible from the repository. The cleaned firmware was installed with `tools/tinytang_flash.py --yes`; after the user's power cycle `platform` reported `6715393-dirty.a597e26` and `ble` reported the radio up with the K950 keyboard and the M750 mouse both paired and reconnecting by themselves, so removing the diagnostics cost nothing at runtime. `.ai/core-reference.md` gained three records and closed a fourth: USB-008, that starting the radio takes the BL616's USB device down while the CPU lives and that `tdsh_bl616_console_restart()` brings it back; BLE-018, that 2.3.36's `bt_conn_unref` asserts on an over-release where the fork was silent and that `bt_conn_create_le()` returns a caller reference only for a newly created connection; TOOL-017, that a Gowin build of the desktop core must let the vendor place the PLLs, because the open-flow `INS_LOC` pins leave `hclk5` off the HDMI serializer; and BLE-017, revised in place to closed and unsupported on this board, with the two facts that bound it -- the BL616 has no link command to send input into a core, and no other BLE-HID gamepad was available to test. The index gained three lines, the routing four, and BLE-017's index line was rewritten. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 32 with six canonical sections, prose in Outcome and Next Steps, an allowed Status set, and a count within the 100-entry limit.

#### Next Steps:

No engineering work is pending in this repository. The board runs upstream Bouffalo SDK 2.3.36 with TinyDesk on the SD card, the desktop core driving both the HDMI and the VGA display, cores loading from the card, firmware updating over USB, and a BLE keyboard and mouse paired and reconnecting; the card's `/cores/console138k/desktop.bin` is the HDMI-capable build with the previous one kept beside it as `desktop.bin.pre-hdmi-fix` and stock nestang kept as `nestang-stock.bin`. What is not done is deliberate and recorded: the Xbox Wireless Controller is unsupported (BLE-017), no BLE gamepad path exists from the BL616 into a core (core-log entry 29), and a USB controller on this board would have to be the loaded core's, on the FPGA's own USB host (BRD-003). If any of those is ever wanted, the starting points are BLE-017's ruled-out list, TOOL-017 for anything touching the core build, and USB-008 for anything that starts the radio. Two housekeeping notes: `../tangcore/nestang` is an empty submodule, so a core build needs `NESTANG_DIR` pointed at a real nestang checkout, and the `third_party/tinydesk` submodule carries its usual patch-applied dirt, which is expected and stays.

#### Files Modified:

- CMakeLists.txt
- main.cpp
- ports/bl616/tang_ble.c
- ports/bl616/tang_ble.h
- ports/bl616/tang_crash.c
- tools/tinytang_flash.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 33 COMMIT Unreleased 2026-10-11T17:20:17-07:00

#### Coming From:

Unreleased 378866b

#### Purpose:

Revert the desktop core to its open-flow build from before entry 30's HDMI fix, keeping entry 32's firmware cleanup, because entry 30 got HDMI working by building with Gowin and this project builds with the open flow, as `.ai/core.md` now states.

#### Outcome:

Entry 30 is superseded: its HDMI result came from changing the build to Gowin EDA, and removing the three `INS_LOC` PLL pins fixed only that Gowin build. The user's `378866b` changed `.ai/core.md` to "Open-flow." and dropped Gowin EDA from the FPGA toolchain. `fpga/desktop/desktop.cst` was restored byte for byte to its `6edf305` contents, so `pll_27` is at `PLL_L[1]`, `pll_hdmi` at `PLL_L[3]` and `pll_nes` at `PLL_R[0]` again with the original comment explaining why the open flow needs them. That also discarded an uncommitted edit to the same file, made after entry 32 and before this session, which re-added the pins but cited a `tools/build_desktop_core_open.sh` that does not exist. Nothing else in the tree changed, so entry 32's firmware cleanup, the BRD-009 wording in `tools/tinytang_flash.py` and the board's firmware `6715393-dirty.a597e26` all stand. No build ran this cycle. The open-flow core of record is Tang-Build's seed-17 build, which Tang-Build entry 77 reproduced byte for byte with its `scripts/build-desktop-core.sh` at `NEXTPNR_SEED=17`: `build/clock-monitor/desktop.90d3fa3a84fa.bin` in that repository, sha256 `90d3fa3a84fa8cf80ca2d7d107fde9e00ac15b4362f34bba0d8ddeef53155718`, 4,463,286 bytes. On the card, the Gowin core was renamed from `/cores/console138k/desktop.bin` to `desktop-gowin-hdmi.bin` and kept as an HDMI reference. The seed-17 file was uploaded as the new `desktop.bin` with `tools/tinytang_put.py`, which wrote all 4,463,286 bytes; the same-sized `desktop.bin.pre-hdmi-fix` was not used, because the board has no hash command to identify it. `tangload` loaded the new core, `fpga` reported core 84, and `tangini` read back `core 0x54 (ABI 1.1) has 0xc0 = 0x0230, as declared`, so `boot.tdsh` now loads the open-flow core at every boot. The user reported VGA on and HDMI off, which is the expected pre-fix state and is accepted as this cycle's result. TOOL-017 in `.ai/core-reference.md` was revised in place. It had turned the Gowin result into a rule against the pins, and now states that the project builds with the open flow, that the pins are what that flow needs, that the open-flow core has no clock route from the PLL into the HCLK block so HDMI stays dark, and that the Gowin build is a reference only; its index line and routing row were updated to match. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed in this cycle (its last change is the user's own `378866b`) and that no settled entry was rewritten, checked that the reference's YAML blocks parse, and validated this entry as number 33 with six canonical sections, prose in Outcome and Next Steps, an allowed Status set and a count within the 100-entry limit.

#### Next Steps:

The next work is HDMI alongside VGA on the open flow, and it starts from Tang-Build's record rather than from a fresh theory. Tang-Build entries 73 to 77 found that the open device data has no clock route from the PLL into the HCLK block, so `hclk5` reaches the HDMI serializer only over fabric. They also refuted two modelled pips from the PLL outputs into the HCLK muxes, because the generator re-types a pip source to HCLK and that destroys the PLL, and the forced-DCS bitstream of 2026-10-09 did not boot. A plan should therefore be agreed with the user before any build, and it should first settle where the open-flow build script lives. `tools/build_desktop_core.sh` here still drives Gowin and is not the build of record, while the working open-flow script, its portability patch and the nextpnr fork at `~/tools/nextpnr-mathieufro` all live outside this repository. Several reference records still describe the pre-migration tree and are owed revision. TOOL-003 still names SDK 2.0.0. TOOL-005 still lists `TINYTANG_NO_FS` and `TINYTANG_NO_SHELL`, which entry 32 removed. BL6-003, BL6-008 and BL6-009 still describe the fork's TLSF heap and `bflb_malloc` wraps, which entry 24 replaced with `components/mm` and `kmalloc`/`krealloc` wraps.

#### Files Modified:

- fpga/desktop/desktop.cst

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 34 COMMIT Unreleased 2026-10-11T17:30:37-07:00

#### Coming From:

Unreleased 9d0699a

#### Purpose:

Bring the open-flow desktop core build into this repository from Tang-Build, at the user's direction, so that this project builds its own core with the flow `.ai/core.md` names.

#### Outcome:

The open flow now builds the desktop core from this repository and reproduces the hardware-proven core byte for byte. The new entry point is `tools/build_desktop_core_open.sh`. It reconstructs the design tree with `tools/build_desktop_core.sh --prepare-only` and applies `third_party/patches/desktop/open/0001-open-toolchain-portability.patch`, four behaviour-preserving edits to nestang sources that yosys's `read_slang` front end needs. It then runs `tools/desktop_open/build.sh`. That script performs synthesis (`synth.sh` and `synth.ys`), place and route with the nextpnr-himbaechel fork (`pnr.sh` and `clock_constraints.py`), the setup/hold gate (`pnr_timing.py`), the clock-topology regression gate against `clock_profile.json` (`clock_route_inventory.py`), packing with the apicula fork's `gowin_pack`, `.fs` to `.bin` conversion (`fs_to_bin.py`) and a content-hash stamp. All of these were carried from Tang-Build `737ddb4`, where the flow was developed, with three adaptations. Internal paths were rewritten to this layout. The default seed is now 17, the hardware-proven placement, where Tang-Build defaulted to 23. Tang-Build's static-DCS experiment branch was left out, because its bitstream did not boot on 2026-10-09. SPDX MIT headers were added where the originals lacked them, and `THIRD_PARTY.md` names the new patch as a modification of GPL-3.0 nestang sources. The flow still depends on tools outside the repository: oss-cad-suite at `~/oss-cad-suite`, the nextpnr fork built at `~/tools/nextpnr-mathieufro` against the regenerated GW5AST-138C database, the apicula fork at `/run/media/vash/GIT/apicula-mathieufro` (`4281068`) and `~/tools/pydeps`. Each can be overridden in the environment. The first build ran with `ALLOW_UNVERIFIED_CLOCK=1`, because the clock-topology profile carried over from Tang-Build predates the clock monitor, which adds 256 `clk`-domain flops (943 against 1199 fabric fallbacks). It produced `desktop.90d3fa3a84fa.bin` with sha256 `90d3fa3a84fa8cf80ca2d7d107fde9e00ac15b4362f34bba0d8ddeef53155718`, identical to the core on the card, with setup 0 and hold 0. The profile was then regenerated from that build's log. A second build from the committed scripts, with both gates enforced and no override, produced the same sha256 and reported no clock-topology regression. The board was not touched, because the built core is the one it already runs, which is why Deployment and User Test do not apply. The same build confirms what TOOL-017 says about HDMI: `hclk5` falls back to fabric at all four of its sinks, `clk` at 1199 and `clk27` at 1, and the pixel clock routes on global resources only. Reading Tang-Build entries 36 to 77 for this cycle also corrected one premise that the HDMI work had carried. Entry 36's report of a receiver locking for 0.6 s was withdrawn by Tang-Build entry 68 as the capture card's own idle field. So no measurement shows the open-flow core emitting any TMDS signal at all, and the claim that fabric-clock jitter breaks the link was never established. TOOL-017 now names the build of record, and the README describes the open flow and keeps the Gowin script as an HDMI reference only. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, checked that the reference's YAML blocks parse, and validated this entry as number 34 with six canonical sections, prose in Outcome and Next Steps, an allowed Status set and a count within the 100-entry limit.

#### Next Steps:

The next work is to find out what the open-flow core's HDMI pins actually do, before any further work on the clock model. No working open-flow HDMI design exists on this die to compare against: apicula's DVI example covers only the GW1N and GW2A boards, and its GW5AST-138C OSER10 examples are fuzzer shapes fed from a pin. The serialiser, divider and differential-output support of the apicula fork has therefore never been proven on hardware for this device. The proposed bisection is a minimal open-flow test core: one PLL, one `CLKDIV` and four `OSER10`s into the same TMDS pins, sending a fixed colour. Build it first at 720p and then at 640x480, where the bit clock is 125.875 MHz rather than 371.25 MHz. If 640x480 locks, timing on a fabric-routed clock is the fault. If neither locks, the fault is in configuration (the PLL, divider, IOLOGIC or output-buffer fuses), which a small design makes decodable tile by tile. A scope on the TMDS clock pair would answer the first question directly, if one is available. Several records are still owed a revision: TOOL-003, TOOL-005, and BL6-003, BL6-008 and BL6-009, as entry 33 named. Whether `tools/build_desktop_core.sh` and the other Gowin scripts should be retired is the user's decision.

#### Files Modified:

- README.md
- THIRD_PARTY.md
- third_party/patches/desktop/open/0001-open-toolchain-portability.patch
- tools/build_desktop_core_open.sh
- tools/desktop_open/build.sh
- tools/desktop_open/clock_constraints.py
- tools/desktop_open/clock_profile.json
- tools/desktop_open/clock_route_inventory.py
- tools/desktop_open/fs_to_bin.py
- tools/desktop_open/pnr.sh
- tools/desktop_open/pnr_timing.py
- tools/desktop_open/synth.sh
- tools/desktop_open/synth.ys

#### Status:

- Build: PASS
- Deployment: N/A
- User Test: N/A

---

## 35 COMMIT Unreleased 2026-10-11T17:47:15-07:00

#### Coming From:

Unreleased d68bc18

#### Purpose:

Isolate the open flow's HDMI failure in a minimal test core built with both flows at two bit-clock rates, to settle whether it is a timing fault at speed or a configuration fault.

#### Outcome:

The open flow's HDMI failure is a configuration fault and not a timing fault at speed. `fpga/hdmi_test/hdmi_test.v` is a minimal DVI core that sends eight colour bars and carries only the desktop core's HDMI hardware path. Its two PLL wrappers, `gowin_pll_27.v` and `gowin_pll_hdmi.v`, are copied at build time from the reconstructed nestang tree. It uses `CLKDIV` /5, three `OSER10`s wired as in nestang's `serializer.sv` and four `ELVDS_OBUF`s on the desktop core's TMDS pins, with the clock lane driven by the divider's output, as in `nes2hdmi.sv`. A DVI 1.0 TMDS encoder replaces nestang's HDMI packet logic. `tools/build_hdmi_test.sh` builds it with the open flow or, as a reference only, with Gowin EDA, at 1280x720 or at 640x480. The open builds carry the desktop core's `pll_27` and `pll_hdmi` pins, and the Gowin builds leave placement to the vendor. At 640x480 the pixel clock is 25.2 MHz and the bit clock 126 MHz, from an in-band VCO of 1134 MHz (27 MHz x 42 / 9). The first choice, 1512 MHz, was refused by the fork's `gowin_pack`, which enforces the datasheet's 650-1300 MHz band. That refusal also showed that the desktop core's own `pll_hdmi` runs its VCO at 1485 MHz, above the band, which the packer admits only as a point measured from a vendor bitstream. All four builds passed their timing gates. The open 720p build reproduces the desktop core's open-flow clock topology exactly: `hclk5` on fabric at 4 sinks, `clk27` at 1, and the pixel clock on global resources only. The open 480p build has the same topology. The four builds were `hdmi_test.caa6a014bbf2.bin` (Gowin 720p), `hdmi_test.35672830f76c.bin` (open 720p), `hdmi_test.c56c910850de.bin` (Gowin 480p) and `hdmi_test.3b1ae8b25756.bin` (open 480p). Each was uploaded to `/cores/console138k/` as `hdmi-test-<flow>-<mode>.bin` and loaded with `tangload` on the same board, cable and monitor. The user reported perfect colour bars at 1280x720 60 Hz from Gowin 720p and at 640x480 from Gowin 480p, and no signal with the monitor going to sleep from both open builds. Because the controls pass, the design, the pins, the PLL recipes, the board, the cable and the monitor are all good. Because the open flow fails at a third of the bit-clock rate, and with an in-band VCO, neither the 371.25 MHz bit clock on fabric nor the out-of-band VCO explains the failure. The remaining difference lies in how the open flow configures the path: the PLLs, the divider, the clock lanes into the I/O logic, the serialisers or the differential output buffers. The monitor sleeping suggests it sees no TMDS clock at all. That clock lane is the divider's output, but this is an inference, since no scope is available. The board was returned to the desktop core, which answered core 84 with its sockets declared. TOOL-017 was revised with the result, and the README lists the new build script. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, checked that the reference's YAML blocks parse, and validated this entry as number 35 with six canonical sections, prose in Outcome and Next Steps, an allowed Status set and a count within the 100-entry limit.

#### Next Steps:

The next step is to compare the two 480p test bitstreams block by block, which this small design now makes tractable. Decode `hdmi_test.3b1ae8b25756.bin` and `hdmi_test.c56c910850de.bin` with the apicula fork's unpacker, then compare the fuses of the two PLLs (including the sites Gowin chose), the `CLKDIV`, the HCLK blocks and clock lanes, the three `OSER10` I/O-logic tiles, and the four `ELVDS_OBUF` I/O blocks and their bank settings. A difference that can be moved into the open build, by constraint or by a packer fix, is then tested on the board through the same 480p core, where the user's monitor gives a pass or fail. Two cheaper probes can run alongside the comparison if needed. One open build drives the clock lane from `clk27` instead of the divider, which separates a dead divider from a dead output buffer. The other pins `pll_hdmi` to the site Gowin chose once the decode names it. The records owed since entry 33 (TOOL-003, TOOL-005, and BL6-003, BL6-008 and BL6-009) are still owed.

#### Files Modified:

- README.md
- fpga/hdmi_test/build.tcl
- fpga/hdmi_test/hdmi_test.cst
- fpga/hdmi_test/hdmi_test.v
- fpga/hdmi_test/hdmi_test_open.cst.in
- tools/build_hdmi_test.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---
