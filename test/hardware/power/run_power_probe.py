#!/usr/bin/env python3
"""Capture one power-probe sequence; this script does not flash firmware."""

import argparse
from collections import defaultdict
import json
from pathlib import Path
import statistics
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--log", required=True, type=Path)
    args = parser.parse_args()
    port = serial.Serial(port=None, baudrate=115200, timeout=0.3)
    port.dtr = False
    port.rts = False
    port.port = args.port
    samples = defaultdict(list)
    started = False
    complete = False
    deadline = time.monotonic() + 90
    args.log.parent.mkdir(parents=True, exist_ok=True)

    with args.log.open("x") as output:
        try:
            port.open()
            # Native USB serial can reset the chip while returning old TX
            # bytes. Wait for boot, discard them, and request fresh identity.
            time.sleep(3)
            port.reset_input_buffer()
            port.write(b"quiet\ninfo\n")
            port.flush()
            while time.monotonic() < deadline:
                line = port.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                try:
                    event = json.loads(line)
                except json.JSONDecodeError:
                    print(line, flush=True)
                    continue
                output.write(json.dumps(event, separators=(",", ":")) + "\n")
                output.flush()
                kind = event.get("type")
                if kind == "ready":
                    if started:
                        raise RuntimeError("board reset during the sequence")
                    if event.get("probe") != "power-v1":
                        raise RuntimeError("unexpected diagnostic firmware")
                    print(json.dumps(event), flush=True)
                    port.write(b"run\n")
                    port.flush()
                    started = True
                elif kind == "sample":
                    samples[event["stage"]].append(event)
                    if len(samples[event["stage"]]) == 1:
                        print(
                            f"{event['stage']}: VBUS {event['vbus_mv']:.1f} mV, "
                            f"BAT node {event['bat_mv']:.1f} mV, "
                            f"EN {event['enable_read']}, display ACK {event['display_ack']}",
                            flush=True,
                        )
                elif kind == "sequence_end":
                    complete = True
                    break
                elif kind == "error":
                    raise RuntimeError(event)
        finally:
            if port.is_open:
                port.close()

    if not complete:
        raise RuntimeError("probe sequence did not finish within 90 seconds")
    expected = {
        "float_off": 4, "high_off": 4, "low_off": 4, "high_dim": 4,
        "high_bright": 4, "low_bright": 4, "high_recovery": 4,
        "cycle_low": 20, "cycle_high": 20, "final_float_off": 4,
    }
    actual = {stage: len(rows) for stage, rows in samples.items() if stage != "idle"}
    if actual != expected:
        raise RuntimeError(f"incomplete sample set: {actual}")
    for stage, rows in samples.items():
        print(
            f"{stage}: n={len(rows)}, "
            f"VBUS median={statistics.median(row['vbus_mv'] for row in rows):.1f} mV, "
            f"BAT median={statistics.median(row['bat_mv'] for row in rows):.1f} mV"
        )
    print(f"PROBE_CAPTURE_COMPLETE: {sum(actual.values())} samples; {args.log}")


if __name__ == "__main__":
    main()
