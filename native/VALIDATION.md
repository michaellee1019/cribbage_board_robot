# Runtime pairing validation — 2026-11-19

Validated on macOS arm64 with Apple Clang, Python 3, and the pinned
ESP32 Arduino toolchain. The branch/worktree is `codex/dynamic-board-pairing`
at `/private/tmp/cribbage-board-robot-dynamic-pairing`.

Completed software checks:

```sh
./test_runner.sh logic
make -C native check
PYTHONPATH=native/python python3 native/examples/local_game.py
make -C native BUILD=build-sanitize \
  CXXFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' build-sanitize/test_c_api
native/build-sanitize/test_c_api
pio run -e controller -e leaderboard_epaper
pio run -e debug
pio run -d ledwall -e ledwall -e ledwall_xiao
pio run -d test/hardware
```

The rule suite has 23 C++ executables plus three USB discovery tests. The native
check links the C11 ABI test and passes 14 Python tests. Coverage includes
runtime session isolation, arbitrary identities, public/static random addresses,
occupied colors, display/controller privileges, startup election, number bounds
and collisions, controller replay across recovery, existing scoring/sleep/roster
behavior, atomic invalid-input handling, and a maximum-width compact snapshot
(454 bytes, within the 512-byte ATT limit). The C test also passes AddressSanitizer
and UndefinedBehaviorSanitizer. Python was not run under those sanitizers.

The connected leaderboard was confirmed to have displays at 0x70..0x73, and
selected its hardware role without an identity lookup. Live startup exposed a
controller limit missed by cross-compilation: nine connections plus scanning
and advertising exceeds the ESP32-S3's ten BLE activities. The configured limit
is now eight, guarded by a static assertion; the corrected hardware starts BLE
and elects an authority with a runtime session and pairing number.

The live bench probe passed against the connected leaderboard: an unrelated
session was ignored, the previously unknown controller was admitted, Red and
Blue were enabled locally, Red +7 was committed exactly once despite retries,
Red's turn passed to Blue, Blue -2 was committed, and a controller impersonating
a player was rejected without a revision change. Final scores were [7,-2,0,0],
turn Blue, version 6.

The e-paper listener then received that live game and completed a full refresh
in 16.45 seconds and a score partial refresh in 1.65 seconds. A separate Python
Bleak scan on macOS observed discovery manufacturer data and score service data
together, with the expected roster, turn, and all four scores. Installing the
production controller image and restarting the boards preserved the same full
session, pairing number 19, scores, and turn; the e-paper rejoined automatically.

The pre-test NVS backups were restored to remove the synthetic game/controller
reservation. Both boards retain the new production firmware. Final radio and
panel checks confirm a fresh lobby with zero scores and no roster, **pairing
number 0**, and matching leader/session selection. The final idle-startup
adjustment was built and installed: active games stay pinned, while an idle
hardware authority can discover and join an existing authority at startup.

Bench firmware is in `test/hardware/pairing_probe.cpp`. It receives the target's
full identity and session over UART at runtime, checks foreign-session rejection, then exercises
mirrored lobby/score/turn commands, duplicate suppression, and privilege
rejection. It requires a fresh, empty lobby before sending controls. Restore
normal display firmware after using this probe. That restoration is complete on the connected
e-paper board. Ignored evidence and saved-state backups are in `.pio/validation/`
in the worktree:

- `pairing-probe.log`: foreign-session, duplicate, score/turn and privilege checks.
- `leaderboard-live-test.log`: live GATT admission and authoritative commits.
- `epaper-live-scores.log` / `epaper-recovery.log`: score reception and panel refreshes.
- `epaper-final-lobby.log`: final pairing number 0, empty roster, zero scores, successful refresh.
- `mac-ble-scores.json` / `mac-ble-final.json`: independent Python BLE observations.
- `leaderboard-nvs-before.bin` / `epaper-nvs-before.bin`: saved-state backups;
  these are local ignored artifacts, not source files.

Consumer contract:

- ABI v1 and the 84-byte state layout remain; pairing APIs are additive.
- Wire protocol v5 requires live pairing GATT metadata and session/controller
  fields. Legacy process-global admission queries fail closed.
- Python applications may remain the sole authority for their persisted
  session. The native game handle does not join another authority as a replica.
  Hardware leaderboards can join it as mirrored controllers.
- Discovery is primary manufacturer data (`ff ff SP 05`); the score packet is
  scan-response **service data UUID 0xfff0**, keeping both independently visible
  to Python BLE APIs. Active scanning is required for scores.
- Persist pairing configuration, controller marks, and validated transport
  bindings alongside the state record. Details and the byte layout are in
  `README.md` in this directory.

Linux/Pi execution, multiple physical leaderboards, and actual player-board
color/pairing controls have not been exercised on hardware here. Portable
policy tests passed on macOS; the connected bench contained one leaderboard and one
e-paper ESP32-S3. Build and run the native check on the consuming Pi. No
shared binary is committed or claimed portable between architectures.
