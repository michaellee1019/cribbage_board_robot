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

# Flash one directly connected board using only macOS USB serial devices.
usb-flash:
    #!/usr/bin/env bash
    set -euo pipefail

    timeout_seconds="${SCOREBOT_USB_FLASH_TIMEOUT_SECONDS:-120}"
    deadline=$((SECONDS + timeout_seconds))
    echo "Waiting up to ${timeout_seconds} seconds for a USB serial board..."

    while true; do
        ports=()
        while IFS= read -r port; do
            ports+=("$port")
        done < <(find /dev -maxdepth 1 -type c \( \
            -name 'cu.usbmodem*' -o \
            -name 'cu.usbserial*' -o \
            -name 'cu.wchusbserial*' -o \
            -name 'cu.SLAB_USBtoUART*' \
        \) -print | sort)

        if (( ${#ports[@]} == 1 )); then
            break
        fi
        if (( ${#ports[@]} > 1 )); then
            echo "Found multiple USB serial devices; unplug all but the board to flash:" >&2
            printf '  %s\n' "${ports[@]}" >&2
            exit 1
        fi
        if (( SECONDS >= deadline )); then
            echo "No USB serial board appeared within ${timeout_seconds} seconds." >&2
            exit 1
        fi
        sleep 0.25
    done

    echo "Flashing ${ports[0]}"
    pio run -e controller -t upload --upload-port "${ports[0]}"

# Build only.
build:
    pio run -e controller

# Run the lightweight rules tests without hardware.
test:
    ./test_runner.sh logic
