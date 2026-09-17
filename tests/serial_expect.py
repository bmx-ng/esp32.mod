#!/usr/bin/env python3
"""Reset an ESP32 and verify one application's pass message on its console."""

import argparse
import sys
import time
from collections import deque

try:
    import serial
except ImportError as exc:
    raise SystemExit("pyserial is required; use the ESP-IDF Tools Python environment") from exc


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--project", required=True)
    parser.add_argument("--expect", required=True)
    parser.add_argument("--reset", choices=("uart", "usb-jtag"), required=True)
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args()

    recent = deque(maxlen=35)
    seen_project = False
    deadline = time.monotonic() + args.timeout

    try:
        with serial.Serial(args.port, 115200, timeout=0.25) as connection:
            connection.reset_input_buffer()
            if args.reset == "usb-jtag":
                # Restart the application, leaving the download-mode line
                # inactive. ESP-IDF's USBJTAGSerialReset is for entering the
                # bootloader, not for running the flashed application.
                connection.dtr = False
                connection.rts = True
                time.sleep(0.2)
                connection.rts = False
                time.sleep(0.2)
            else:
                # USB-to-UART bridge: RTS resets EN; DTR holds GPIO0 high.
                connection.dtr = False
                connection.rts = True
                time.sleep(0.1)
                connection.rts = False

            while time.monotonic() < deadline:
                line = connection.readline().decode("utf-8", "replace").strip()
                if not line:
                    continue
                recent.append(line)
                if "Project name:" in line and args.project in line:
                    seen_project = True
                if seen_project and args.expect in line:
                    print(f"PASS {args.project}: {line}")
                    return 0
                if seen_project and ("Guru Meditation Error" in line or
                                     "RuntimeError" in line or
                                     "check failed" in line or
                                     "checks failed" in line or
                                     "loopback failed" in line):
                    break
    except serial.SerialException as exc:
        print(f"Serial port {args.port}: {exc}", file=sys.stderr)
        return 1

    print(f"FAIL {args.project}: did not observe {args.expect!r}", file=sys.stderr)
    if not seen_project:
        print("The expected application boot banner was not seen.", file=sys.stderr)
    for line in recent:
        print(f"  {line}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
