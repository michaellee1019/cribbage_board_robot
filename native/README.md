# Scorebot native core (ABI v1, wire protocol v5)

The portable C++ core supports Pi, Linux, and macOS authority applications, with
a C ABI and a standard-library Python adapter. Local/virtual players still use
`local_action`; an all-LOCAL game needs no ESP32 or BLE network. The library
performs no radio, UI, persistence, scheduling, or leader election I/O. The host
application supplies those services and runs one authority process per session.

## Build and use

Requires a C++17 compiler, Make, and Python 3.9+. Build on the target architecture:

```sh
make -C native check
PYTHONPATH=native/python python3 native/examples/local_game.py
```

Linux produces `native/build/libscorebot.so`; macOS produces
`native/build/libscorebot.dylib`. Cross builds may override `CXX`, `CC`, `UNAME_S`,
`BUILD`, and compiler/linker flags. Pin this repository's source revision in the
consuming application and add `native/python` to its import path. Load an
explicit path with `Library(path)` or `SCOREBOT_LIBRARY`, or use the default
artifact in this checkout. The core builds without C++ exceptions.

```python
from scorebot import Action, Library, LobbyMode, Player, Result

library = Library()
with library.game() as game:
    # Fresh runtime node/session identities and a random pairing number 0..20.
    assert game.resume_authority() == Result.OK
    for player in (Player.RED, Player.BLUE):
        assert game.set_lobby_mode(player, LobbyMode.LOCAL) == Result.OK
    assert game.start() == Result.OK
    assert game.local_action(Player.RED, 3, Action.ADD) == Result.OK
    assert game.local_action(Player.RED, 0, Action.OK) == Result.OK
    saved = {
        "pairing": game.pairing,
        "state": game.read().to_dict(),
        "controllers": game.controller_marks(),
    }
    wire = game.read().to_wire(library.protocol_version)
```

Persist pairing and game state together. Recovery uses
`library.game(**saved["pairing"])`, then `restore(saved["state"])` and
`controller_marks(saved["controllers"])`. Check results, call
`resume_authority()`, and durably save the new term before publishing or accepting
requests. Resume drops live connections and prunes implicit lobby availability;
it preserves scores, the frozen roster, and operation marks. Do not configure a
fresh lobby over a restored active game.

`Library.game(node_id=..., session_id=..., pairing_id=...)` accepts runtime
configuration. Omitted values are randomly generated; node/session must be
nonzero uint32 values and pairing must be 0..20. Reuse the saved values for
recovery. No adapter address or authority ID is compiled into players or the
library. The compatibility environments `pi_players` and `pi_epaper` now build
the same firmware as their ordinary counterparts, without identity flags.

## Pairing and transport integration

A Pi authority advertises `game.pairing_packet()` as primary manufacturer data,
and exposes/uses the game service UUIDs in `BleService.hpp`. Its UI or animation
shows `game.pairing["pairing_id"]`. A host creating a session can perform startup discovery and elect the
smallest candidate runtime ID before creating its authority. A product that
owns a persisted session can remain its sole authority; hardware leaderboards
join that session as mirrors. The native game handle is authority-only: a
replica joining another authority requires host application support. Existing
authorities win over fresh candidates. Recovering active games and explicitly
selected replicas remain bound to their saved leader. An idle hardware authority
can join another authority on startup. Never promote a replica solely because
the authority is asleep or disconnected.

The shared policy and codec are in `PairingRules.hpp`. A legacy advertisement
can carry the 22-byte discovery packet plus GAP flags. The manufacturer bytes
are independent of C++ struct layout:

| Byte offset | Content |
| --- | --- |
| 0..4 | `ff ff 53 50 05` (experimental company ID, `SP`, protocol v5) |
| 5..8 | Runtime node ID, uint32 little endian |
| 9..12 | Selected authority ID, uint32 little endian |
| 13..16 | Full session ID, uint32 little endian |
| 17 | Pairing number 0..20; 255 before selection |
| 18 | Device kind: player=1, leaderboard=2, passive display=3 |
| 19 | BoardRole: unknown=0, leaderboard=1, red..yellow=2..5 |
| 20 | Flags: authority=1, user-selected=2, recovering candidate=4 |
| 21 | Occupied physical-color mask, bits red..yellow=0..3 |

A player UI browses discovered authorities and confirms the displayed number;
it does not ask the user to enter it. The selected authority ID and full session
ID identify the game. The short number can change to resolve a collision
without changing that identity. A leaderboard candidate advertises its own
node as leader with authority unset; an established authority sets flags 1|2.
A mirrored controller advertises the selected leader/session with kind 2,
role 1, and only the selected flag. Display-only devices use kind 3, role 0.

The authority is BLE central. Scan for discovery manufacturer data, connect to
selected peripherals, read identity, protocol, and the **live** pairing
characteristic (`kPairingUuid`, 22 bytes), and check identity/protocol consistency.
Use the actual address and type reported by that connection:

```python
kind, player = game.admit_peer(pairing_bytes, transport_address, public_address=True)
```

`PeerKind.PLAYER`, `DISPLAY`, and `CONTROLLER` are distinct. Only admitted
players can call `player_action`; only admitted controllers can call
`controller_action`. Rejected/foreign sessions and occupied colors return
`(PeerKind.REJECTED, Player.NONE)`. The per-game directory binds complete
transport addresses (public or stable random), identities, and colors. Persist
the host's validated transport bindings and re-admit them before opening a
recovered session to new devices, retaining sleeping players' color reservations.
Legacy process-global `Library.admit_peer`/`is_leader` queries fail closed;
`leader_node_id` returns zero. Use `game.read().leader_id` instead.

For a player peer, update `set_connected` only after admission; its payload node
must match the admitted transport node passed to `player_action`. Do not derive
transport authority from incoming JSON. After start, reject players outside the
frozen roster. Display/controller connections do not occupy player slots.
Map validated `control` JSON to `controller_action(node, operation, command,
player, value, passes_turn, game_id, term)`; commands are SCORE=0, START=1,
LOBBY=2. The value is a score delta or lobby mode. Controller marks are updated
with the same authoritative revision as their effects, including consumed
rejections, and are included by `game.read().to_wire(...)`.

Publish compact state JSON after every mutation and periodically (firmware uses
1.5 seconds). Resend current state for duplicates/rejections. Validate field
types and the 512-byte wire limit before marshaling. Notifications carry player
requests to the central; write state to each peer's downlink. Player sleep
notices are hints: mark sleeping, then report the disconnect separately. A
player considers its authority unavailable after 15 seconds without accepted
state/reset traffic. Enrollment binds the selected session and transport; it
does not introduce cryptographic Bluetooth bonding.

The hardware authority places the 24-byte `LeaderboardBroadcast.hpp` score
packet in **16-bit service data UUID `0xfff0`** in its scan response, keeping
discovery manufacturer data in its primary advertisement. The full UUID is
`0000fff0-0000-1000-8000-00805f9b34fb`. Using separate AD types prevents Python
BLE APIs that key manufacturer data by company ID from losing discovery. A host supporting the
e-paper listener must publish both and permit active scanning. Score data is
accepted only from the display's selected discovery source. Unconfigured
passive displays choose the sole visible game after discovery and wait when
selection would be ambiguous.

## Stable game API and ownership

The C11 contract is in `include/scorebot.h`. ABI remains **v1** and the state
struct remains **84 bytes**; the runtime pairing/controller APIs are additive.
C callers configure pairing immediately after `create`. Game handles own their
state; snapshot reads return caller-owned copies. `destroy(NULL)` is safe.
Python supports `with`, idempotent `close()`, and finalizer cleanup. Serialize
access to each handle externally, including reads and destruction (`ctypes`
may release the GIL). No borrowed C++ objects, callbacks, or exceptions cross
this interface.

`State.to_dict()` is the versioned JSON-compatible game persistence record;
raw struct bytes and wire JSON are not persistence formats. Persist pairing,
controller marks, and transport bindings alongside it. `State.from_dict()`
validates integer ranges and shape. For a standalone restored State, pass its
saved session explicitly to `to_wire(protocol, session_id=...)`; reading from a
game supplies session and controller metadata automatically.

Players are RED..YELLOW=0..3, NONE=4; masks use bits 0..3 and booleans are 0/1.
Local ADD(0) is a no-op; OK(0) can pass. Clear a local draft only on `Result.OK`.
Physical requests use operation high-water marks. A physical score overflow is
consumed and acknowledged without changing the score; a local overflow is
rejected without mutation. Out-of-turn OK scores without passing. Ordinary
`reset()` starts another lobby while preserving identity and operation marks;
it is separate from the hardware maintenance reset. Authority term exhaustion
is refused. Invalid arguments do not partially mutate state or output buffers.

Protocol **v5** is intentionally incompatible with v4 peers and firmware
snapshots. Update participating devices together. No separate Pi-specific
firmware compilation is required. See `VALIDATION.md` for checks and hardware
validation still required.
