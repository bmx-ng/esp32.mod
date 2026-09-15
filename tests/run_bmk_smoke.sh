#!/usr/bin/env bash
set -euo pipefail

module_root="$(cd "$(dirname "$0")/.." && pwd)"
sdk="$(cd "$module_root/../.." && pwd)"
bmk="${ESP32_TEST_BMK:-$sdk/bin/bmk}"
target="${ESP32_TEST_TARGET:-esp32}"
case "$target" in
	esp32|esp32s2|esp32s3|esp32s3_n8r8|baguette_s3) architecture=xtensa ;;
	esp32c2|esp32c3|esp32c5|esp32c6|esp32h2|esp32p4|baguette_c3) architecture=riscv32 ;;
	*) echo "Unknown ESP32 smoke-test target: $target" >&2; exit 1 ;;
esac
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

for example in scalar_smoke managed_smoke managed_stress managed_fault_safety managed_oom hello_world time_alarm calendar power gpio_mirrored gpio_events uart_controller buffered_uart i2c_controller spi_controller random_esp32 adc_pwm watchdog_device watchdog_reset psram_info storage_smoke nvs_storage partition_info ota_from_storage ota_network_receiver wifi_scan wifi_esp32 wifi_live network_failure socket_loopback ble_scan ble_peripheral ble_client ble_gatt_long_client ble_indication_peripheral ble_secure_peripheral ble_passkey_peripheral ble_connection_management; do
	"$bmk" makeapp \
		-a -r \
		-l esp32 -g "$architecture" -board "$target" -heap 64k \
		-o "$work_dir/$example" \
		"$module_root/examples/$example.bmx"

	test -s "$work_dir/$example.elf"
	test -s "$work_dir/$example.bin"
	test -s "$work_dir/$example.map"
done

if "$bmk" makeapp -a -r -l esp32 -g xtensa -board baguette_s3 \
		-heap-region psram -heap auto -o "$work_dir/invalid-psram" \
		"$module_root/examples/psram_info.bmx"; then
	echo "Expected the Baguette S3 PSRAM heap configuration to fail" >&2
	exit 1
fi

"$bmk" makeapp -a -r -l esp32 -g xtensa -board esp32s3_n8r8 \
	-heap-region psram -heap 1m -o "$work_dir/psram-n8r8" \
	"$module_root/examples/psram_info.bmx"
test -s "$work_dir/psram-n8r8.elf"
test -s "$work_dir/psram-n8r8.bin"

conformance_root="$module_root/../embedded.mod/tests"
for conformance in embedded_runtime_conformance embedded_language_conformance embedded_gpio_conformance embedded_gpio_events_conformance embedded_time_conformance embedded_power_conformance embedded_uart_conformance embedded_buffered_uart_conformance embedded_i2c_conformance embedded_spi_conformance embedded_adc_conformance embedded_pwm_conformance embedded_watchdog_conformance embedded_device_conformance embedded_random_conformance embedded_unicode_conformance embedded_events_conformance embedded_uncaught_string embedded_runtime_error; do
	"$bmk" makeapp \
		-a -r \
		-l esp32 -g "$architecture" -board "$target" -heap 64k \
		-o "$work_dir/$conformance" \
		"$conformance_root/$conformance.bmx"

	test -s "$work_dir/$conformance.elf"
	test -s "$work_dir/$conformance.bin"
	test -s "$work_dir/$conformance.map"
done

for build_mode in debug release; do
	conformance="embedded_assert_$build_mode"
	if [ "$build_mode" = debug ]; then
		mode=-d
	else
		mode=-r
	fi
	"$bmk" makeapp \
		-a "$mode" \
		-l esp32 -g "$architecture" -board "$target" -heap 64k \
		-o "$work_dir/$conformance" \
		"$conformance_root/$conformance.bmx"

	test -s "$work_dir/$conformance.elf"
	test -s "$work_dir/$conformance.bin"
	test -s "$work_dir/$conformance.map"
done

echo "ESP32 compiler, managed runtime stress/failure, and standard IO smoke builds passed: target=$target architecture=$architecture"
