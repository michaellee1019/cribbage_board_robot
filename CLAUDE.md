# Cribbage Board Robot

ESP32-based mesh networked cribbage scoring system with multiple player devices and a leaderboard display.

## Project Overview

- **Platform**: ESP32 (Seeed XIAO ESP32S3)
- **Framework**: Arduino/PlatformIO
- **Architecture**: Mesh network using painlessMesh library
- **Devices**: Multiple player scoring units + leaderboard controller
- **Hardware**: 7-segment displays, rotary encoders, button grids, LEDs

## Quick Start

```bash
# Setup
brew install platformio

# Build and upload to specific device
./run.sh red    # Upload to red player device
./run.sh blue   # Upload to blue player device  
./run.sh controller  # Upload to controller/leaderboard

# Alternative direct commands
pio run -t upload -t monitor -e red
pio run -t upload -t monitor -e blue
```

## Development Environment

### PlatformIO Environments
- `red` - Player device (red)
- `blue` - Player device (blue) 
- `controller` - Leaderboard/controller device

### Key Libraries
- `painlessMesh` - Mesh networking
- `Wire` - I2C communication
- `Adafruit seesaw Library` - Hardware interface
- `Adafruit MCP23017` - GPIO expander
- `SparkFun Qwiic Alphanumeric Display` - Display driver

## Architecture

### Core Components
- **Coordinator** (`src/Coordinator.cpp`) - Main orchestrator, handles events and coordinates all subsystems
- **GameState** (`src/GameState.cpp`) - Manages scoring state and turn logic
- **MyWifi** (`src/MyWifi.cpp`) - Mesh networking and device communication
- **ButtonGrid** (`src/ButtonGrid.cpp`) - Input handling from button matrix
- **RotaryEncoder** (`src/RotaryEncoder.cpp`) - Score input via rotary encoder
- **HT16Display** (`src/HT16Display.cpp`) - 7-segment display management

### Event System
- FreeRTOS queue-based event handling
- Events defined in `lib/scorebot/src/Event.hpp`
- Coordinator dispatches events to appropriate handlers

## Common Development Tasks

### Building & Uploading
```bash
# Quick upload and monitor
./run.sh <environment>

# Build only
pio run -e <environment>

# Clean build
pio run -t clean -e <environment>

# Upload without monitor
pio run -t upload -e <environment>
```

### Testing
```bash
# Run unit tests (needs setup)
pio test -e <environment>

# Debug project configuration
pio project config --json-output
pio project metadata --json-output -e <environment>
```

### Code Formatting
```bash
# Format all C++ files
clang-format -i ./**/*.{hpp,cpp}
```

### Debugging
- Hardware debugger: `esp-prog`
- Debug speed: 2000 (reduced for stability)
- Monitor filters: `esp32_exception_decoder`
- Debug level: `CORE_DEBUG_LEVEL=5`

## Known Issues & Considerations

### Type System Issues
⚠️ **Integer Type Mixing**: Libraries use various integer types (uint8_t, uint32_t, int) which can cause casting issues. Be explicit about types, especially:
- Peer IDs: `uint32_t` (not `uint8_t` - compiler won't help!)
- Array indices and sizes
- Display values and coordinates

### Common Pitfalls
- Check peer type consistency in mesh networking code
- Verify display coordinate types match library expectations
- Use explicit casts when necessary and document why

## Hardware Configuration

### Device Types
- **Player Units**: Score input (rotary encoder, buttons), local display
- **Leaderboard**: Central display showing all player scores
- **Mesh Network**: Devices communicate game state changes

### Ports (varies by device)
```ini
# Red device
upload_port = /dev/cu.usbmodem101
monitor_port = /dev/cu.usbmodem101

# Blue device  
upload_port = /dev/cu.usbmodem2101
monitor_port = /dev/cu.usbmodem2101
```

## TODO Features
- SOS light when idle
- IR receiver for configuration
- Brightness control based on turn/winning status
- Score commitment vs turn passing
- Leaderboard buttons

## Files to Check
- `platformio.ini` - Build configuration and environments
- `src/main.cpp` - Entry point (minimal, delegates to Coordinator)
- `secret.h` - WiFi credentials and device-specific config
- `lib/scorebot/src/` - Header files with class definitions