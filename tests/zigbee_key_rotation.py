#!/usr/bin/env python3
"""Verify two-board traffic across a network-key rotation."""

import argparse
import queue
import threading
import time

import serial


def read_port(name, port, output, stop):
    while not stop.is_set():
        try:
            with serial.Serial(port, 115200, timeout=0.2) as stream:
                stream.dtr = False
                stream.rts = False
                pending = b""
                last_data = time.monotonic()
                while not stop.is_set():
                    data = stream.read(2048)
                    if data:
                        pending += data
                        last_data = time.monotonic()
                    elif time.monotonic() - last_data > 10:
                        break
                    while b"\n" in pending:
                        line, pending = pending.split(b"\n", 1)
                        output.put((name, line.decode("utf-8", "replace").strip()))
        except (OSError, serial.SerialException):
            pass
        if not stop.is_set():
            time.sleep(0.5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--light", required=True, help="coordinator serial port")
    parser.add_argument("--switch", required=True, help="end-device serial port")
    parser.add_argument("--timeout", type=int, default=150)
    args = parser.parse_args()
    output = queue.Queue()
    stop = threading.Event()
    threads = [threading.Thread(target=read_port, args=(name, port, output, stop), daemon=True)
               for name, port in (("light", args.light), ("switch", args.switch))]
    for thread in threads:
        thread.start()
    deadline = time.monotonic() + args.timeout
    baseline = distributed = announced = switched = False
    post_states = set()
    post_reads = 0
    try:
        while time.monotonic() < deadline:
            try:
                name, line = output.get(timeout=0.5)
            except queue.Empty:
                continue
            print(f"[{name}] {line}", flush=True)
            if name == "light":
                if "ROTATE baseline traffic verified" in line:
                    baseline = True
                if "ROTATE distribute result=0" in line:
                    distributed = True
                if "ROTATE announce switch result=0" in line:
                    announced = True
                if "ROTATE local switch result=0" in line:
                    switched = True
                if switched and "ROTATE light change=" in line:
                    if "value=0" in line:
                        post_states.add(0)
                    if "value=1" in line:
                        post_states.add(1)
            if (name == "switch" and switched and "Zigbee event 8 status=0" in line
                    and "cluster=6 attribute=0" in line):
                post_reads += 1
            if (baseline and distributed and announced and switched
                    and post_states == {0, 1} and post_reads >= 2):
                print("PASS: On/Off traffic and reads continued after network-key rotation", flush=True)
                return 0
        print(f"TIMEOUT: baseline={baseline} distributed={distributed} announced={announced} "
              f"switched={switched} post_states={post_states} post_reads={post_reads}", flush=True)
        return 1
    finally:
        stop.set()
        for thread in threads:
            thread.join(timeout=2)


if __name__ == "__main__":
    raise SystemExit(main())
