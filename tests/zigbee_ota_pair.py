"""Verify a Zigbee OTA transfer and the client's boot into firmware version 2."""

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
    parser.add_argument("--server", required=True)
    parser.add_argument("--client", required=True)
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--require-mark-valid", action="store_true",
                        help="require the updated app to confirm rollback validation")
    args = parser.parse_args()
    output = queue.Queue()
    stop = threading.Event()
    threads = [threading.Thread(target=read_port, args=(name, port, output, stop), daemon=True)
               for name, port in (("server", args.server), ("client", args.client))]
    for thread in threads:
        thread.start()
    seen = set()
    deadline = time.monotonic() + args.timeout
    try:
        while time.monotonic() < deadline:
            try:
                name, line = output.get(timeout=min(0.5, max(0.01, deadline - time.monotonic())))
            except queue.Empty:
                continue
            print(f"[{name}] {line}", flush=True)
            if "SERIAL ERROR:" in line:
                return 2
            if name == "server" and "OTA serve image result=0" in line:
                seen.add("server")
            if name == "client" and "OTA client version=1" in line:
                seen.add("v1")
            if name == "client" and "OTA event kind=22 signal=0 status=0" in line:
                seen.add("start")
            if name == "client" and "OTA event kind=22 signal=1 status=0" in line:
                seen.add("receiving")
            if name == "client" and "OTA event kind=22 signal=3 status=0" in line:
                seen.add("checked")
            if name == "client" and "OTA event kind=22 signal=4 status=0" in line:
                seen.add("applied")
            if name == "client" and "OTA client version=2" in line:
                seen.add("v2")
            if name == "client" and "OTA mark running valid result=0" in line:
                seen.add("valid")
            required = {"receiving", "checked", "applied", "v2"}
            if args.require_mark_valid:
                required.add("valid")
            if required <= seen:
                print("PASS: Zigbee OTA transferred, validated, activated, and booted version 2", flush=True)
                return 0
        print(f"TIMEOUT: observed {sorted(seen)}", flush=True)
        return 1
    finally:
        stop.set()
        for thread in threads:
            thread.join(timeout=1)


if __name__ == "__main__":
    sys.exit(main())
