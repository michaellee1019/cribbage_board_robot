#!/usr/bin/env python3
"""Write a PlatformIO ESP32 firmware image to one or more armed BLE boards."""

import argparse
import asyncio
from pathlib import Path
from typing import Optional

from bleak import BleakClient, BleakScanner


CONTROL_UUID = "c6a861b1-2f9d-46bc-9a23-bb9c89a519be"
DATA_UUID = "c6a861b2-2f9d-46bc-9a23-bb9c89a519be"
STATUS_UUID = "c6a861b3-2f9d-46bc-9a23-bb9c89a519be"
CHUNK_SIZE = 160  # Fits the firmware's 185-byte BLE MTU with protocol overhead.


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    target = parser.add_mutually_exclusive_group()
    target.add_argument("--id", help="hex board id shown in its BLE name (Scorebot-<id>)")
    target.add_argument("--name", help="complete advertised BLE device name")
    target.add_argument("--all", action="store_true", help="update every discoverable Scorebot board")
    parser.add_argument("firmware", type=Path, help="compiled .bin image")
    return parser.parse_args()


async def find_scorebot_devices():
    print("Searching for Scorebot boards…")
    devices = await BleakScanner.discover(timeout=8.0)
    # CoreBluetooth can report the same peripheral more than once in a scan.
    candidates = {device.address: device for device in devices if (device.name or "").startswith("Scorebot-")}
    return sorted(candidates.values(), key=lambda device: (device.name or "", device.address))


async def find_device(name: Optional[str]):
    candidates = await find_scorebot_devices()
    if name is not None:
        for device in candidates:
            if (device.name or "").casefold() == name.casefold():
                return device
        raise RuntimeError(f"Could not find {name}. Ensure it is powered and nearby.")

    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        raise RuntimeError("Could not find a Scorebot board. Ensure it is powered and nearby.")
    names = ", ".join(sorted(device.name or "unknown" for device in candidates))
    raise RuntimeError(f"Found multiple boards ({names}). Run `just flash <board-id>` to select one.")


async def wait_for_status(statuses: asyncio.Queue[str], expected: str, timeout: float) -> None:
    while True:
        status = await asyncio.wait_for(statuses.get(), timeout=timeout)
        print(f"Board: {status}")
        if status == expected:
            return
        if status.startswith("ERR:"):
            raise RuntimeError(f"Board rejected the update: {status}")


async def write_firmware(device, firmware: bytes) -> None:
    statuses: asyncio.Queue[str] = asyncio.Queue()

    def on_status(_: int, value: bytearray) -> None:
        statuses.put_nowait(bytes(value).decode("ascii", errors="replace"))

    async with BleakClient(device, timeout=15.0) as client:
        await client.start_notify(STATUS_UUID, on_status)
        status = bytes(await client.read_gatt_char(STATUS_UUID)).decode("ascii", errors="replace")
        if status != "ARMED":
            raise RuntimeError("board is not locally armed; hold its rotary button for three seconds")
        await client.write_gatt_char(CONTROL_UUID, f"START:{len(firmware)}".encode(), response=True)
        try:
            await wait_for_status(statuses, "READY", timeout=8.0)
        except RuntimeError as error:
            if "NOT_ARMED" in str(error):
                raise RuntimeError(
                    "Hold and release the board's rotary button for three seconds, then retry within ten minutes."
                ) from error
            raise

        for offset in range(0, len(firmware), CHUNK_SIZE):
            await client.write_gatt_char(DATA_UUID, firmware[offset : offset + CHUNK_SIZE], response=True)
            progress = min(offset + CHUNK_SIZE, len(firmware)) * 100 // len(firmware)
            print(f"\rUploading: {progress:3d}%", end="", flush=True)
        print()
        await client.write_gatt_char(CONTROL_UUID, b"COMMIT", response=True)
        await wait_for_status(statuses, "DONE", timeout=10.0)


async def main() -> None:
    args = parse_args()
    firmware = args.firmware.read_bytes()
    if not firmware:
        raise RuntimeError("Firmware image is empty.")
    if not args.all:
        name = args.name or (f"Scorebot-{args.id}" if args.id else None)
        device = await find_device(name)
        print("Connected. The board must have been locally armed within the last ten minutes.")
        await write_firmware(device, firmware)
        print("Update accepted; the board is restarting into the new firmware.")
        return

    devices = await find_scorebot_devices()
    if not devices:
        raise RuntimeError("Could not find any Scorebot boards. Ensure they are powered and nearby.")
    updated = []
    skipped = []
    for device in devices:
        name = device.name or device.address
        print(f"\nUpdating {name}…")
        try:
            await write_firmware(device, firmware)
        except Exception as error:  # Continue so every armed board gets its chance.
            print(f"Skipped {name}: {error}")
            skipped.append(name)
        else:
            updated.append(name)
            print(f"Updated {name}; it is restarting.")
    print(f"\nUpdated {len(updated)} board(s).")
    if skipped:
        raise RuntimeError("Not updated: " + ", ".join(skipped))


if __name__ == "__main__":
    asyncio.run(main())
