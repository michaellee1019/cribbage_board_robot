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

# First install or USB recovery for a directly connected board.
just usb-flash

# Upload to specific device using convenience script
./run.sh red         # Upload to red player device
./run.sh blue        # Upload to blue player device  
./run.sh controller  # Upload to controller/leaderboard

# Or use PlatformIO directly
pio run -t upload -t monitor -e red
pio run -t upload -t monitor -e blue
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

- Every accepted score or turn action is an atomic, versioned commit on the leaderboard.
- The committed snapshot is persisted locally and replicated to connected player boards. Each board retains its latest snapshot in flash, so a rebooted board reconnects and resumes the game state.
- Player requests carry a monotonic operation ID, so repeated notifications are ignored. Scoring and turn changes are separate permissions: every connected rostered player may score, while only the active player may advance the turn.
- **OK** always commits the visible score delta. For the active player it also passes the turn; out of turn it commits the score without changing the turn. This lets accidental scoring be corrected without weakening turn control.
- If the leaderboard is unavailable, player boards always show `PAIR` and reject new game actions. They resume automatically when the leaderboard reconnects; this prevents competing versions of the score while preserving the last committed state.

### Game controls

- In the lobby, the leaderboard shows the names of connected players. Press its **ADD/Start** button to freeze that roster and start with Red, or the first connected color after Red.
- A paired player in the lobby shows idle dashes. During gameplay, **-1**, **+1**, **+5**, or the rotary encoder build a visible score delta. **ADD** submits it without changing the turn. **OK** also submits it; if that player owns the turn, OK advances it and clears `GO` and the turn light immediately while the leaderboard confirms the operation.
- During gameplay, a temporarily disconnected rostered board alternates `PAIR` with its saved score until it rejoins. Player positions that were not in the frozen starting roster stay blank.
- A temporary disconnect or deep-sleep cycle never advances the turn. If the active board is asleep, the game waits for that same board to rejoin.
- The active player's `GO` display continuously fades in for one second, holds for one second, and fades out for one second while its dim turn light fades inversely. Player interaction wakes the leaderboard displays and turn light; after five quiet seconds they fade to their minimum visible levels.
- On dual-core ESP32-S3 boards, the Bluetooth controller and NimBLE host are pinned to radio Core 0. The Arduino loop, UI dispatcher, I2C input/display work, and non-blocking fades run on application Core 1; BLE callbacks only copy gameplay messages into the application queue.
- During a game, briefly press the leaderboard's rotary encoder to zero every score and return to an open lobby. Holding it for three seconds still arms OTA. Persisted scores survive ordinary power cycles until this explicit reset.

## BLE OTA updates

After initially flashing firmware by USB, an individual board can be updated from the laptop without joining a Wi-Fi network:

1. Hold that board's rotary button for at least three seconds. Its display shows `OTA ` as soon as the hold is recognized and opens a ten-minute update window.
2. On the laptop, run `just flash <board-id>` (the board advertises as `Scorebot-<board-id>`). If it is the only Scorebot board nearby, `just flash` selects it automatically.

To update every board at once, locally arm each board first and run `just flash-all`. The laptop updates them sequentially and reports any board that was not armed or could not be reached.

The board accepts an update only during that local window. This physical-presence gate prevents a nearby BLE device from starting an update by itself.

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
