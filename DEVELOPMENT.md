# Scorebot development guide

ESP32-based BLE scoring system with player boards and a leaderboard.

## Project Overview

- **Platform**: ESP32 (Seeed XIAO ESP32S3)
- **Framework**: Arduino/PlatformIO
- **Architecture**: BLE star, with the leaderboard acting as central
- **Devices**: Multiple player scoring units + leaderboard controller
- **Hardware**: 7-segment displays, rotary encoders, button grids, LEDs

## Quick Start

```bash
# Setup
brew install platformio

# USB flash the one directly connected board
just usb-flash

# Build production firmware without uploading
just build
```

## Development Environment

### PlatformIO Environments
- `controller` - Production firmware; hardware identity selects leaderboard/player role
- `debug` - Production behavior with application diagnostics enabled
- `sleep_test` - Hardware validation build with a 15-second sleep timeout
- `test_embedded` - Unity integration tests on ESP32 hardware

### Key Libraries
- `NimBLE-Arduino` - Low-power BLE central/peripheral transport
- `Wire` - I2C communication
- `Adafruit seesaw Library` - Hardware interface
- `Adafruit MCP23017` - GPIO expander
- `SparkFun Qwiic Alphanumeric Display` - Display driver

## Architecture

### Core Components
- **Coordinator** (`src/Coordinator.cpp`) - Main orchestrator, handles events and coordinates all subsystems
- **GameState** (`src/GameState.cpp`) - Manages scoring state and turn logic
- **MyBle** (`src/MyBle.cpp`) - BLE star transport and device communication
- **ButtonGrid** (`src/ButtonGrid.cpp`) - Input handling from button matrix
- **RotaryEncoder** (`src/RotaryEncoder.cpp`) - Score input via rotary encoder
- **HT16Display** (`src/HT16Display.cpp`) - 7-segment display management
- **ErrorHandler** (`lib/scorebot/src/ErrorHandler.hpp`) - Idiomatic ESP32/FreeRTOS error handling with logging and controlled restarts

### Event System
- FreeRTOS queue-based event handling
- Events defined in `lib/scorebot/src/Event.hpp`
- Coordinator dispatches events to appropriate handlers

## Common Development Tasks

### Building & Uploading
```bash
# Build only
just build

# USB upload
just usb-flash

# BLE upload to a physically armed board
just flash <board-id>
```

### Testing
```bash
# Run logic tests (no hardware needed)
./test_runner.sh logic
# or
just test

# Run tests on ESP32 hardware
pio test -e test_embedded

# Run specific test file
pio test -e test_embedded -f test_integration_error_handler

# Run tests with verbose output
pio test -e test_embedded -v

# Debug project configuration
pio project config --json-output
pio project metadata --json-output -e <environment>
```

#### Error Handling Tests
The ErrorHandler system includes both logic and integration testing:

- **Logic Tests**: Test error handling logic with no hardware required (built into test runner)
- **Integration Tests** (`test/test_integration_error_handler/test_main.cpp`): Test real FreeRTOS resource creation, memory allocation, and thread safety on ESP32

**Running Error Handler Tests:**
```bash
# Quick logic tests (no hardware required)
./test_runner.sh logic

# Error handling integration tests (requires ESP32 hardware)
pio test -e test_embedded -f test_integration_error_handler

# Both logic and integration tests
./test_runner.sh error-handler
```

**Regression Testing:**
Run these tests after any changes to error handling code:
```bash
# Quick logic validation (no hardware)
./test_runner.sh logic

# Full test suite on ESP32
pio test -e test_embedded

# Or use the convenience script
./test_runner.sh all
```

#### Test Runner Script
For convenience, use `./test_runner.sh`:
```bash
./test_runner.sh                 # Run integration tests (default, requires hardware)
./test_runner.sh logic           # Logic tests only (fast, no hardware needed)
./test_runner.sh embedded        # Integration tests (requires hardware)  
./test_runner.sh error-handler   # Error handling tests (logic + integration)
./test_runner.sh all            # All tests (logic + integration)
```

**Note**: The logic tests use the actual `ErrorHandler.hpp` code with platform abstraction, ensuring no code duplication while enabling fast testing without hardware.

### Code Formatting
```bash
# Format all C++ files
clang-format -i ./**/*.{hpp,cpp}
```

### Debugging
- Hardware debugger: `esp-prog`
- Debug speed: 2000 (reduced for stability)
- Monitor filters: `esp32_exception_decoder`
- Production debug level: errors; the `debug` environment enables application diagnostics

## Known Issues & Considerations

### Type System Issues
⚠️ **Integer Type Mixing**: Libraries use various integer types (uint8_t, uint32_t, int) which can cause casting issues. Be explicit about types, especially:
- Peer IDs: `uint32_t` (not `uint8_t` - compiler won't help!)
- Array indices and sizes
- Display values and coordinates

### Common Pitfalls
- Check stable chip-ID/role assignments when provisioning new boards
- Verify display coordinate types match library expectations
- Use explicit casts when necessary and document why

## Hardware Configuration

### Device Types
- **Player Units**: Score input (rotary encoder, buttons), local display
- **Leaderboard**: Central display showing all player scores
- **BLE Star**: Devices communicate through the leaderboard; unpaired player boards show `PAIR`

USB ports are discovered at flash time. `just usb-flash` ignores unrelated
serial devices and accepts a board ID when multiple Scorebot boards are connected.

## TODO Features
- SOS light when idle
- IR receiver for configuration
- Brightness control based on turn/winning status

## Files to Check
- `platformio.ini` - Build configuration and environments
- `src/main.cpp` - Entry point (minimal, delegates to Coordinator)
- `src/BoardRole.cpp` - Stable device-ID-to-role assignments
- `lib/scorebot/src/` - Header files with class definitions
