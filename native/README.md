# Scorebot native core (ABI v1)

This is the Scorebot source dependency for a Pi leaderboard. Nimbus can control
any or all roster colors through the local action API. An all-LOCAL game needs
only this library and its caller: no hardware leaderboard, player board, or
ESP32 bridge. Physical player boards use the existing v4 protocol against the
same authoritative state. Run one authority process per game; this library
does not elect leaders or coordinate competing processes.

## Build and use

Requires a C++17 compiler, Make, and Python 3.9+ for the standard-library-only
adapter/tests. From this repository root:

```sh
make -C native
make -C native check
PYTHONPATH=native/python python3 native/examples/local_game.py
```

Linux/Pi produces `native/build/libscorebot.so`; macOS produces
`native/build/libscorebot.dylib`. **Build on each target architecture.** A Mac
library is not a Pi library. Cross builds can override `CXX`, `CC`, `UNAME_S`,
`BUILD`, and compiler/linker flags; running `check` requires a runnable target.
The small library always rebuilds so changing identity flags cannot silently
reuse a previous authority. It compiles without C++ exceptions and exports only
the C interface. The C11 test links against the actual shared library.

Pin this repository at the deliverable commit in the consuming application's
source dependencies (for example, a submodule). Build from that source and add
its `native/python` directory to the application's import path; there is no
Python package installation or sibling-checkout assumption. Load an explicit
artifact with `Library("/absolute/path/to/libscorebot.so")` or set
`SCOREBOT_LIBRARY`; otherwise the adapter uses this checkout's `native/build`.

```python
from scorebot import Action, Library, LobbyMode, Player, Result

library = Library()
with library.game() as game:
    # Restore trusted saved game.read().to_dict() here, if present; check result.
    assert game.resume_authority() == Result.OK
    # Persist the new term before accepting actions or publishing snapshots.
    for player in (Player.RED, Player.BLUE):
        assert game.set_lobby_mode(player, LobbyMode.LOCAL) == Result.OK
    assert game.start() == Result.OK
    assert game.local_action(Player.RED, 3, Action.ADD) == Result.OK
    assert game.local_action(Player.RED, 0, Action.OK) == Result.OK
    saved = game.read().to_dict()
    wire = game.read().to_wire(library.protocol_version)
```

The example deliberately starts a fresh game. A resumed game keeps its profile,
scores, frozen roster, turn, and operation high-water marks; skip fresh lobby
configuration when restoring an already started game.

## Interface contract

The complete C contract and result values are in `include/scorebot.h`.
`scorebot_abi_version()` is the stable discovery entry point; all v1 calls have
the `scorebot_v1_` prefix. Creation checks the requested ABI. Snapshot reads and
restores require the exact v1 size (84 bytes); restore also checks its version,
configured authority, masks, booleans, and game invariants. The Python loader
checks ABI and struct size before creating a handle and prevents integer
truncation during argument/dictionary conversion.

| Operation | Contract |
| --- | --- |
| `create` / `destroy` | Opaque allocation; caller destroys once. Null destroy is safe. Python supports `with`, idempotent `close()`, and finalizer cleanup. |
| `read` / `restore` | Caller-owned copy; atomic replacement from trusted local persistence. Includes all four operation marks, game ID, term/version, masks, and turn. |
| `resume_authority` | After create/restore, drop live links, prune implicit lobby availability and increment term. Persist before publication. Refuses exhausted `UINT32_MAX` term. |
| `set_lobby_mode` / `start` / `reset` | Existing rules; LOCAL/PHYSICAL/OFF; start freezes roster. Ordinary reset preserves profile, game ID, term and operation marks. It is not the hardware maintenance reset. |
| `local_action` | Same firmware ADD/OK helper. ADD(0) is a no-op; OK(0) can pass. Selected roster colors remain controllable when their boards are absent. Clear the caller's draft only on `Result.OK`, after either action. |
| `player_action` | Requires an admitted transport node, matching payload node and game ID. Existing deduplication and out-of-turn correction rules. |
| `set_connected` / `mark_sleeping` | Live links update version; connect clears sleep. Sleep is an unversioned hint, then disconnect separately. Sleep retains the player's turn eligibility. |
| `admit_peer` / `is_leader` | Full public Bluetooth address, identity, and protocol checks against configured roles; passive displays cannot submit scores or act as leaders. |
| revision/operation helpers | Existing term-first ordering, maximum operation reconciliation and acknowledgement comparison. No wrap-aware sequence rules are introduced. |

All integers have explicit widths. Player order is red, blue, green, white
(0..3); no player is 4. Booleans are 0/1; masks use bits 0..3. No raw C++ object,
borrowed buffer, file descriptor, or callback crosses the ABI. Calls do not
perform I/O or scheduling. Serialize calls to each handle externally, including
reads and destruction; `ctypes` may release the GIL. Null/invalid arguments and
ABI errors do not mutate state or output buffers, except creation clears its
output pointer on failure. Arbitrary non-null pointers and use-after-destroy
are caller errors, as with other C APIs.

Python action/restore methods return `Result`, including negative rejection
codes. Construction, read, and peer-query failures raise `ScorebotError`;
Python representation/range errors raise `TypeError` or `ValueError`. A policy
rejection may intentionally mutate state: **physical score overflow consumes
the operation and increments version**, allowing acknowledgement. Repeating it
is a duplicate. Local overflow is rejected without mutation. Physical
out-of-turn OK returns `ACCEPTED_WITHOUT_TURN_CHANGE`; local ADD/OK returns OK
when committed, including an out-of-turn correction. Read the authoritative
snapshot to observe the resulting turn. Start/lobby/connection/reset calls
return `NO_CHANGE` when the existing rules decline the transition.

`State.to_dict()` / `State.from_dict()` is a lossless, versioned JSON-compatible
native persistence record. Persist this dictionary, not raw struct bytes. It
is distinct from firmware Preferences storage; existing firmware JSON and its
legacy recovery behavior remain unchanged. `State.to_wire(protocol)` maps a
read snapshot to the existing firmware JSON keys and BoardRole turn values
(0 = unknown, 2..5 = colors). It omits the local sleep hint, just as firmware
does. Do not use wire JSON as the native persistence format. No wire service
shape or JSON field changes are made, so protocol remains **v4**, following
`Protocol.hpp`'s version policy. ABI versioning is independent of wire version.

The portable sources remain `GameRules.hpp`, `LeaderboardControlRules.hpp`,
and `ReplicationRules.hpp`, consumed directly by firmware and this library.
Firmware also uses the extracted sleep/startup helpers. `GameState.cpp`,
Arduino/FreeRTOS, radios, displays, flash storage, retry timing, UI edit buffers,
and animation are outside this native build. Python contains marshaling and
enum/JSON translation only.

## Configure the Pi authority

No real Pi identity is assumed or stored here. The wall integration must supply:

1. `SCOREBOT_LEADER_NODE_ID`: a stable nonzero uint32 ID distinct from every
   configured hardware board (including the retired hardware leader).
2. `SCOREBOT_LEADER_BLUETOOTH_MAC`: the Pi central adapter's actual stable
   **public** Bluetooth address as a 48-bit integer in displayed byte order:
   remove the colons and prefix `0x`; append `ULL` for compiler definitions.
   This is the actual radio address, not an ESP32 base MAC. A rotating/random
   address does not satisfy the existing public-address trust contract.

Supply the same two environment variables for the native build and the player
firmware build. For example, after setting both to the actual values:

```sh
make -C native BUILD=build-pi check
pio run -c platformio.pi.ini -e pi_players -e pi_epaper
```

`platformio.pi.ini` reads both environment variables and derives its build flags
from the normal environments. It never alters personal settings or the default
hardware configuration. Equivalent explicit `-D` definitions can be used in
`CPPFLAGS` or firmware build flags. Supplying only one input, zero/out-of-range
values, or a collision with a configured hardware identity/address fails
compilation. Use a matching pair on every participating player board and
native build. The Pi profile **replaces**, rather than adds to, the default
leader allowlist. Existing default builds continue to recognize the original
hardware leader. Boards with different authority profiles are separate games;
there is no automatic authority migration of saved games. Restore refuses a
snapshot belonging to a different configured leader.

The Pi's downlink snapshots/reset messages must claim that configured node ID;
the player's transport callback resolves its sender from the complete public
address, then validates protocol and claimed authority. Unknown nodes,
different full addresses sharing the same tail, random-address peers, mismatched
protocols and spoofed payload identities are rejected. This preserves the
existing address-based trust model; it does not add cryptographic pairing or
authenticate an attacker capable of cloning the trusted radio address.

## Transport responsibilities for the wall integration

The Pi takes the existing leaderboard **central** role. It discovers player
peripherals using the GATT UUIDs in `BleService.hpp`, reads their little-endian
uint32 identity and uint16 protocol, and calls `admit_peer` using the transport's
actual full public address. Use the ESP32 Bluetooth offset appropriate to that
player firmware (current XIAO default is 1). The configured Pi address is always
matched exactly, with no offset. Retain the validated transport identity for
the connection; never derive it from incoming JSON.

Announce only admitted players through `set_connected`; display peers have no
roster slot. After start, retain only physical links in the frozen roster
(display links remain eligible). Validate incoming v4 JSON field types and
message size before marshaling; route player score requests to `player_action`
with the validated transport node and claimed payload node/game/operation.
Route sleep notices only from that admitted player with a matching payload
identity; mark sleeping, then report the subsequent disconnect. Activity
messages affect transport/UI liveness, not game state. Player input cannot
restore authority state. Use local actions for Nimbus, not invented physical
identities or operation IDs.

Read state after each request: commit/persist/publish changes, including a
consumed overflow. Resend the current snapshot to acknowledge duplicates and
rejections. Encode wire JSON compactly (`json.dumps(wire, separators=(",", ":"))`)
within `kMaxWireMessageSize`, write the player downlink, and subscribe to its
uplink. Publish periodic state heartbeats as firmware does (1.5 seconds), and
handle disconnects, roster admission, retries, persistence durability and
liveness outside the core. Player firmware expires its leader after 15 seconds
without an accepted state/reset. An all-LOCAL game does not need any transport.

This deliverable implements none of that BLE client, wall animation, or
controller UI. The e-paper profile only changes which leader address it trusts;
the Pi must provide existing manufacturer-data advertisements separately if
that passive listener is to display its game.

## Validation

Run the original native rule suite plus authority configuration checks:

```sh
./test_runner.sh logic
make -C native check
PYTHONPATH=native/python python3 native/examples/local_game.py
```

The C11 ABI smoke/invalid-input test and 11 Python integration tests cover
all-LOCAL/mixed games, ADD/OK, out-of-turn correction, overflow consumption,
duplicates, sleep/rejoin, revisions, state round trips, ABI errors, ownership,
invalid inputs, and display privilege rejection. `test_leader_identity_config.py`
compiles production authority checks under default and synthetic Pi profiles,
then verifies invalid/colliding configurations fail compilation.

See `VALIDATION.md` for the exact deliverable checks, independent review and
what has not been verified on a Pi or physical BLE network.
