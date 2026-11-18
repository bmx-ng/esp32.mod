"""Wrap a built ESP32 application image as a single-element Zigbee OTA file."""

import argparse
import pathlib
import struct


HEADER = struct.Struct("<IHHHHHIH32sI")
ELEMENT = struct.Struct("<HI")


def number(value):
    return int(value, 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=pathlib.Path, help="built ESP32 application .bin")
    parser.add_argument("output", type=pathlib.Path, help="Zigbee .ota file to create")
    parser.add_argument("--manufacturer", type=number, required=True)
    parser.add_argument("--image-type", type=number, required=True)
    parser.add_argument("--version", type=number, required=True)
    args = parser.parse_args()

    if not 0 <= args.manufacturer < 0xFFFF:
        parser.error("manufacturer must be from 0 to 0xFFFE")
    if not 0 <= args.image_type < 0xFFFF:
        parser.error("image type must be from 0 to 0xFFFE")
    if not 1 <= args.version < 0xFFFFFFFF:
        parser.error("version must be from 1 to 0xFFFFFFFE")
    image = args.image.read_bytes()
    if len(image) < 24 or image[0] != 0xE9:
        parser.error("input is not an ESP32 application image")
    total = HEADER.size + ELEMENT.size + len(image)
    if total > 0xFFFFFFFF:
        parser.error("image is too large for a Zigbee OTA file")
    header = HEADER.pack(0x0BEEF11E, 0x0100, HEADER.size, 0,
                         args.manufacturer, args.image_type, args.version,
                         0x0002, b"BlitzMax ESP32 firmware", total)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(header + ELEMENT.pack(0, len(image)) + image)
    print(f"Created {args.output}: {total} bytes, version {args.version}")


if __name__ == "__main__":
    main()
