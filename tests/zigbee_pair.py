"""Observe the H2 light and C6 switch bring-up with a bounded serial test."""
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
    parser.add_argument("--h2", required=True, help="H2 USB serial port")
    parser.add_argument("--c6", required=True, help="C6 USB serial port")
    parser.add_argument("--timeout", type=int, default=70, help="maximum observation time in seconds")
    args = parser.parse_args()
    output = queue.Queue()
    stop = threading.Event()
    threads = [threading.Thread(target=read_port, args=(name, port, output, stop), daemon=True)
               for name, port in (("H2", args.h2), ("C6", args.c6))]
    for thread in threads:
        thread.start()
    deadline = time.monotonic() + args.timeout
    light_states = set()
    saw_toggle = False
    saw_read = False
    print(f"Observing Zigbee pair for up to {args.timeout} seconds", flush=True)
    try:
        while time.monotonic() < deadline:
            try:
                name, line = output.get(timeout=min(0.5, max(0.01, deadline - time.monotonic())))
            except queue.Empty:
                continue
            print(f"[{name}] {line}", flush=True)
            if "SERIAL ERROR:" in line:
                return 2
            if name == "H2" and "Light On/Off=" in line:
                light_states.add(line.rsplit("=", 1)[-1])
            if name == "C6" and "Toggle request result=0" in line:
                saw_toggle = True
            if name == "C6" and "Read light On/Off=" in line:
                saw_read = True
            if len(light_states) == 2 and saw_toggle and saw_read:
                print("PASS: joined switch toggled light both ways and read its attribute", flush=True)
                return 0
        print(f"TIMEOUT: light states={sorted(light_states)}, toggle={saw_toggle}, read={saw_read}", flush=True)
        return 1
    finally:
        stop.set()
        for thread in threads:
            thread.join(timeout=1)


if __name__ == "__main__":
    sys.exit(main())
