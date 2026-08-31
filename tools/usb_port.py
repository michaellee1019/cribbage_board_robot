#!/usr/bin/env python3
"""Find one stable ESP32-S3 native USB serial port for flashing."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import Iterable, Sequence


SUPPORTED_USB_IDS = frozenset({"303A:1001", "2886:0056", "2886:8056"})
USB_ID_PATTERN = re.compile(r"VID:PID=([0-9A-Fa-f]{4}:[0-9A-Fa-f]{4})")
SERIAL_PATTERN = re.compile(r"(?:^|\s)SER=([^\s]+)")


@dataclass(frozen=True)
class UsbBoard:
    port: str
    usb_id: str
    serial: str
    board_id: str
    description: str


def board_id_from_serial(serial: str) -> str:
    mac = "".join(character for character in serial if character in "0123456789abcdefABCDEF")
    return mac[-8:].lower() if len(mac) == 12 else ""


def recognized_boards(devices: Iterable[dict[str, str]]) -> list[UsbBoard]:
    boards: list[UsbBoard] = []
    for device in devices:
        hardware_id = str(device.get("hwid", ""))
        usb_match = USB_ID_PATTERN.search(hardware_id)
        if usb_match is None:
            continue
        usb_id = usb_match.group(1).upper()
        if usb_id not in SUPPORTED_USB_IDS:
            continue
        serial_match = SERIAL_PATTERN.search(hardware_id)
        serial = serial_match.group(1) if serial_match is not None else ""
        boards.append(
            UsbBoard(
                port=str(device.get("port", "")),
                usb_id=usb_id,
                serial=serial,
                board_id=board_id_from_serial(serial),
                description=str(device.get("description", "ESP32-S3 USB")),
            )
        )
    return sorted((board for board in boards if board.port), key=lambda board: board.port)


def select_board(boards: Sequence[UsbBoard], requested_id: str) -> UsbBoard | None:
    normalized_id = requested_id.lower().removeprefix("0x")
    if normalized_id:
        matches = [board for board in boards if board.board_id == normalized_id]
        return matches[0] if len(matches) == 1 else None
    return boards[0] if len(boards) == 1 else None


def list_devices() -> list[dict[str, str]]:
    result = subprocess.run(
        ["pio", "device", "list", "--json-output"],
        check=False,
        capture_output=True,
        text=True,
    )
    try:
        devices = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        detail = result.stderr.strip() or result.stdout.strip() or "no output"
        raise RuntimeError(f"PlatformIO device discovery failed: {detail}") from error
    if not isinstance(devices, list):
        raise RuntimeError("PlatformIO returned an unexpected device list")
    return devices


def describe(board: UsbBoard) -> str:
    identity = f" board-id={board.board_id}" if board.board_id else ""
    serial = f" serial={board.serial}" if board.serial else ""
    return f"{board.port} ({board.usb_id}{identity}{serial})"


def wait_for_board(requested_id: str, timeout_seconds: float) -> UsbBoard:
    deadline = time.monotonic() + timeout_seconds
    previous: UsbBoard | None = None
    ambiguous_reported = False

    while True:
        boards = recognized_boards(list_devices())
        selected = select_board(boards, requested_id)
        if selected is not None and selected == previous:
            return selected
        previous = selected

        if not requested_id and len(boards) > 1 and not ambiguous_reported:
            print(
                "Multiple supported USB boards are connected; pass a board id:",
                file=sys.stderr,
            )
            for board in boards:
                print(f"  {describe(board)}", file=sys.stderr)
            ambiguous_reported = True

        if time.monotonic() >= deadline:
            target = f" matching {requested_id}" if requested_id else ""
            raise TimeoutError(
                f"No single stable ESP32-S3 USB board{target} appeared within "
                f"{timeout_seconds:g} seconds."
            )
        time.sleep(0.25)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--id", default="", help="optional eight-digit Scorebot board id")
    parser.add_argument("--timeout", type=float, default=120.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    requested_id = args.id.lower().removeprefix("0x")
    if requested_id and not re.fullmatch(r"[0-9a-f]{8}", requested_id):
        print("Board id must be exactly eight hexadecimal digits.", file=sys.stderr)
        return 2
    if args.timeout <= 0:
        print("Timeout must be greater than zero.", file=sys.stderr)
        return 2

    try:
        board = wait_for_board(requested_id, args.timeout)
    except (RuntimeError, TimeoutError) as error:
        print(error, file=sys.stderr)
        return 1
    print(board.port)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
