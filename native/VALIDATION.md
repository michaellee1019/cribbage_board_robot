# Deliverable validation — 2026-11-19

Host: macOS arm64, Apple Clang, Python 3. The native artifact inspected was
an arm64 Mach-O dylib. All commands below passed from the repository root.

```sh
./test_runner.sh logic
make -C native check
PYTHONPATH=native/python python3 native/examples/local_game.py

# SYNTHETIC identity, used only for tests; not the real Pi configuration.
make -C native BUILD=build-pi-test \
  SCOREBOT_LEADER_NODE_ID=0x10203040u \
  SCOREBOT_LEADER_BLUETOOTH_MAC=0x123456789abcULL check

make -C native BUILD=build-sanitize \
  CXXFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' build-sanitize/test_c_api
native/build-sanitize/test_c_api

pio run -e controller -e leaderboard_epaper
pio run -e debug -e button_diag -e sleep_test
SCOREBOT_LEADER_NODE_ID=0x10203040u \
SCOREBOT_LEADER_BLUETOOTH_MAC=0x123456789abcULL \
  pio run -c platformio.pi.ini -e pi_players -e pi_epaper

git diff --check
```

- The existing 21 native rule executables and three USB Python tests passed.
  The authority configuration test compiled/ran both default hardware and
  synthetic Pi profiles, then checked ten invalid configurations fail to build.
- Each normal/configured shared-library check linked a C11 caller and passed
  11 Python integration tests. Coverage includes all-LOCAL and mixed games,
  ADD/OK, out-of-turn actions, signed score overflow, duplicate/stale operations,
  revisions, game/term/high-water state round trips, sleep/reconnect, nulls,
  invalid ranges/masks/booleans, wrong sizes/versions, ownership and authority.
- The standalone C ABI test passed AddressSanitizer and UndefinedBehaviorSanitizer.
  The Python interpreter was not run under those sanitizers.
- All seven firmware environments above built successfully. `ledwall` and
  `ledwall_xiao` do not consume any changed inputs (their `BleService.hpp`,
  `BoardIdentity.hpp` and `Protocol.hpp` remain unchanged), so were not rebuilt.
  The hardware error-handler integration test was not run.
- `nm -gU native/build/libscorebot.dylib` showed only the 21 intended C API
  symbols exported; `otool -L` confirmed the test uses `@rpath/libscorebot.dylib`.

One independent agent reviewed the actual C ABI, adapter ownership/error paths,
and production peer-authority paths. It found a stale-build risk when changing
identity flags; the Makefile now always recompiles the tiny library. Its final
review reported no remaining substantive findings. Its draft-clearing comment
clarification was also applied.

No Linux/Pi compiler or runtime, actual Pi adapter address, physical BLE
connection, e-paper reception from a Pi, or hardware game was verified. The
synthetic Pi-profile tests on macOS and ESP32 cross-builds are **not** Pi runtime
validation. Linux's shared-library/rpath branch is provided but has not been
executed here. Build and run `make -C native check` on the actual Pi before
integration. No binary is committed or claimed portable between architectures.

The actual Pi identity must still be supplied by the wall integration. No
ledgrid code, BLE client, animation, UI, flashing, push, or personal
credentials/settings changes are included.
