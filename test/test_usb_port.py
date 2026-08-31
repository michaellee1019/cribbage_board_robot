#!/usr/bin/env python3

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))

from usb_port import board_id_from_serial, recognized_boards, select_board


class UsbPortTests(unittest.TestCase):
    def test_board_id_uses_last_four_mac_bytes(self) -> None:
        self.assertEqual(board_id_from_serial("98:3D:AE:EA:17:BC"), "aeea17bc")
        self.assertEqual(board_id_from_serial("not-a-mac"), "")

    def test_recognizes_native_and_seeed_usb_ids(self) -> None:
        devices = [
            {
                "port": "/dev/cu.Bluetooth-Incoming-Port",
                "description": "n/a",
                "hwid": "n/a",
            },
            {
                "port": "/dev/cu.usbmodem1201",
                "description": "USB JTAG/serial debug unit",
                "hwid": "USB VID:PID=303A:1001 SER=98:3D:AE:EA:17:BC LOCATION=0-1.2",
            },
            {
                "port": "/dev/cu.usbserial-unrelated",
                "description": "unrelated adapter",
                "hwid": "USB VID:PID=10C4:EA60 SER=1234",
            },
        ]
        boards = recognized_boards(devices)
        self.assertEqual(len(boards), 1)
        self.assertEqual(boards[0].port, "/dev/cu.usbmodem1201")
        self.assertEqual(boards[0].board_id, "aeea17bc")

    def test_selection_requires_id_when_multiple_boards_are_present(self) -> None:
        boards = recognized_boards(
            [
                {
                    "port": "/dev/cu.usbmodem1",
                    "description": "USB JTAG/serial debug unit",
                    "hwid": "USB VID:PID=303A:1001 SER=98:3D:AE:EA:17:BC",
                },
                {
                    "port": "/dev/cu.usbmodem2",
                    "description": "USB JTAG/serial debug unit",
                    "hwid": "USB VID:PID=303A:1001 SER=98:3D:AE:EA:0F:40",
                },
            ]
        )
        self.assertIsNone(select_board(boards, ""))
        self.assertEqual(select_board(boards, "aeea0f40").port, "/dev/cu.usbmodem2")
        self.assertEqual(select_board(boards, "0xAEEA17BC").port, "/dev/cu.usbmodem1")


if __name__ == "__main__":
    unittest.main()
