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

# Flash one directly connected ESP32-S3 board. With multiple supported boards,
# pass the same eight-digit id used by the Scorebot BLE device name.
usb-flash device_id="":
    #!/usr/bin/env bash
    set -euo pipefail

    # Finish the potentially slow build before asking the user to wake a board.
    pio run -e controller

    timeout_seconds="${SCOREBOT_USB_FLASH_TIMEOUT_SECONDS:-120}"
    echo "Waiting up to ${timeout_seconds} seconds for a stable ESP32-S3 USB connection."
    echo "If the board is asleep, press any board control once to wake it."
    echo "If no port appears, use a data-capable cable, then hold BOOT, tap RESET, and release BOOT."
    port="$(python3 tools/usb_port.py --id "{{ device_id }}" --timeout "$timeout_seconds")"

    echo "Flashing ${port} at the configured reliable upload speed..."
    if ! pio run -e controller -t upload --upload-port "$port"; then
        echo "USB flash failed. Close any serial monitor and retry." >&2
        echo "For recovery: hold BOOT, tap RESET, release BOOT, then rerun this command." >&2
        exit 1
    fi
    echo "USB flash complete. If BOOT+RESET recovery was used and the app does not start, tap RESET once."

# Build only.
build:
    pio run -e controller

# Run the lightweight rules tests without hardware.
test:
    ./test_runner.sh logic
