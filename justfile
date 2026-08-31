default:
    @just --list

# Build and update one board through the Mac's native Bluetooth stack. With no
# id, this proceeds only if exactly one Scorebot board is visible.
flash device_id="":
    pio run -e controller
    if [ -n "{{ device_id }}" ]; then uv run --with bleak python3 tools/ota_ble.py --id "{{ device_id }}" .pio/build/controller/firmware.bin; else uv run --with bleak python3 tools/ota_ble.py .pio/build/controller/firmware.bin; fi

# Discover and sequentially update every locally armed Scorebot board.
flash-all:
    pio run -e controller
    uv run --with bleak python3 tools/ota_ble.py --all .pio/build/controller/firmware.bin

# First-install/recovery path for a directly connected board over USB.
usb-flash:
    pio run -e controller -t upload

# Build only.
build:
    pio run -e controller

# Run the lightweight rules tests without hardware.
test:
    ./test_runner.sh logic
