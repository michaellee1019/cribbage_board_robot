# Scorebot

ESP32-based BLE scoring system with battery-powered player boards and a leaderboard.

## Quick Start

### Setup
```bash
brew install platformio
```

### Build and Upload
```bash
# Update a physically armed board over the Mac's Bluetooth connection. With
# multiple boards nearby, specify its BLE id shown as Scorebot-<id>.
brew install just uv
just flash <board-id>

# First install or USB recovery for a directly connected board. If more than
# one supported board is connected, pass its eight-digit Scorebot id.
just usb-flash
just usb-flash <board-id>

# Build without uploading.
just build
```

### Testing
```bash
# Quick logic tests (no hardware needed)
./test_runner.sh logic

# Integration tests (requires ESP32 hardware)
./test_runner.sh embedded

# Error handling tests specifically
./test_runner.sh error-handler

# All tests
./test_runner.sh all
```

## Development

### Code Formatting
```bash
clang-format -i ./**/*.{hpp,cpp}
```

### Debugging
```bash
pio project config --json-output
pio project metadata --json-output -e <environment>
```

## Architecture

- **Platform**: ESP32 (Seeed XIAO ESP32S3)
- **Framework**: Arduino/PlatformIO  
- **Architecture**: BLE star — the leaderboard is the central and player boards are peripherals
- **Devices**: Multiple player scoring units + leaderboard controller
- **Hardware**: 7-segment displays, rotary encoders, button grids, LEDs

See [DEVELOPMENT.md](DEVELOPMENT.md) for detailed development documentation.

## Reliability and recovery

- Every accepted score or turn action is an atomic, versioned commit on the leaderboard. Its display, controls, turn light, and player-board broadcast update immediately; an immutable copy is handed to a low-priority persistence worker so flash I/O never holds the gameplay/UI state lock. Bursts are coalesced to the latest recoverable snapshot, and sleep waits for the worker to finish.
- Leaderboard-entered actions never wait for a player-board acknowledgment.
- Fixed board identities are checked against each peer's factory-derived public Bluetooth address; payloads cannot self-assert another configured color or the leaderboard role.
- The committed snapshot is persisted locally and replicated to connected player boards. Each board retains its latest snapshot in flash, so a rebooted board reconnects and resumes the game state.
- Player requests carry a monotonic operation ID, so repeated notifications are ignored. Scoring and turn changes are separate permissions: every connected rostered player may score, while only the active player may advance the turn.
- **OK** always commits the visible score delta. For the active player it also passes the turn; out of turn it commits the score without changing the turn. This lets accidental scoring be corrected without weakening turn control.
- If the leaderboard is unavailable, player boards always show `PAIR` and reject new game actions. They resume automatically when the leaderboard reconnects; this prevents competing versions of the score while preserving the last committed state.

### Game controls

- In a clean lobby, every unavailable color defaults to `OFF`; an untouched color automatically shows its name and becomes available when that board connects, then returns to `OFF` if it disconnects. For example, Blue as the only connected board displays `[OFF, BLUE, OFF, OFF]`. Click the leaderboard encoder to advance through Red, Blue, Green, and White; the selected display blinks. Rotate the encoder in either direction to explicitly choose `LOCAL`, physical-board mode (the color name when connected, otherwise `PAIR`), or `OFF` for that color. Explicit choices survive connection changes: `PAIR` waits for that board, while `OFF` excludes even a connected board. The leaderboard's **OK** button freezes the enabled, available colors and starts the game; its other buttons have no single-button lobby action.
- During a game, the leaderboard automatically selects the latest authoritative turn, updates the turn-color light synchronously, and pulses that display. The active leaderboard and player-board displays alternate once per second between `GO` and that player's authoritative total score, beginning with `GO`. The leaderboard can operate every color in the frozen roster whether that player's board is connected, sleeping, or showing `PAIR`; click its encoder to cycle through roster colors for corrections. Any subsequent turn change immediately returns the light, selection, and controls to the new active color. Its four buttons are **-1**, **+1**, **ADD**, and **OK**; the encoder supplies arbitrary deltas in place of a dedicated **+5** button. Physical-board and leaderboard actions share the same authoritative score and turn state.
- A paired player in the lobby shows idle dashes. During gameplay, **-1**, **+1**, **+5**, or the rotary encoder build a visible score delta. **ADD** submits it without changing the turn. **OK** also submits it; if that player owns the turn, OK advances it and clears `GO` and the turn light immediately while the leaderboard confirms the operation.
- During gameplay, an unexpectedly disconnected rostered board alternates `PAIR` with its saved score until it rejoins. Player positions that were not in the frozen starting roster stay blank.
- A temporary disconnect or deep-sleep cycle never advances the turn. A player sends three sleep notices before turning off its Bluetooth radio, so during a game the leaderboard alternates `ZZZZ` with the saved score for an intentionally sleeping board and reserves `PAIR` for an unexpected disconnect. Sleeping players retain their place in the frozen turn order; when their turn arrives, the game waits for them to rejoin.
- Player boards enter deep sleep after five idle minutes and the leaderboard after ten; either waits twenty minutes when it contains a locally entered score that has not been submitted. The first local control action on a sleeping board is wake-only: it wakes and rejoins without also changing a score or starting/resetting a game. The leaderboard and player boards are woken independently.
- The active player's `GO` display continuously fades in for one second, holds for one second, and fades out for one second while its dim turn light fades inversely. While the leaderboard is awake, player interaction wakes its displays and turn light; after five quiet seconds they fade to their minimum visible levels. Deep-sleeping boards are woken independently with a local control press.
- On dual-core ESP32-S3 boards, the Bluetooth controller and NimBLE host are pinned to radio Core 0. The Arduino loop, high-priority FIFO gameplay/input dispatcher, low-priority persistence worker, OTA flash worker, I2C input/display work, and non-blocking fades run on application Core 1. BLE callbacks only copy gameplay and OTA requests into bounded queues. OTA sends one durable-consumption credit after each flash chunk, preventing the laptop from outrunning the worker. Established game links use 15–30 ms connection intervals with zero slave latency; deep sleep supplies battery savings instead of delaying interactive traffic.
- Maintenance actions use deliberate five-second button chords on either board type. Hold **-1** and **+1** to scroll `RESET`, flash red progressively faster, and clear the current board's stored state. Hold **ADD** and **OK** to show `OTA`, flash purple progressively faster, and arm OTA. Releasing either chord early cancels it without performing the component button actions. Resetting a player clears only its local replica before it rejoins; resetting the leaderboard clears the authoritative game and controller profile while preserving a monotonic reset epoch for safe peer resynchronization. The leaderboard drains the authoritative reset snapshot and services the radio before rebooting, so connected players see the cleared epoch immediately.

### Leaderboard printing

The leaderboard can print a snapshot of the current authoritative scores and turn. Hold all four leaderboard buttons continuously for five seconds. `PRINT` appears immediately and the light flashes cyan progressively faster; releasing any button early cancels the gesture without applying any of the four component actions. If the press begins as a RESET or OTA chord, adding the other two buttons promotes it to PRINT and restarts the five-second hold.

After the hold completes, the displays show `WIFI` while the leaderboard joins the printer network, `SEND` while it submits the job to `http://192.168.1.163:8099/text/print`, and `DONE` for about two seconds after success. Failures scroll a short message such as `WIFI FAIL`, `NO ROUTE`, `PRINT TIMEOUT`, or an HTTP status class for no more than five seconds. A normal input dismisses a result message but is consumed, so it cannot change the game invisibly.

Each job captures immutable committed game state at the end of the hold. Its idempotency key binds the exact request body to the leaderboard identity, game version, and boot generation. Repeating the gesture without a state change during the same boot safely retries the same job and does not produce another physical label; a committed game change or reboot permits a new one.

Wi-Fi is disabled at boot and remains off during normal play. The leaderboard enables it only for a print attempt and explicitly turns it off again after every success, timeout, or other failure. Gameplay, persistence, and BLE state updates continue while the temporary print display is active.

Firmware builds generate a private header from `.printer-wifi.json`. Copy the checked-in example, then edit only its two string values:

```bash
cp .printer-wifi.example.json .printer-wifi.json
```

The required fields are exactly `ssid` and `password`. The local file is ignored by Git, its values are not placed in compiler flags or build logs, and the generated header lives only under the build output. The credentials are compiled into firmware so the leaderboard can connect without storing them separately at runtime.

## USB flashing

Use a data-capable USB-C cable and run `just usb-flash`. The command builds the firmware before waiting for a board, ignores unrelated serial devices, and starts the upload only after the same supported ESP32-S3 USB port is visible in two consecutive checks. If multiple Scorebot boards are connected, use `just usb-flash <board-id>` with the eight hexadecimal digits from its `Scorebot-<board-id>` name.

An awake board connected to an active USB computer will not enter deep sleep, even when no serial monitor is open. If the board was already asleep when the cable was connected, press any board control once; that first action remains wake-only, and the USB connection will then keep the board awake for flashing.

A charging-only cable, wall charger, or suspended computer does not provide a detectable active USB data connection. If ordinary enumeration fails, close any serial monitor, hold **BOOT**, tap **RESET**, then release **BOOT** and rerun the command. A board placed into download mode manually may need one final **RESET** tap after the write completes. The upload remains limited to 57,600 baud because that setting is more reliable through multi-port USB-C hubs.

## BLE OTA updates

After initially flashing firmware by USB, an individual board can be updated from the laptop without joining a Wi-Fi network:

1. Hold that board's **ADD** and **OK** buttons together for five seconds. Its displays immediately switch to full-brightness `OTA ` and its light flashes purple progressively faster; after five seconds it opens a ten-minute update window.
2. On the laptop, run `just flash <board-id>` (the board advertises as `Scorebot-<board-id>`). If it is the only Scorebot board nearby, `just flash` selects it automatically.

To update every board at once, locally arm each board first and run `just flash-all`. The laptop updates them sequentially and reports any board that was not armed or could not be reached.

The board accepts an update only during that local window. OTA is an exclusive mode: normal game traffic, player pairing, input, display fading, and deep sleep remain suspended for the entire window. Bluetooth runs at maximum transmit power, the light stays solid purple while waiting, and it blinks purple while firmware is being received. Normal pairing and UI behavior resume if the window expires or a transfer aborts. This physical-presence gate prevents a nearby BLE device from starting an update by itself.

On macOS, allow the terminal application running `just` to use Bluetooth if macOS asks. The update script uses the operating system's native CoreBluetooth support through Bleak; it does not join the game BLE network or require Wi-Fi.

## Wire protocol

The BLE service and every game message carry a wire-protocol version. A leaderboard refuses to add a player board with a different version; snapshots and score requests with a mismatched or absent version are ignored. Update all boards with `just flash-all` before expecting them to participate in the same game.

## TODO Features

- SOS light when idle
- IR receiver for configuration
- Brightness control based on turn/winning status

## Hardware Notes

- Put LED on PWM-enabled pin for dimming (A0-A7)
- Custom board definition in `boards/seeed_xiao_esp32s3.json`
