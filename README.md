# esp32.mod

ESP-IDF platform support for BlitzMax NG embedded applications.

`esp32.mod` lets `bmk` build, upload, and run BlitzMax applications on supported
ESP32 devices. It provides ESP32 implementations of the portable `Embedded.*`
APIs alongside target-specific modules for applications that need more direct
control of the hardware.

## Supported targets

The currently validated targets use ESP-IDF 6.1:

| Target | Architecture | Validated board profile | Notes |
| --- | --- | --- | --- |
| ESP32 | Xtensa | `esp32` | Generic original ESP32 profile |
| ESP32-S3 | Xtensa | `baguette_s3` | 8 MiB flash, MicroSD slot, no PSRAM |
| ESP32-S3 | Xtensa | `esp32s3_44pin_n16r8` | 16 MiB flash, 8 MiB PSRAM, dual USB-C |
| ESP32-S3 | Xtensa | `xiao_esp32s3_plus` | 16 MiB flash, 8 MiB PSRAM, native USB, no onboard SD slot |
| ESP32-C3 | 32-bit RISC-V | `baguette_c3` | 4 MiB flash, no SD slot or PSRAM |
| ESP32-C6 | 32-bit RISC-V | `esp32_c6_supermini` | 4 MiB flash, native USB, no PSRAM |

The Baguette S3, ESP32-S3 44-pin N16R8, XIAO ESP32-S3 Plus, Baguette C3, and
ESP32-C6 SuperMini profiles have been tested on their respective physical
boards. Additional
generic profiles describe other ESP-IDF target families for inspection and
future validation; their presence does not by itself mean that the complete
BlitzMax implementation has been tested on that target.

## Requirements

- A BlitzMax NG installation containing the ESP32-enabled `bmk` and compiler.
- `esp32.mod` installed as `mod/esp32.mod`.
- The matching `embedded.mod`, `blitzmax.mod`, `brl.mod`, `pub.mod`, and
  `random.mod` versions.
- ESP-IDF 6.1 and its target toolchains.
- A data-capable USB connection for one-command upload.

`bmk` locates ESP-IDF using, in order, the `esp32.idf` option in `custom.bmk`,
the `IDF_PATH` environment variable, or the newest recognized installation
beneath `~/.espressif`.

## Quick start

Create `hello.bmx`:

```blitzmax
SuperStrict

Framework BRL.StandardIO

Print "Hello from BlitzMax on ESP32"
```

Build and upload it to a connected Baguette S3:

```sh
bmk makeapp -a -r -board baguette_s3 -heap 64k -x hello.bmx
```

For a Baguette C3, select its board profile instead:

```sh
bmk makeapp -a -r -board baguette_c3 -heap 64k -x hello.bmx
```

The ESP32-C6 SuperMini likewise has its own board profile:

```sh
bmk makeapp -a -r -board esp32_c6_supermini -heap 64k -x hello.bmx
```

The dual-USB 44-pin N16R8 board uses its CH343P `COM` socket for automatic
upload, reset, and console output. Its separate `USB` socket is wired directly
to the ESP32-S3 native USB/OTG interface. To use its external PSRAM-backed
managed heap:

```sh
bmk makeapp -a -r -board esp32s3_44pin_n16r8 \
    -heap-region psram -heap auto -x hello.bmx
```

For recognised profiles, `bmk` infers the ESP32 platform, compiler
architecture, and ESP-IDF target from `-board`. Explicit `-l` and `-g` options
remain available for scripts and are checked against the selected profile.

The important options are:

| Option | Meaning |
| --- | --- |
| `-l esp32` | Explicitly select ESP32; inferred from a recognised `-board` profile |
| `-g xtensa` | Explicitly select the original ESP32/ESP32-S3 architecture |
| `-g riscv32` | Explicitly select a RISC-V ESP32 architecture, such as ESP32-C3 or ESP32-C6 |
| `-board <name>` | Select a board profile |
| `-heap auto` | Use the default managed heap; currently 64 KiB in internal SRAM |
| `-heap <size>` | Set the managed heap in bytes or with `k`, `KiB`, `m`, or `MiB` |
| `-heap-region sram` | Place the managed heap in internal SRAM; this is the default |
| `-heap-region psram` | Place the managed heap in profile-declared external PSRAM |
| `-x` | Build, upload, reset, and start the application |
| `-o <name>` | Choose the output name |
| `-d` | Build with source-level GDB information |
| `-r` | Build optimised release firmware |

Without `-x`, the build produces an ELF image, flashable BIN image, and link
map. ESP-IDF's bootloader, partition table, flash arguments, and generated
configuration remain in the source file's `.bmx` build directory.

When exactly one compatible board is connected, upload selects it
automatically. Set `ESPPORT` or `esp32.port` in `custom.bmk` when more than one
device is available.

## Tool configuration

Tool locations and persistent defaults can be set in `bin/custom.bmk` within
the BlitzMax installation:

```bmk
addoption esp32.idf "/path/to/esp-idf"
addoption esp32.tools "/path/to/.espressif"
addoption esp32.python "/path/to/.espressif/python_env/idf6.1_py3.10_env"
addoption esp32.target "baguette_s3"
addoption esp32.port "/dev/cu.usbmodem101"
addoption esp32.board.dirs "/path/to/custom/board/profiles"
addoption esp32.heap.region "psram"
```

The corresponding environment variables are:

| `custom.bmk` key | Environment variable | Purpose |
| --- | --- | --- |
| `esp32.idf` | `IDF_PATH` | ESP-IDF root containing `tools/idf.py` |
| `esp32.tools` | `IDF_TOOLS_PATH` | ESP-IDF's downloaded-tools root |
| `esp32.python` | `IDF_PYTHON_ENV_PATH` | Matching Python environment directory or executable |
| `esp32.port` | `ESPPORT` | Serial or USB device used for inspection and upload |
| `esp32.board.dirs` | `ESP32_BOARD_DIRS` | Additional board-profile roots |
| `esp32.target` | — | Default board profile when `-board` is omitted |
| `esp32.heap.region` | — | Default managed-heap region, `sram` or `psram` |

A command-line `-board` or `-heap-region` overrides the applicable persistent
default. For settings with both forms, the `custom.bmk` option takes precedence
over its environment variable. If no port is configured, `bmk` can select an
exactly matching single connected device automatically.

An installation created by Espressif's installer normally needs no entries in
`custom.bmk`. If `esp32.idf` and `IDF_PATH` are both absent, `bmk` searches
`~/.espressif/v*/esp-idf` and selects the newest recognized version.

A manual installation may keep the ESP-IDF checkout elsewhere while retaining
downloaded tools under `~/.espressif/tools` and versioned Python environments
under `~/.espressif/python_env`. `bmk` honours `IDF_TOOLS_PATH` and
`IDF_PYTHON_ENV_PATH` when they are set by ESP-IDF's `export.sh`. When they are
unset, it recognizes both the manual layout and Espressif's installer-managed
layout, and selects a Python environment whose `idf_version.txt` matches the
selected ESP-IDF checkout.

On Windows, separate multiple `esp32.board.dirs` or `ESP32_BOARD_DIRS` entries
with semicolons. On macOS and Linux, use colons.

## Boards and device inspection

Inspect a board profile without connecting hardware:

```sh
bmk boardinfo -l esp32 -board baguette_s3
```

The report includes the target architecture, flash and PSRAM configuration,
console transport, default buses, onboard resources, named pins, and important
pin conflicts or restrictions.

Inspect a connected device without building or flashing:

```sh
bmk deviceinfo -l esp32
```

Select a profile as well to compare the physical device with the intended build
configuration:

```sh
bmk deviceinfo -l esp32 -board baguette_c3
```

The retail board cannot be inferred reliably from its chip and flash header, so
`deviceinfo` reports detected hardware and selected configuration separately.
It warns when the configured image flash size differs from the detected flash.

Profiles live under [`boards`](boards). See the [board-profile guide](boards/README.md)
to add a board or configure an additional profile directory.

## Portable and target-specific APIs

Use `Embedded.*` when an application should compile unchanged for both Pico and
ESP32. Use the corresponding `ESP32.*` module when the application needs
ESP-IDF-specific capabilities. The target facade retains familiar operation
names where the underlying hardware concepts align.

For example, portable GPIO code imports:

```blitzmax
Import Embedded.Hardware.GPIO
```

ESP32-specific GPIO code can instead import:

```blitzmax
Import ESP32.Hardware.GPIO
```

The latter also provides ESP32 drive-capability controls. This pattern is used
throughout GPIO, time, UART, I2C, SPI, ADC, PWM, watchdog, device, random,
power, Wi-Fi, and BLE support.

## Feature guide

| Area | Primary modules | Start with |
| --- | --- | --- |
| GPIO | `Embedded.Hardware.GPIO`, `ESP32.Hardware.GPIO` | [`gpio_mirrored.bmx`](examples/gpio_mirrored.bmx), [`gpio_events.bmx`](examples/gpio_events.bmx) |
| Time and calendar | `Embedded.System.Time`, `ESP32.System.Time`, `ESP32.System.Calendar` | [`time_alarm.bmx`](examples/time_alarm.bmx), [`calendar.bmx`](examples/calendar.bmx) |
| Power | `Embedded.System.Power`, `ESP32.System.Power` | [`power.bmx`](examples/power.bmx) |
| UART | `Embedded.Hardware.UART`, `Embedded.IO.BufferedUART`, `ESP32.Hardware.UART` | [`uart_controller.bmx`](examples/uart_controller.bmx), [`buffered_uart.bmx`](examples/buffered_uart.bmx) |
| I2C and SPI | `Embedded.Hardware.I2C`, `Embedded.Hardware.SPI`, ESP32 facades | [`i2c_controller.bmx`](examples/i2c_controller.bmx), [`spi_controller.bmx`](examples/spi_controller.bmx) |
| Timed pulses (RMT) | `ESP32.Hardware.RMT` | [`rmt_pulses.bmx`](examples/rmt_pulses.bmx), [`rmt_loopback.bmx`](examples/rmt_loopback.bmx), [`rmt_ws2812_44pin.bmx`](examples/rmt_ws2812_44pin.bmx) |
| Random data | `Embedded.Random`, `ESP32.Random` | [`random_esp32.bmx`](examples/random_esp32.bmx) |
| ADC and PWM | `Embedded.Hardware.ADC`, `Embedded.Hardware.PWM`, ESP32 facades | [`adc_pwm.bmx`](examples/adc_pwm.bmx) |
| Watchdog and identity | `Embedded.Hardware.Watchdog`, `Embedded.System.Device`, ESP32 facades | [`watchdog_device.bmx`](examples/watchdog_device.bmx) |
| PSRAM | `ESP32.Hardware.PSRAM` | [`psram_info.bmx`](examples/psram_info.bmx) |
| Filesystems | `ESP32.Storage.LittleFS`, `ESP32.Storage.SDCard` | [`storage_smoke.bmx`](examples/storage_smoke.bmx) |
| NVS and partitions | `ESP32.Storage.NVS`, `ESP32.System.Partition` | [`nvs_storage.bmx`](examples/nvs_storage.bmx), [`partition_info.bmx`](examples/partition_info.bmx) |
| OTA | `ESP32.System.OTA` | [`ota_from_storage.bmx`](examples/ota_from_storage.bmx) |
| Wi-Fi and sockets | `Embedded.Network.WiFi`, `ESP32.Network.WiFi`, `BRL.Socket` | [`wifi_scan.bmx`](examples/wifi_scan.bmx), [`socket_loopback.bmx`](examples/socket_loopback.bmx) |
| BLE | `Embedded.Network.BLE`, `ESP32.Network.BLE` | [`ble_scan.bmx`](examples/ble_scan.bmx), [`ble_peripheral.bmx`](examples/ble_peripheral.bmx), [`ble_client.bmx`](examples/ble_client.bmx) |

Hardware resources differ between ESP32 variants. Query capabilities where the
API provides them and consult `boardinfo` before choosing pins. For example,
the Baguette C3 profile reports that its documented I2C and SPI clocks share
GPIO6, and that GPIO2, GPIO8, and GPIO9 are strapping pins. The number of UARTs,
ADC channels, and other peripherals is likewise target-dependent.

`ESP32.Hardware.RMT` owns transmit and receive channels for precisely timed
GPIO pulse sequences. It supports packed symbols, byte-to-pulse encoding,
optional carrier modulation/demodulation, and nonblocking receive with a
native buffer. Transmit calls complete before returning; an RMT interrupt
never calls BlitzMax or retains a managed array. The WS2812 example shows how
to build an LED protocol on this general API, using the 44-pin S3 board's
GPIO48 LED. Change the pin and color order for other boards.
The loopback example tests reception with a jumper from GPIO4 to GPIO5.

## Managed memory and PSRAM

`-heap` selects the fixed managed arena used for BlitzMax objects, strings, and
arrays. The size must be a multiple of 16 bytes. ESP-IDF and native subsystem
allocations remain outside this arena.

Boards with supported external RAM may place the managed arena in PSRAM:

```sh
bmk makeapp -a -r -l esp32 -g xtensa -board esp32s3_n8r8 \
    -heap-region psram -heap 1m -x hello.bmx
```

The selected profile must declare a fixed PSRAM capacity and supply the required
ESP-IDF settings. Unsupported combinations fail before compilation. With a
PSRAM heap, `-heap auto` keeps one eighth of the declared capacity available to
ESP-IDF and native services, with a minimum reserve of 256 KiB. Use
`ESP32.Hardware.PSRAM` to inspect capacity, free space, and managed-arena
placement.

The managed runtime is single-task. Ordinary applications should use the
provided event-based modules rather than call BlitzMax directly from interrupts
or foreign FreeRTOS tasks. Authors of native integrations should read the
[runtime integration guide](docs/runtime-integration.md).

## Filesystems and persistent storage

Importing `ESP32.Storage.LittleFS` mounts the profile's internal `storage`
partition and makes it the default filesystem. It works through the normal
`BRL.FileSystem`, stream, and text APIs:

```blitzmax
Import BRL.TextStream
Import ESP32.Storage.LittleFS

SaveText "settings", "config.txt"
Print LoadText("littlefs::config.txt")
```

On a board with a profile-defined SD slot, mount it explicitly and use the
`sd::` prefix:

```blitzmax
Import ESP32.Storage.SDCard

If MountSDCard() Then
    SaveText "archive", "sd::archive.txt"
End If
```

`littlefs::` and `sd::` always select a volume. Plain paths and `file::` use the
selected default. `SetDefaultStorageVolume("sd")` changes that default, while
`CopyFile` can copy between volumes. Cross-volume renames are rejected.

The Baguette C3 has no SD slot; use LittleFS or NVS there. Examples that name an
`sd::` path are intended for an SD-equipped profile unless adapted to use
LittleFS.

A completely erased LittleFS partition may be formatted automatically.
Unrecognised existing data is not erased implicitly. SD mounting is
non-destructive by default. `FormatLittleFS`, `FormatSDCard`, and mounting with
the SD formatting option are explicit destructive operations.

`ESP32.Storage.NVS` provides wear-levelled namespaced key/value storage for
integers, floating-point values, strings, managed byte arrays, and caller-owned
raw memory. Changes are persisted by an explicit commit.

## Wi-Fi and sockets

`Embedded.Network.WiFi` provides the portable station interface. The
`ESP32.Network.WiFi` facade adds power saving, transmit power, protocols,
bandwidth, detailed station information, and concurrent SoftAP control.
`BRL.Socket` uses ESP-IDF/lwIP for UDP, TCP, address lookup, socket options, and
deferred readiness events.

[`wifi_scan.bmx`](examples/wifi_scan.bmx) needs no credentials. The live network
examples expect `wifi.conf` to contain the SSID on its first line and password
on its second. The supplied S3 examples read `sd::wifi.conf`; use a suitably
protected LittleFS path when adapting them for a board without SD storage.
Credentials are application data and are never compiled into these modules.

TLS and `Net.HTTP` integration are not currently part of this module. The HTTP
checks in the examples use plain local or test traffic.

## Bluetooth Low Energy

`Embedded.Network.BLE` provides portable lifecycle, discovery, central, GATT
peripheral, security, bonding, and connection-management APIs.
`ESP32.Network.BLE` supplies the ESP-IDF NimBLE adapter and ESP32 result names.
Importing BLE enables the Bluetooth components for that application; programs
which do not import it retain a smaller non-Bluetooth build.

Useful examples include:

- [`ble_scan.bmx`](examples/ble_scan.bmx): active discovery.
- [`ble_peripheral.bmx`](examples/ble_peripheral.bmx): read, write, and notify.
- [`ble_client.bmx`](examples/ble_client.bmx): discovery and central operations.
- [`ble_gatt_long_client.bmx`](examples/ble_gatt_long_client.bmx): MTU negotiation and 512-byte GATT values.
- [`ble_indication_peripheral.bmx`](examples/ble_indication_peripheral.bmx): acknowledged indications.
- [`ble_secure_peripheral.bmx`](examples/ble_secure_peripheral.bmx): encryption and persistent bonding.
- [`ble_passkey_peripheral.bmx`](examples/ble_passkey_peripheral.bmx): authenticated passkey pairing.
- [`ble_connection_management.bmx`](examples/ble_connection_management.bmx): connection, RSSI, parameters, PHY, and bond management.

BLE callbacks are converted into normal deferred BlitzMax events. Call
`PollSystem`, `WaitSystem`, or use `BRL.EventQueue` so those events are drained.

## OTA updates

`ESP32.System.OTA` accepts firmware from a byte array, caller-owned raw memory,
or any `TStream`, including LittleFS, SD, and network streams. Finishing an
update validates the complete ESP-IDF application image; activating it is a
separate decision.

After a provisional boot, the new application should run its own startup checks
and confirm the image:

```blitzmax
Local state:UInt
If OTARunningImageState(state) = 0 And state = OTAImageStatePendingVerify Then
    If OTAMarkRunningImageValid() <> 0 Then RuntimeError "Could not confirm OTA image"
End If
```

[`ota_network_receiver.bmx`](examples/ota_network_receiver.bmx) and
[`ota_upload_client.bmx`](examples/ota_upload_client.bmx) demonstrate a simple
desktop-to-device development workflow. Their small TCP protocol has no peer
authentication or transport encryption and must not be exposed as a production
Internet-facing update service.

The Baguette S3 partition layout contains two 2 MiB OTA slots and 4024 KiB of
LittleFS. The Baguette C3 layout contains two 1.5 MiB OTA slots and 952 KiB of
LittleFS. When moving from another partition layout, flash the generated
bootloader, partition table, application, and initial OTA metadata together.
Back up persistent data first because changing partition offsets makes the old
filesystem inaccessible.

## Hardware regression tests

The hardware runner builds, flashes, and checks self-verifying examples on a
connected board. It supports `baguette_s3`, `esp32s3_44pin_n16r8`, and
`xiao_esp32s3_plus`; the complete suite has been verified on the Baguette S3.
It checks the detected chip and flash against the board profile before
flashing and stops if they disagree. It does not format storage, join Wi-Fi, or
alter OTA partitions beyond the normal application upload.

Set the serial port for the connected board and run the basic suite:

```sh
ESP32_TEST_BOARD=baguette_s3 ESP32_TEST_PORT=/dev/cu.usbmodem101 \
    ./tests/run_hardware.sh basic
```

For the RMT receive/transmit loopback, connect GPIO4 to GPIO5 and run
`./tests/run_hardware.sh loopback` with the same variables. `all` runs both
suites. On the 44-pin S3, use its USB-to-UART `COM` socket. The runner needs
Python with `pyserial`; set `ESP32_TEST_PYTHON` to the ESP-IDF Tools Python if
your system Python does not have it. Test output is checked after an automatic
board reset, and the runner stops on the first missing pass result.

On the PSRAM-equipped XIAO S3 Plus or 44-pin S3, run
`./tests/run_hardware.sh psram` to verify that a managed arena can be placed
in PSRAM. The XIAO S3 Plus uses its native USB Serial/JTAG port.

## Troubleshooting

- **Architecture mismatch:** omit `-g` and let the board profile select it, or
  use `-g xtensa` for the original ESP32 and S3 and `-g riscv32` for C3/C6.
- **More than one serial device:** set `ESPPORT` or `esp32.port`.
- **Uncertain board configuration:** compare `boardinfo` with `deviceinfo`.
- **Application does not fit:** inspect the reported image size and the selected
  profile's application partitions.
- **SD mount fails:** confirm that the profile defines an SD resource, a card is
  inserted, and it has a supported FAT filesystem. Formatting is never the
  default recovery action.
- **No deferred network/BLE events:** ensure the program regularly calls
  `PollSystem`, `WaitSystem`, or uses `BRL.EventQueue`.

## Further documentation

- [Board profiles](boards/README.md)
- [Runtime integration and contributor notes](docs/runtime-integration.md)
- [Examples](examples)
