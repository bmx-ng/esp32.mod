#!/usr/bin/env bash
set -euo pipefail

module_root="$(cd "$(dirname "$0")/.." && pwd)"
sdk="$(cd "$module_root/../.." && pwd)"
bmk="${ESP32_TEST_BMK:-$sdk/bin/bmk}"
board="${ESP32_TEST_BOARD:-}"
port="${ESP32_TEST_PORT:-}"
suite="${1:-basic}"

if [[ -z "$port" ]]; then
	echo "Set ESP32_TEST_PORT to the board's COM/UART serial port." >&2
	exit 2
fi
if [[ -z "$board" ]]; then
	echo "Set ESP32_TEST_BOARD to the connected board's profile name." >&2
	exit 2
fi
if [[ ! -e "$port" ]]; then
	echo "Serial port does not exist: $port" >&2
	exit 2
fi
if [[ -n "${ESP32_TEST_PYTHON:-}" ]]; then
	python="$ESP32_TEST_PYTHON"
elif [[ -n "${IDF_PYTHON_ENV_PATH:-}" && -x "$IDF_PYTHON_ENV_PATH/bin/python" ]]; then
	python="$IDF_PYTHON_ENV_PATH/bin/python"
else
	python=python3
fi
if ! "$python" -c 'import serial' >/dev/null 2>&1; then
	echo "pyserial is unavailable in $python; set ESP32_TEST_PYTHON to the ESP-IDF Tools Python." >&2
	exit 2
fi

case "$suite" in
	basic)
		tests=(random_esp32 time_alarm gpio_mirrored adc_pwm)
		;;
	loopback)
		tests=(rmt_loopback)
		;;
	psram)
		tests=(psram_info)
		;;
	all)
		tests=(random_esp32 time_alarm gpio_mirrored adc_pwm rmt_loopback)
		;;
	random_esp32|time_alarm|gpio_mirrored|adc_pwm|rmt_loopback|psram_info)
		tests=("$suite")
		;;
	*)
		echo "Unknown suite '$suite' (choose basic, loopback, psram, all, or an example name)." >&2
		exit 2
		;;
esac

if [[ "$board" != esp32s3_44pin_n16r8 && "$board" != baguette_s3 && "$board" != xiao_esp32s3_plus && "$board" != xiao_esp32c6 && "$board" != waveshare_h2_dev_kit_n4 ]]; then
	echo "This hardware suite is currently qualified only for the Baguette S3, 44-pin S3, XIAO S3 Plus, XIAO C6, and Waveshare H2 Dev Kit N4." >&2
	exit 2
fi

if [[ "$board" == baguette_s3 || "$board" == xiao_esp32s3_plus || "$board" == xiao_esp32c6 || "$board" == waveshare_h2_dev_kit_n4 ]]; then
	reset=usb-jtag
else
	reset=uart
fi

if [[ ( "$board" == baguette_s3 || "$board" == xiao_esp32c6 || "$board" == waveshare_h2_dev_kit_n4 ) && " ${tests[*]} " == *" psram_info "* ]]; then
	echo "The selected board has no PSRAM; choose a PSRAM-equipped board for this suite." >&2
	exit 2
fi

echo "Checking connected device against $board before flashing..."
if ! device_report="$(ESPPORT="$port" "$bmk" deviceinfo -l esp32 -board "$board" 2>&1)"; then
	echo "$device_report" >&2
	exit 1
fi
if [[ "$device_report" != *"Port: $port"* || "$device_report" == *"Warning: the selected profile"* ]]; then
	echo "$device_report" >&2
	echo "Connected hardware does not match the selected port/profile; no image was flashed." >&2
	exit 1
fi

work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

for test_name in "${tests[@]}"; do
	case "$test_name" in
		random_esp32) expected='ESP32 random checks passed' ;;
		time_alarm) expected='ESP32 monotonic time and native alarm checks passed' ;;
		gpio_mirrored) expected='ESP32 mirrored GPIO checks passed' ;;
		adc_pwm) expected='ESP32 ADC and PWM checks passed' ;;
		rmt_loopback) expected='RMT loopback passed' ;;
		psram_info) expected='Managed arena in PSRAM: 1' ;;
	esac
	echo "Building and flashing $test_name on $board..."
	source_name="$test_name"
	if [[ "$board" == xiao_esp32c6 && "$test_name" == rmt_loopback ]]; then
		source_name=rmt_loopback_xiao_c6
	fi
	build_args=(-a -r -x -board "$board" -heap 64k)
	if [[ "$test_name" == psram_info ]]; then
		build_args=(-a -r -x -board "$board" -heap-region psram -heap 1m)
	fi
	if ! ESPPORT="$port" "$bmk" makeapp "${build_args[@]}" \
		-o "$work_dir/$test_name" "$module_root/examples/$source_name.bmx" \
		>"$work_dir/$test_name.build.log" 2>&1; then
		tail -n 35 "$work_dir/$test_name.build.log" >&2
		exit 1
	fi
	if [[ "$board" == xiao_esp32s3_plus || "$board" == xiao_esp32c6 || "$board" == waveshare_h2_dev_kit_n4 ]]; then
		# XIAO native USB Serial/JTAG can take a moment to settle after flashing.
		sleep 1
	fi
	"$python" "$module_root/tests/serial_expect.py" \
		--port "$port" --project "$test_name" --expect "$expected" \
		--reset "$reset" --timeout 20
done

echo "ESP32 hardware suite passed: $suite ($board)"
