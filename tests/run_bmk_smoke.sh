#!/usr/bin/env bash
set -euo pipefail

module_root="$(cd "$(dirname "$0")/.." && pwd)"
sdk="$(cd "$module_root/../.." && pwd)"
bmk="${ESP32_TEST_BMK:-$sdk/bin/bmk}"
target="${ESP32_TEST_TARGET:-esp32}"
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

for example in scalar_smoke managed_smoke managed_stress hello_world; do
	"$bmk" makeapp \
		-a -r \
		-l esp32 -g xtensa -board "$target" -heap 64k \
		-o "$work_dir/$example" \
		"$module_root/examples/$example.bmx"

	test -s "$work_dir/$example.elf"
	test -s "$work_dir/$example.bin"
	test -s "$work_dir/$example.map"
done

echo "ESP32 compiler, managed runtime stress, and standard IO smoke builds passed: target=$target"
