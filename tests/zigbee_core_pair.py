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
    parser.add_argument("--require-group", action="store_true",
                        help="also require a group-addressed toggle submission")
    parser.add_argument("--require-scalar", action="store_true",
                        help="also require a typed 16-bit cluster revision read")
    parser.add_argument("--require-write", action="store_true",
                        help="also require a successful scalar attribute write response")
    parser.add_argument("--require-extra-endpoint", action="store_true",
                        help="also require discovery of the second light endpoint")
    parser.add_argument("--require-custom-cluster", action="store_true",
                        help="also require discovery, write, and readback of a custom cluster")
    args = parser.parse_args()
    output = queue.Queue()
    stop = threading.Event()
    threads = [threading.Thread(target=read_port, args=(name, port, output, stop), daemon=True)
               for name, port in (("light", args.light), ("switch", args.switch))]
    for thread in threads:
        thread.start()
    deadline = time.monotonic() + args.timeout
    states = set()
    toggle = read = report = group = group_change = scalar = write = write_readback = extra = False
    custom_match = custom_write = custom_read = False
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
            if (name == "light" and "Zigbee event 7 status=0" in line
                    and "cluster=6 attribute=0" in line):
                if "value=0" in line:
                    states.add(0)
                if "value=1" in line:
                    states.add(1)
                if group:
                    group_change = True
            if name == "switch" and "Toggle submission=0" in line:
                toggle = True
            if name == "switch" and "Group toggle submission=0" in line:
                group = True
            if (name == "switch" and "Zigbee event 12" in line
                    and "endpoint=11 cluster=6" in line):
                extra = True
            if (name == "switch" and "Zigbee event 12" in line
                    and "endpoint=11 cluster=64512" in line):
                custom_match = True
            if name == "switch" and "Zigbee event 8 status=0" in line:
                read = True
                if "attribute=65533" in line and "type=33" in line and "size=2" in line:
                    scalar = True
                if ("cluster=3 attribute=0" in line and "type=33" in line
                        and "size=2" in line and "value=0 " not in line):
                    write_readback = True
                if ("cluster=64512 attribute=1" in line and "type=33" in line
                        and "size=2" in line and "value=43 " in line):
                    custom_read = True
            if name == "switch" and "Zigbee event 15 status=0" in line:
                report = True
            if (name == "switch" and "Zigbee event 18 status=0" in line
                    and "cluster=3" in line):
                write = True
            if (name == "switch" and "Zigbee event 18 status=0" in line
                    and "cluster=64512" in line):
                custom_write = True
            if (states == {0, 1} and (toggle or group) and read
                    and (report or not args.require_report)
                    and (group_change or not args.require_group)
                    and (scalar or not args.require_scalar)
                    and (write and write_readback or not args.require_write)
                    and (extra or not args.require_extra_endpoint)
                    and (custom_match and custom_write and custom_read
                         or not args.require_custom_cluster)):
                print("PASS: switch toggled both light states, read the attribute"
                      + (", and received a report" if args.require_report else "")
                      + (", with a group toggle" if args.require_group else "")
                      + (", and read a typed scalar" if args.require_scalar else "")
                      + (", and wrote and read back a scalar" if args.require_write else "")
                      + (", and found a second endpoint" if args.require_extra_endpoint else "")
                      + (", and checked a custom cluster" if args.require_custom_cluster else ""), flush=True)
                return 0
        print(f"TIMEOUT: light states={sorted(states)}, toggle={toggle}, read={read}, "
              f"report={report}, group={group}, group_change={group_change}, scalar={scalar}, "
              f"write={write}, write_readback={write_readback}, extra={extra}, "
              f"custom_match={custom_match}, custom_write={custom_write}, custom_read={custom_read}",
              flush=True)
        return 1
    finally:
        stop.set()
        for thread in threads:
            thread.join(timeout=1)


if __name__ == "__main__":
    sys.exit(main())
