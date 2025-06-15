# Cribbage Board Robot

ESP32-based mesh networked cribbage scoring system with multiple player devices and a leaderboard display.

## Quick Start

### Setup
```bash
brew install platformio
```

### Build and Upload
```bash
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
- **Architecture**: Mesh network using painlessMesh library
- **Devices**: Multiple player scoring units + leaderboard controller
- **Hardware**: 7-segment displays, rotary encoders, button grids, LEDs

See [CLAUDE.md](CLAUDE.md) for detailed development documentation.

## TODO Features

- SOS light when idle
- IR receiver for configuration
- Brightness control based on turn/winning status
- Score commitment vs turn passing logic
- Leaderboard buttons functionality

## Hardware Notes

- Put LED on PWM-enabled pin for dimming (A0-A7)
- Custom board definition in `boards/seeed_xiao_esp32s3.json`