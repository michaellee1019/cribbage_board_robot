# Runtime pairing validation — 2026-11-19

Software checkpoint on macOS arm64 with Apple Clang, Python 3, and the pinned
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
turn Blue, version 6. E-paper acceptance is still in progress. Bench firmware is
in `test/hardware/pairing_probe.cpp`. It receives the target's full identity and
session over UART at runtime, checks foreign-session rejection, then exercises
mirrored lobby/score/turn commands, duplicate suppression, and privilege
rejection. It refuses to overwrite an active game. Restore normal display
firmware after using this probe. Ignored logs and saved-state backups are in
`.pio/validation/` in the worktree.

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

Linux/Pi execution and actual player-board physical controls are not validated
by these host tests. Build and run the native check on the consuming Pi. No
shared binary is committed or claimed portable between architectures.
