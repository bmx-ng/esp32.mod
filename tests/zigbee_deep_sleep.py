#!/usr/bin/env python3
"""Check that the Zigbee end-device survives a timed deep-sleep restart."""

import argparse
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="USB serial port of the deep-sleep end device")
    parser.add_argument("--timeout", type=int, default=90)
    args = parser.parse_args()
    deadline = time.monotonic() + args.timeout
    boots = restored = stopped = 0
    connection = None
    last_line = time.monotonic()
    try:
        while time.monotonic() < deadline:
            if connection is None:
                try:
                    connection = serial.Serial(args.port, 115200, timeout=1)
                    last_line = time.monotonic()
                except serial.SerialException:
                    time.sleep(0.5)
                    continue
            try:
                line = connection.readline().decode("utf-8", errors="replace").strip()
            except (OSError, serial.SerialException):
                connection.close()
                connection = None
                continue
            if not line:
                # The native USB serial device can disappear during deep sleep
                # without raising an exception on macOS. Reopen a silent handle
                # so the monitor follows its wake-up re-enumeration.
                if time.monotonic() - last_line > 4:
                    connection.close()
                    connection = None
                continue
            last_line = time.monotonic()
            print(line, flush=True)
            if "Deep-sleep sensor boot" in line:
                boots += 1
            if "Deep-sleep saved network restored" in line:
                restored += 1
            if "Deep-sleep Zigbee stopped" in line:
                stopped += 1
            if boots >= 2 and restored >= 2 and stopped >= 1:
                print("PASS: Zigbee network restored after deep-sleep restart", flush=True)
                return 0
        print(f"TIMEOUT: boots={boots}, restored={restored}, stopped={stopped}", flush=True)
        return 1
    finally:
        if connection is not None:
            connection.close()


if __name__ == "__main__":
    raise SystemExit(main())
