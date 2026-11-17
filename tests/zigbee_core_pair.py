"""Verify the public Zigbee light/switch API on two serial-connected boards."""
import argparse
import queue
import sys
import threading
import time

import serial


def read_port(name, port, output, stop):
    try:
        with serial.Serial(port, 115200, timeout=0.2) as stream:
            pending = b""
            while not stop.is_set():
                pending += stream.read(2048)
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    output.put((name, line.decode("utf-8", "replace").strip()))
    except Exception as error:
        output.put((name, f"SERIAL ERROR: {error}"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--light", required=True, help="coordinator/light serial port")
    parser.add_argument("--switch", required=True, help="end-device/switch serial port")
    parser.add_argument("--timeout", type=int, default=70)
    parser.add_argument("--require-report", action="store_true",
                        help="also require an On/Off attribute report at the switch")
    args = parser.parse_args()
    output = queue.Queue()
    stop = threading.Event()
    threads = [threading.Thread(target=read_port, args=(name, port, output, stop), daemon=True)
               for name, port in (("light", args.light), ("switch", args.switch))]
    for thread in threads:
        thread.start()
    deadline = time.monotonic() + args.timeout
    states = set()
    toggle = read = report = False
    print(f"Observing Zigbee core pair for up to {args.timeout} seconds", flush=True)
    try:
        while time.monotonic() < deadline:
            try:
                name, line = output.get(timeout=min(0.5, max(0.01, deadline - time.monotonic())))
            except queue.Empty:
                continue
            print(f"[{name}] {line}", flush=True)
            if "SERIAL ERROR:" in line:
                return 2
            if name == "light" and "Zigbee event 7 status=0" in line:
                if "value=0" in line:
                    states.add(0)
                if "value=1" in line:
                    states.add(1)
            if name == "switch" and "Toggle submission=0" in line:
                toggle = True
            if name == "switch" and "Zigbee event 8 status=0" in line:
                read = True
            if name == "switch" and "Zigbee event 15 status=0" in line:
                report = True
            if states == {0, 1} and toggle and read and (report or not args.require_report):
                print("PASS: switch toggled both light states, read the attribute"
                      + (", and received a report" if args.require_report else ""), flush=True)
                return 0
        print(f"TIMEOUT: light states={sorted(states)}, toggle={toggle}, read={read}, report={report}", flush=True)
        return 1
    finally:
        stop.set()
        for thread in threads:
            thread.join(timeout=1)


if __name__ == "__main__":
    sys.exit(main())
