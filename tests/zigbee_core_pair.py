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
    parser.add_argument("--require-group-query", action="store_true",
                        help="also require View Group and Get Group Membership readback")
    parser.add_argument("--require-scalar", action="store_true",
                        help="also require a typed 16-bit cluster revision read")
    parser.add_argument("--require-write", action="store_true",
                        help="also require a successful scalar attribute write response")
    parser.add_argument("--require-extra-endpoint", action="store_true",
                        help="also require discovery of the second light endpoint")
    parser.add_argument("--require-custom-cluster", action="store_true",
                        help="also require discovery, write, and readback of a custom cluster")
    parser.add_argument("--require-custom-profile", action="store_true",
                        help="also require discovery and readback across custom-profile endpoints")
    parser.add_argument("--require-string", action="store_true",
                        help="also require UTF-8 string write and readback")
    parser.add_argument("--require-local-update", action="store_true",
                        help="also require a remote read of a local custom attribute update")
    parser.add_argument("--require-temperature", action="store_true",
                        help="also require discovery and readback of the temperature sensor")
    parser.add_argument("--require-temperature-report", action="store_true",
                        help="also require a changed temperature attribute report")
    parser.add_argument("--require-humidity", action="store_true",
                        help="also require humidity discovery, readback and a changed report")
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
    custom_match = custom_write = custom_read = custom_local = False
    custom_profile_match = custom_profile_read = False
    string_restored = string_submission = string_write = string_read = False
    temperature_match = temperature_read = False
    temperature_report = False
    humidity_match = humidity_read = humidity_report = False
    group_viewed = group_member = group_membership_complete = False
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
            if name == "switch" and "String write submission=0" in line:
                string_submission = True
            if name == "switch" and "Group toggle submission=0" in line:
                group = True
            if (name == "switch" and "Zigbee event 19 status=0" in line
                    and "value=4660 " in line and "cluster=4" in line):
                group_viewed = True
            if (name == "switch" and "Zigbee event 20 status=0" in line
                    and "value=4660 " in line and "cluster=4" in line):
                group_member = True
            if (name == "switch" and "Zigbee event 21 status=0" in line
                    and "cluster=4" in line):
                group_membership_complete = True
            if (name == "switch" and "Zigbee event 12" in line
                    and "endpoint=11 cluster=6" in line):
                extra = True
            if (name == "switch" and "Zigbee event 12" in line
                    and "endpoint=11 cluster=64512" in line):
                custom_match = True
            if (name == "switch" and "Zigbee event 12" in line
                    and "endpoint=13 cluster=64513" in line):
                custom_profile_match = True
            if (name == "switch" and "Zigbee event 12" in line
                    and "endpoint=12 cluster=1026" in line):
                temperature_match = True
            if (name == "switch" and "Zigbee event 12" in line
                    and "endpoint=12 cluster=1029" in line):
                humidity_match = True
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
                if ("cluster=64512 attribute=1" in line and "type=33" in line
                        and "size=2" in line and "value=44 " in line):
                    custom_local = True
                if ("endpoint=2 cluster=64513 attribute=1" in line and "type=33" in line
                        and "size=2" in line and "value=77 " in line):
                    custom_profile_read = True
                if ("cluster=64512 attribute=2" in line and "type=66" in line
                        and "size=9 text=Zigbee é" in line and string_write):
                    string_read = True
                if ("cluster=64512 attribute=2" in line and "type=66" in line
                        and "size=5 text=ready" in line and string_read):
                    string_restored = True
                if ("cluster=1026 attribute=0" in line and "type=41" in line
                        and "size=2" in line and "value=2345 " in line):
                    temperature_read = True
                if ("cluster=1029 attribute=0" in line and "type=33" in line
                        and "size=2" in line and "value=4550 " in line):
                    humidity_read = True
            if (name == "switch" and "Zigbee event 15 status=0" in line
                    and "cluster=1026 attribute=0" in line and "type=41 size=2" in line
                    and "value=2355 " in line):
                temperature_report = True
            if (name == "switch" and "Zigbee event 15 status=0" in line
                    and "cluster=1029 attribute=0" in line and "type=33 size=2" in line
                    and "value=4560 " in line):
                humidity_report = True
            if (name == "switch" and "Zigbee event 15 status=0" in line
                    and "cluster=6 attribute=0" in line):
                report = True
            if (name == "switch" and "Zigbee event 18 status=0" in line
                    and "cluster=3" in line):
                write = True
            if (name == "switch" and "Zigbee event 18 status=0" in line
                    and "cluster=64512" in line):
                custom_write = True
                if string_submission:
                    string_write = True
            if (states == {0, 1} and (toggle or group) and read
                    and (report or not args.require_report)
                    and (group_change or not args.require_group)
                    and (group_viewed and group_member and group_membership_complete
                         or not args.require_group_query)
                    and (scalar or not args.require_scalar)
                    and (write and write_readback or not args.require_write)
                    and (extra or not args.require_extra_endpoint)
                    and (custom_match and custom_write and custom_read
                         or not args.require_custom_cluster)
                    and (custom_profile_match and custom_profile_read
                         or not args.require_custom_profile)
                    and (string_restored and string_submission and string_write and string_read
                         or not args.require_string)
                    and (custom_local or not args.require_local_update)
                    and (temperature_match and temperature_read or not args.require_temperature)
                    and (temperature_report or not args.require_temperature_report)
                    and (humidity_match and humidity_read and humidity_report
                         or not args.require_humidity)):
                print("PASS: switch toggled both light states, read the attribute"
                      + (", and received a report" if args.require_report else "")
                      + (", with a group toggle" if args.require_group else "")
                      + (", and confirmed group membership" if args.require_group_query else "")
                      + (", and read a typed scalar" if args.require_scalar else "")
                      + (", and wrote and read back a scalar" if args.require_write else "")
                      + (", and found a second endpoint" if args.require_extra_endpoint else "")
                      + (", and checked a custom cluster" if args.require_custom_cluster else "")
                      + (", and read a custom-profile endpoint" if args.require_custom_profile else "")
                      + (", and wrote and read back a UTF-8 string" if args.require_string else "")
                      + (", and read a local update" if args.require_local_update else "")
                      + (", and read temperature" if args.require_temperature else "")
                      + (", and received a changed temperature report"
                         if args.require_temperature_report else "")
                      + (", and read and received humidity" if args.require_humidity else ""), flush=True)
                return 0
        print(f"TIMEOUT: light states={sorted(states)}, toggle={toggle}, read={read}, "
              f"report={report}, group={group}, group_change={group_change}, "
              f"group_viewed={group_viewed}, group_member={group_member}, "
              f"group_membership_complete={group_membership_complete}, scalar={scalar}, "
              f"write={write}, write_readback={write_readback}, extra={extra}, "
              f"custom_match={custom_match}, custom_write={custom_write}, custom_read={custom_read}, "
              f"custom_profile_match={custom_profile_match}, custom_profile_read={custom_profile_read}, "
              f"string_restored={string_restored}, string_submission={string_submission}, "
              f"string_write={string_write}, "
              f"string_read={string_read}, "
              f"custom_local={custom_local}, temperature_match={temperature_match}, "
              f"temperature_read={temperature_read}, temperature_report={temperature_report}, "
              f"humidity_match={humidity_match}, humidity_read={humidity_read}, "
              f"humidity_report={humidity_report}",
              flush=True)
        return 1
    finally:
        stop.set()
        for thread in threads:
            thread.join(timeout=1)


if __name__ == "__main__":
    sys.exit(main())
