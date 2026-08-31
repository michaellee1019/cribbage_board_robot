#!/usr/bin/env python3
"""Write a PlatformIO ESP32 firmware image to one or more armed BLE boards."""

import argparse
import asyncio
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from bleak import BleakClient, BleakScanner


SERVICE_UUID = "c6a8619e-2f9d-46bc-9a23-bb9c89a519be"
IDENTITY_UUID = "c6a8619f-2f9d-46bc-9a23-bb9c89a519be"
CONTROL_UUID = "c6a861b1-2f9d-46bc-9a23-bb9c89a519be"
DATA_UUID = "c6a861b2-2f9d-46bc-9a23-bb9c89a519be"
STATUS_UUID = "c6a861b3-2f9d-46bc-9a23-bb9c89a519be"
CHUNK_SIZE = 160  # Fits the firmware's 185-byte BLE MTU with protocol overhead.


@dataclass(frozen=True)
class DiscoveredBoard:
    device: object
    name: str


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
    discovered = await BleakScanner.discover(timeout=8.0, return_adv=True)
    # CoreBluetooth can report the same peripheral more than once in a scan.
    candidates = {}
    for device, advertisement in discovered.values():
        reported_name = advertisement.local_name or ""
        advertised_services = {uuid.casefold() for uuid in advertisement.service_uuids or []}
        if reported_name.startswith("Scorebot-") or SERVICE_UUID.casefold() in advertised_services:
            candidates[device.address] = (device, reported_name)

    boards = []
    for device, reported_name in candidates.values():
        name = reported_name
        if not name.startswith("Scorebot-"):
            # The local name may be omitted from an advertisement. The identity
            # characteristic is the canonical source for a board's stable name.
            try:
                async with BleakClient(device, timeout=10.0) as client:
                    identity = bytes(await client.read_gatt_char(IDENTITY_UUID))
                if len(identity) != 4:
                    raise RuntimeError(f"invalid identity length {len(identity)}")
                name = f"Scorebot-{int.from_bytes(identity, 'little'):x}"
            except Exception as error:
                print(f"Skipping {device.address}: could not read Scorebot identity ({error})")
                continue
        boards.append(DiscoveredBoard(device, name))

    return sorted(boards, key=lambda board: (board.name, board.device.address))


async def find_device(name: Optional[str]):
    candidates = await find_scorebot_devices()
    if name is not None:
        for board in candidates:
            if board.name.casefold() == name.casefold():
                return board
        raise RuntimeError(f"Could not find {name}. Ensure it is powered and nearby.")

    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        raise RuntimeError("Could not find a Scorebot board. Ensure it is powered and nearby.")
    names = ", ".join(sorted(board.name for board in candidates))
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
            raise RuntimeError("board is not locally armed; hold its rotary button until OTA appears")
        await client.write_gatt_char(CONTROL_UUID, f"START:{len(firmware)}".encode(), response=True)
        try:
            await wait_for_status(statuses, "READY", timeout=8.0)
        except RuntimeError as error:
            if "NOT_ARMED" in str(error):
                raise RuntimeError(
                    "Hold the board's rotary button until OTA appears, then retry within ten minutes."
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
        board = await find_device(name)
        print("Connected. The board must have been locally armed within the last ten minutes.")
        await write_firmware(board.device, firmware)
        print("Update accepted; the board is restarting into the new firmware.")
        return

    devices = await find_scorebot_devices()
    if not devices:
        raise RuntimeError("Could not find any Scorebot boards. Ensure they are powered and nearby.")
    updated = []
    skipped = []
    for board in devices:
        name = board.name
        print(f"\nUpdating {name}…")
        try:
            await write_firmware(board.device, firmware)
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
