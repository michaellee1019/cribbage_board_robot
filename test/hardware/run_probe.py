#!/usr/bin/env python3
"""Run the bench firmware on a UART-connected ESP32-S3 (requires pyserial)."""
import argparse
import json
import time
from pathlib import Path
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", required=True)
parser.add_argument("--session", type=Path, required=True, help="JSON object with leader, session, pair")
parser.add_argument("--log", type=Path, required=True)
args = parser.parse_args()
configuration = json.loads(args.session.read_text())
if set(configuration) != {"leader", "session", "pair"}:
    raise ValueError("expected only leader, session, pair")
port = serial.Serial(port=None, baudrate=115200, timeout=0.2)
# DTR controls BOOT on UART adapters. Keep it deasserted while pulsing EN/RTS.
port.dtr = False
port.rts = False
port.port = args.port
port.open()
port.rts = True
time.sleep(0.15)
port.rts = False
deadline = time.monotonic() + 55
sent = False
result = 1
with args.log.open("w") as log:
    while time.monotonic() < deadline:
        line = port.readline().decode("utf-8", errors="replace")
        if line:
            log.write(line)
            log.flush()
            print(line.rstrip(), flush=True)
            if "PROBE_READY" in line and not sent:
                port.write((json.dumps(configuration) + "\n").encode())
                port.flush()
                sent = True
            if "PROBE_PASS" in line:
                result = 0
                break
            if "PROBE_FAIL" in line:
                break
port.close()
if result:
    print("Probe failed or timed out; inspect the log.")
raise SystemExit(result)
