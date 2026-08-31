# First-Class Leaderboard Printer Support

## Goal

Add printing to the production controller firmware. On the leaderboard only,
holding all four leaderboard buttons for five uninterrupted seconds will print
an immutable copy of the current authoritative game state through
`POST http://192.168.1.163:8099/text/print`.

Printing must behave like the existing RESET and OTA maintenance gestures:

- the component buttons do not perform score or turn actions;
- the displays show the pending action immediately;
- the encoder light flashes progressively faster during the five-second hold;
- releasing any required button early cancels the action and restores the game UI;
- execution starts once, at the five-second threshold.

The Wi-Fi station must be off at boot and whenever no print is in progress.
Network work must not block the game-state mutex, input dispatcher, persistence,
or BLE loop.

## User Experience

### Gesture

- Define a leaderboard-only `Print` maintenance action whose required mask is
  all four physical leaderboard buttons (`0x0f`). Player boards do not gain a
  print gesture.
- Give the all-four mask priority over the two leaderboard maintenance masks.
  If RESET or OTA is recognized while the user is still assembling the
  four-button chord, promote it to PRINT as soon as all four buttons are down.
- Start the five-second PRINT timer when the fourth button becomes pressed. A
  user must therefore hold all four buttons for the full safety interval.
- Suppress all four component actions until all four buttons have subsequently
  been released, whether PRINT completes or is cancelled.
- While holding, scroll `PRINT` on all four displays and use the existing
  accelerating maintenance flash cadence with a distinct cyan print color.

### Progress and result display

After the hold completes, replace the hold animation with these full-brightness
phases on all four leaderboard displays:

1. `WIFI` while the station radio starts and joins the configured network.
2. `SEND` while the HTTP request is being transmitted and answered.
3. `DONE` for approximately two seconds after any successful 2xx response,
   including an idempotent replay.

The light remains cyan during network work, becomes green for `DONE`, and
returns to the authoritative turn color when the normal UI is restored.

Failures are nonfatal. Scroll one short message on all four displays for no
more than five seconds, then restore the latest game UI. Use fixed messages so
the display behavior is deterministic and does not expose an arbitrary server
response:

- `WIFI FAIL` for authentication, association, DHCP, or Wi-Fi timeout;
- `NO ROUTE` for TCP connection failures;
- `PRINT TIMEOUT` for an HTTP timeout;
- `HTTP 409`, `HTTP 4XX`, or `HTTP 5XX` for server responses;
- `BAD RESPONSE` for a malformed successful response;
- `PRINT BUSY` if a second job cannot be accepted.

An ordinary local input may dismiss a terminal success/error message early,
but that input is consumed rather than applied invisibly to the game. BLE state
updates continue behind the temporary UI and the latest state is rendered when
the overlay ends.

## Printed State

Capture the print job at the instant the five-second hold completes. The job is
an immutable value containing:

- leaderboard node ID;
- `gameId`, `term`, and `version`;
- whether a game is active;
- all four authoritative total scores;
- the authoritative turn.

Do not include uncommitted leaderboard input deltas. They are edit buffers, not
part of the authoritative totals, and remain intact after the print UI closes.

Build this stable version-1 payload from the captured value:

```json
{
  "version": 1,
  "filename": "scorebot-g<game>-t<term>-v<version>.png",
  "title": "SCOREBOT",
  "lines": [
    "RED    121",
    "BLUE    96",
    "GREEN   80",
    "WHITE   67",
    "TURN: RED"
  ],
  "footer": "Printed {{Timestamp}}"
}
```

Format score lines with `%-5s%5ld`, which reproduces the tested alignment while
remaining deterministic for negative and multi-digit values. In the lobby,
use `TURN: NONE`. Keep all four color totals in the payload so a printed label
always describes the complete leaderboard, independent of live connectivity.

## Idempotency

Reuse `GameState::term` as the leaderboard boot generation. Leaderboard startup
already increments and synchronously persists it before normal operation, so a
new flash counter is unnecessary.

Construct the key as:

```text
scorebot:p1:<leader-id>:<term>:<game-id>:<version>:<sha256-of-request-body>
```

Properties:

- `p1` versions the client-side payload format.
- The full SHA-256 digest binds the key to the exact body and prevents the
  key/payload conflict encountered by the smoke test.
- A retry of the same captured state in one boot uses the same key and cannot
  dispatch a duplicate physical label.
- A committed state change changes `version` and the body digest.
- A reboot changes `term`, allowing the same game state to be intentionally
  printed once during the new boot generation.
- A job keeps its captured body and key even if gameplay changes while the
  network request is in flight.

Do one HTTP attempt per completed hold. After a lost response or other uncertain
outcome, another hold retries with the same key if the state is unchanged; the
printer service remains the authority for at-most-once dispatch.

## Architecture

### Gesture rules

Extend `MaintenanceAction` in `MaintenanceRules.hpp` with `Print` and add the
leaderboard all-button mask. Add a pure transition helper that can promote an
in-progress RESET/OTA gesture to PRINT. Update the button event path and
heartbeat in `GameState.cpp` to:

1. recognize or promote PRINT before retaining an existing two-button action;
2. reset the hold start time on promotion;
3. cancel on any missing required button;
4. enqueue one immutable print job at the hold threshold;
5. keep the full mask suppressed until release.

### Printer worker

Add a `PrinterClient` owned by `Coordinator`, with a capacity-one FreeRTOS queue
and a low-priority worker pinned to the application core. Create the worker only
for the leaderboard role. The dispatcher copies the captured POD print state
into the queue and returns immediately; JSON serialization, hashing, Wi-Fi, and
HTTP all happen in the worker without holding `stateMutex`.

The worker publishes only small atomic status/error enums and timestamps.
`Coordinator::loop()` owns display and light I/O, observes those atomics, and
restores the current state under the normal mutex when a terminal overlay
expires. The network worker never writes I2C displays directly.

Treat printer UI as an overlay with this priority:

1. restart/reset handoff;
2. active OTA;
3. active maintenance hold;
4. active printer transaction or terminal result;
5. normal game UI and brightness animations.

Normal state changes may continue while printing, but normal display writes
must not overwrite the printer overlay. Underlying turn color and display state
should still be updated in memory so restoration paints the newest state.
Block deep sleep while a print job, Wi-Fi shutdown, or terminal overlay is
active.

### Wi-Fi lifecycle

Use the successfully tested literal endpoint IP rather than `.local`, because
the printer network assigns the leaderboard to `192.168.6.0/24` while Home
Assistant is on `192.168.1.163`; multicast `.local` discovery does not cross
that boundary.

At leaderboard printer setup:

- disable Wi-Fi credential persistence and automatic reconnect;
- erase any station association retained by the smoke firmware;
- explicitly set Wi-Fi mode to off.

For each dequeued job:

1. set station mode and call `WiFi.begin()` with the generated credentials;
2. wait at most 15 seconds for an address while publishing `WIFI`;
3. configure 5-second TCP-connect and 15-second HTTP-response timeouts;
4. send the JSON with `Content-Type: application/json` and the generated
   `Idempotency-Key` while publishing `SEND`;
5. classify the response and retain only a bounded diagnostic/error code;
6. run one common cleanup path that calls `http.end()` when applicable,
   disables reconnect, disconnects the station, and sets `WIFI_OFF`;
7. publish `DONE` or the error only after shutdown is complete.

Avoid early returns after the radio is enabled; use a cleanup guard or a single
epilogue so every timeout and error powers Wi-Fi down. Verify that BLE links
remain responsive during the short Wi-Fi coexistence window.

### Configuration

Keep `.printer-wifi.json` ignored with exactly `ssid` and `password`. Reuse
`tools/printer_wifi.py` to generate a build-local header rather than exposing
the password in compiler flags. Apply the generator to production, debug, and
sleep-test firmware environments, and fail those builds with a clear message
when the local credential file is missing. Do not log the password.

Keep the endpoint and client payload version in a committed printer config
header because neither is secret. Add a checked-in example credential file
containing placeholders only.

After integration, remove the standalone `printer_smoke_test` environment and
`src/printer_smoke_test_main.cpp`; the production controller becomes the sole
implementation.

## Planned File Changes

- `lib/scorebot/src/MaintenanceRules.hpp`: add PRINT mask, precedence,
  transition, text, and color rules.
- `lib/scorebot/src/PrinterRules.hpp`: define captured state, deterministic
  payload fields, idempotency input, workflow enums, error mapping, scrolling,
  and UI timing as host-testable rules.
- `lib/scorebot/src/PrinterClient.hpp` and `src/PrinterClient.cpp`: queue,
  worker, SHA-256, Wi-Fi/HTTP request, status publication, and guaranteed radio
  cleanup.
- `lib/scorebot/src/GameState.hpp` and `src/GameState.cpp`: gesture promotion,
  immutable state capture, enqueue, suppression, and UI restoration hooks.
- `lib/scorebot/src/Coordinator.hpp` and `src/Coordinator.cpp`: own the printer,
  start its worker only on the leaderboard, render the overlay, gate normal
  display writes, and block sleep while printing.
- `tools/printer_wifi.py`, `platformio.ini`, `.gitignore`, and a new
  `.printer-wifi.example.json`: production credential generation and build
  wiring.
- `test/native/test_maintenance_rules.cpp`, a new
  `test/native/test_printer_rules.cpp`, and `test_runner.sh`: gesture,
  formatting, key, state-machine, timeout, and scroll tests.
- `README.md`: document the gesture, one-print-per-state-per-boot behavior,
  progress/error displays, Wi-Fi lifecycle, endpoint, and local credential
  setup.
- Remove `src/printer_smoke_test_main.cpp` and the temporary PlatformIO
  environment after production acceptance.

## Verification

### Native tests

- Every press ordering that reaches all four leaderboard buttons promotes to
  PRINT and never completes RESET or OTA.
- PRINT requires all four buttons for the entire five seconds; early release
  cancels and all component actions remain suppressed.
- Player-board chord behavior is unchanged.
- Payload formatting is exact for normal, negative, and maximum-width scores,
  active turns, and lobby state.
- Identical captured state and boot generation produce identical payloads and
  keys; changes to term, version, scores, turn, or payload schema change the
  key.
- Workflow transitions, UI precedence, result duration, five-second error
  expiry, early dismissal, and every scrolling error frame are deterministic
  and wrap-safe across `millis()` rollover.
- Queue-busy and all HTTP/Wi-Fi error classes map to bounded messages.

### Hardware acceptance

1. Boot the normal leaderboard firmware and confirm it neither associates with
   the configured printer network nor enables Wi-Fi before a print gesture.
2. Try multiple staggered press orders. Confirm `PRINT` replaces any initial
   RESET/OTA hint, early release is harmless, and a full hold executes only
   once.
3. Observe `WIFI`, `SEND`, and `DONE`, an HTTP 200 response, and one physical
   label matching the authoritative scores and turn.
4. Repeat without changing state. Confirm the same key returns an idempotent
   replay and the printer produces no second label.
5. Commit a score or turn change and confirm a new key and new label.
6. Reboot without changing the game and confirm the incremented `term` permits
   one new physical label.
7. Test a bad password, unavailable access point, unreachable server, request
   timeout, 409, and 5xx. Confirm the useful scrolling message lasts no more
   than five seconds and Wi-Fi is off afterward.
8. Keep player boards connected and active during a print. Confirm BLE state
   replication, input processing, persistence, and the restored display remain
   correct.
9. Confirm deep sleep cannot begin during a job but resumes normal eligibility
   after Wi-Fi shutdown and the result overlay.

## Acceptance Criteria

- The feature is present only on the leaderboard in the production controller
  firmware.
- All-four hold behavior is order-independent, cancel-safe, and cannot leak a
  RESET, OTA, score, or turn action.
- A print reflects one immutable authoritative state and uses a deterministic
  state-plus-boot-generation idempotency key.
- Network work never holds the game-state mutex or blocks the BLE/application
  loops.
- Wi-Fi is provably off before the gesture and after every success or failure.
- Progress is visible, errors are useful and bounded to five seconds, and the
  latest game UI is restored afterward.
- Repeated attempts cannot accidentally dispatch duplicate labels.
