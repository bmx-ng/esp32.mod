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
| ESP32-C3 | 32-bit RISC-V | `baguette_c3` | 4 MiB flash, no SD slot or PSRAM |

The Baguette S3 and C3 profiles have been tested on their respective physical
boards. Additional generic profiles describe other ESP-IDF target families for
inspection and future validation; their presence does not by itself mean that
the complete BlitzMax implementation has been tested on that target.

## Requirements

- A BlitzMax NG installation containing the ESP32-enabled `bmk` and compiler.
- `esp32.mod` installed as `mod/esp32.mod`.
- The matching `embedded.mod`, `blitzmax.mod`, `brl.mod`, `pub.mod`, and
  `random.mod` versions.
- ESP-IDF 6.1 and its target toolchains.
- A data-capable USB connection for one-command upload.

`bmk` locates ESP-IDF using, in order, the `IDF_PATH` environment variable, the
`esp32.idf` option in `custom.bmk`, or an installation beneath
`~/.espressif`.

## Quick start

Create `hello.bmx`:

```blitzmax
SuperStrict

Framework BRL.StandardIO

Print "Hello from BlitzMax on ESP32"
```

Build and upload it to a connected Baguette S3:

```sh
bmk makeapp -a -r -l esp32 -g xtensa -board baguette_s3 -heap 64k -x hello.bmx
```

For a Baguette C3, select the RISC-V architecture instead:

```sh
bmk makeapp -a -r -l esp32 -g riscv32 -board baguette_c3 -heap 64k -x hello.bmx
```

The important options are:

| Option | Meaning |
| --- | --- |
| `-l esp32` | Build for the ESP32 platform |
| `-g xtensa` | Use the original ESP32/ESP32-S3 architecture |
| `-g riscv32` | Use the ESP32-C3 architecture |
| `-board <name>` | Select a board profile |
| `-heap 64k` | Reserve a 64 KiB BlitzMax managed heap |
| `-x` | Build, upload, reset, and start the application |
| `-o <name>` | Choose the output name |

Without `-x`, the build produces an ELF image, flashable BIN image, and link
map. ESP-IDF's bootloader, partition table, flash arguments, and generated
configuration remain in the source file's `.bmx` build directory.

When exactly one compatible board is connected, upload selects it
automatically. Set `ESPPORT` or `esp32.port` in `custom.bmk` when more than one
device is available.

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
ESP-IDF settings. Unsupported combinations fail before compilation. `-heap auto`
reserves all but 64 KiB of the profile's declared PSRAM. Use
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

## Troubleshooting

- **Architecture mismatch:** use `-g xtensa` for the original ESP32 and S3, or
  `-g riscv32` for C3.
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
