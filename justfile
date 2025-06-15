# Justfile for cribbage_board_robot project
# Run commands with 'just <command>'

# List available commands
default:
    @just --list

# Build the project for a specific environment (default: controller)
build ENV="controller":
    pio run -e {{ENV}}

# Clean the build files
clean:
    pio run -t clean

# Upload firmware to device for a specific environment
upload ENV="controller":
    pio run -t upload -e {{ENV}}

# Upload and monitor device (same as run.sh)
run ENV="controller":
    pio run -t upload -e {{ENV}}
    pio device monitor -e {{ENV}} --quiet

# Run tests
test:
    pio test -e test

# Run specific test
test-specific TEST_NAME:
    pio test -e test --filter {{TEST_NAME}}

# Start the OTA update server
serve:
    node serve.js

# Build and start OTA server for over-the-air updates
ota ENV="controller":
    pio run -e {{ENV}}
    node serve.js

# Build and run tests, then build for controller
ci:
    pio test -e test
    pio run -e controller

# Generate compilation database (for code analysis tools)
compiledb:
    pio run --target compiledb

# Show memory usage
memory ENV="controller":
    pio run -e {{ENV}} --target size

# List connected devices
devices:
    pio device list

# Update PlatformIO and project dependencies
update:
    pio update
    pio pkg update

# Build for all environments
build-all:
    pio run

# Build for red environment
build-red:
    pio run -e red

# Build for blue environment
build-blue:
    pio run -e blue

# Upload to red device
upload-red:
    pio run -t upload -e red

# Upload to blue device
upload-blue:
    pio run -t upload -e blue

# Run red device
run-red:
    just run red

# Run blue device
run-blue:
    just run blue

# Full development workflow: build, test, and serve for OTA updates
dev ENV="controller":
    pio run -e {{ENV}}
    pio test -e test
    node serve.js
