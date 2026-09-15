# esp32.mod

ESP-IDF platform support for BlitzMax NG embedded applications.

The shared compact runtime ABI, allocator, garbage collector, strings, arrays,
objects, and exceptions live in `embedded.mod`. This repository supplies only
the ESP32 platform adapter, ESP-IDF application integration, and ESP32 APIs.

On ESP32, the managed arena is reserved once from internal, byte-addressable
RAM through ESP-IDF's capability allocator. The collector owns that complete
arena for the lifetime of the application; unmanaged ESP-IDF allocations stay
outside it. `-heap` selects the reservation size and must be a multiple of 16
bytes.

The supported targets are the original ESP32 and ESP32-S3 using Xtensa, and
ESP32-C3 using 32-bit RISC-V, with ESP-IDF 6.1. The Baguette S3 and Baguette C3
profiles are tested on their respective physical boards.

The ESP32 module tree mirrors the Pico tree where the hardware concepts align.
Portable applications can use modules such as `Embedded.System.Time`; code
which needs the complete target API can use the familiar
`ESP32.System.Time` counterpart. The ESP32 time module adds the current CPU
clock and generation-checked one-shot and repeating native alarms while keeping
managed BlitzMax execution out of the ESP timer task.

`ESP32.Hardware.GPIO` mirrors the portable and Pico GPIO operation names while
also exposing ESP-IDF's native drive-capability levels. Portable applications
can continue to import `Embedded.Hardware.GPIO` unchanged.

`ESP32.Hardware.UART` mirrors Pico's low-level controller, pin, format,
flow-control, blocking I/O, timeout, and status operations. ESP-IDF's native
rings remain an implementation detail. ESP32-specific additions expose the
third controller, flexible GPIO-matrix routing, input flushing, and internal
loopback. It supports both ESP-IDF's finite, bit-counted transmit break and a
persistent Pico-style break that temporarily holds the configured TX pin low.

`ESP32.Hardware.I2C` and `ESP32.Hardware.SPI` are familiar target facades over
the shared controller APIs. ESP-IDF supplies flexible pin routing, atomic I2C
write/read transactions, and SPI DMA-backed transfers internally. Chip select
is deliberately controlled through GPIO so the portable SPI contract remains
equivalent across targets. `ESP32.Text.Unicode` similarly exposes the optional
shared Unicode String case tables under the target namespace.

`ESP32.Random` registers the ESP32 hardware RNG with `Random.Core`, provides
raw word and buffer-filling operations, and exposes explicit internal entropy
source control for applications which need true-random output without enabling
Wi-Fi or Bluetooth. That source must not overlap use of the ADC, I2S on the
original ESP32, or the RF subsystems.

`ESP32.Hardware.ADC` mirrors the portable pin-oriented ADC operations and adds
ADC unit/channel discovery, attenuation control, and calibrated millivolt
readings when the SoC supports an ESP-IDF calibration scheme. The portable API
uses raw readings because calibrated voltage ranges are target-specific.

`ESP32.Hardware.PWM` mirrors the portable PWM operations using ESP-IDF LEDC.
Channels are allocated automatically, equal-frequency outputs share timers,
and target code can inspect the allocated channel, timer, and resolution.

`ESP32.Hardware.Watchdog` exposes the shared watchdog lifecycle through
ESP-IDF's task watchdog, monitoring the calling BlitzMax task while preserving
the project's configured idle-task monitoring.

`Embedded.System.Power` provides capability-checked portable idle and sleep
operations. `ESP32.System.Power` mirrors those operations using returning
ESP-IDF light sleep and separately exposes timer-driven deep sleep because deep
sleep resumes through a reboot. Pico retains its additional dormant and
low-leakage-pin capabilities through the same portable contract.

`ESP32.System.Calendar` controls ESP-IDF system time as UTC and provides a
deferred absolute alarm. Setting it is immediately visible through the normal
`Pub.Time` functions, including `CurrentDateTime` and `CurrentUnixTime`.

Boards with external RAM can place the precise managed arena in PSRAM with
`-heap-region psram`. The selected profile must declare a fixed PSRAM capacity
and provide the appropriate ESP-IDF initialization settings; unsupported boards
fail before compilation. `-heap auto` reserves all but 64 KiB of profile PSRAM.
`ESP32.Hardware.PSRAM` reports initialization, capacity, free space, largest
free block, address membership, and the managed arena's placement. The generic
`esp32s3_n8r8` profile describes an 8 MB flash/8 MB octal-PSRAM module.

`ESP32.System.Device` exposes the factory eFuse base MAC as the portable unique
device identifier, normalized and native reset reasons, and chip model,
revision, core-count, and feature information.

`ESP32.Storage.LittleFS` mounts the board profile's `storage` partition and
installs it as the default filesystem. A completely erased partition can be
formatted automatically; existing unrecognised data is never erased
implicitly. `ESP32.Storage.SDCard` mounts a profile-defined native SDMMC slot
without formatting by default. Both use the normal `BRL.FileSystem`, stream,
and text APIs:

```blitzmax
Import BRL.TextStream
Import ESP32.Storage.LittleFS
Import ESP32.Storage.SDCard

SaveText "settings", "config.txt"              ' default: internal LittleFS
SaveText "archive", "sd::archive.txt"          ' explicit SD card
SetDefaultStorageVolume "sd"
SaveText "logs", "file::latest.txt"            ' file:: follows the default
```

`littlefs::` and `sd::` always select a volume. Plain paths and `file::` use
the selected default. Mounting SD does not replace an already-mounted
LittleFS default, and cross-volume renames fail; `CopyFile` works between
volumes. Formatting either medium is always an explicit destructive request.

`ESP32.Storage.NVS` exposes ESP-IDF's wear-levelled key/value store through
explicit namespace handles and commits. It supports signed and unsigned
32-bit and 64-bit integers, floats, doubles, strings, and byte arrays while
retaining the exact ESP-IDF result code so a missing key is distinguishable
from a stored zero or empty value. Blob overloads also copy to and from raw
`Byte Ptr` memory with an explicit size or capacity; ownership always remains
with the caller. `examples/nvs_storage.bmx` verifies that managed and raw
typed data survives a software reboot and then removes its test namespace.

`ESP32.System.Partition` provides read-only snapshots of the active flash
partition table: labels, native types and subtypes, address, size, erase size,
encryption and read-only flags, and the currently running application image.
It deliberately has no raw write or erase operations.

`ESP32.System.OTA` performs validated, transport-neutral application updates.
Firmware bytes can be supplied directly, from a byte array, or from any
`TStream`, including files on LittleFS or SD and network streams. `Finish`
validates the complete ESP-IDF application image without changing the boot
selection; `Activate` is a deliberately separate decision. Open updates are
aborted automatically when their handle is closed or collected.

An application booted provisionally after an update can inspect
`OTARunningImageState()` and call `OTAMarkRunningImageValid()` only after its
own startup checks pass. It may instead mark the image invalid, allowing the
next boot to return to the previously valid slot. `examples/ota_from_storage.bmx`
shows an update read through the normal storage and stream APIs.

For a development-network example, flash `examples/ota_network_receiver.bmx`,
then build and run `examples/ota_upload_client.bmx` as a desktop application:

```sh
bmk makeapp -a -r -l esp32 -g xtensa -board baguette_s3 -x examples/ota_network_receiver.bmx
bmk makeapp -a -r -o ota_upload_client examples/ota_upload_client.bmx
./ota_upload_client 192.168.1.170 path/to/application.bin
```

The receiver's `sd::wifi.conf` contains the SSID on its first line and password
on its second. The uploaded application should confirm a successful provisional
boot after completing its own checks:

```blitzmax
Local state:UInt
If OTARunningImageState(state) = 0 And state = OTAImageStatePendingVerify Then
	If OTAMarkRunningImageValid() <> 0 Then RuntimeError "Could not confirm OTA image"
End If
```

This small TCP protocol deliberately provides no authentication or transport
encryption; it is a local development example, not an Internet-facing update
service. A production transport should authenticate its peer and normally add
application-level signing policy in addition to ESP-IDF image validation.

The Baguette S3 profile now has two 2 MiB application slots, rollback metadata,
and 4024 KiB of internal LittleFS storage. This replaces its earlier factory
application plus larger LittleFS layout. Flash the generated bootloader,
partition table, application, and initial OTA metadata together when adopting
the new layout; data stored using the old LittleFS offsets must be backed up
first.

`Embedded.Network.WiFi` provides the portable station interface for radio
initialisation, asynchronous scanning, connection state, IPv4 configuration,
and deferred events. `Pico.Network.WiFi` and `ESP32.Network.WiFi` are familiar
target facades over that same contract. `ESP32.Network.WiFi` additionally
controls modem power saving, transmit power, station protocols and bandwidth,
reports a detailed live station snapshot, and can run a SoftAP alongside the
station connection. Credentials are application data and are never compiled
into the modules.

`Embedded.Network.BLE` provides a portable BLE lifecycle, discovery, central,
and GATT peripheral layer.
Initialisation is asynchronous, with explicit ready and reset events. Active
or passive scans deliver ordinary BlitzMax events containing the peer address
and type, RSSI, advertisement kind, raw advertisement bytes, local name and
manufacturer data. Native callbacks use a fixed queue; managed objects are
created only when `PollSystem` or `WaitSystem` drains it. Applications can
define primary services and readable, writable, notifying, or indicating
characteristics before initialization, then advertise a selected service.
Characteristic values live in fixed native storage so BLE reads are answered
synchronously without entering managed code from the NimBLE task. Connections,
writes, and subscription changes are delivered as deferred BlitzMax events;
changing a characteristic can notify its subscribed clients.
Central applications can connect to a discovered peer, enumerate its services,
characteristics, and descriptors, read and write remote attributes, explicitly
configure notification or indication subscriptions through the remote client
configuration descriptor, and receive values as deferred events. Discovery and
value objects preserve their remote handles without exposing NimBLE structures.
Connections can negotiate and query their ATT MTU. Reads automatically use the
long-read procedure and acknowledged writes automatically use prepared writes
when their value exceeds a single ATT packet; characteristic and descriptor
values may contain up to the standard GATT limit of 512 bytes. Writes without a
response remain single-packet operations, with their current payload limit
reported by `BLEMaximumWriteWithoutResponse`.

Peripheral applications can send a notification to one connection or all
subscribers, or send an acknowledged indication to a specific connection.
`EVENT_BLEGATTUPDATECOMPLETE` reports transmission errors and indication
confirmation, while `EVENT_BLEMTUCHANGED` reports negotiated MTU changes.
`ESP32.Network.BLE` selects ESP-IDF's NimBLE host and adds ESP32 result names.
Importing BLE enables the controller and NimBLE components for that application;
programs which do not import it retain the smaller non-Bluetooth build.
`examples/ble_scan.bmx` demonstrates a ten-second active discovery scan, while
`examples/ble_peripheral.bmx` provides a read/write/notify service which echoes
client writes back as notifications. `examples/ble_client.bmx` exercises the
opposite role against the example service (with the local name as a fallback),
including the complete discover/read/subscribe/write/notification round trip.
`examples/ble_gatt_long_client.bmx` verifies MTU negotiation plus a 512-byte
read, prepared write, and read-back against a suitable test peripheral.
`examples/ble_indication_peripheral.bmx` demonstrates a targeted indication
and observes its acknowledgement as an ordinary BlitzMax event.
`examples/ble_secure_peripheral.bmx` demonstrates an encrypted characteristic,
LE Secure Connections pairing, a persistent bond, and security-state events.
`examples/ble_passkey_peripheral.bmx` demonstrates authenticated pairing with a
displayed passkey and a characteristic which requires MITM protection.
`examples/ble_connection_management.bmx` demonstrates active-connection and
bond enumeration, connection parameter updates, RSSI inspection, and PHY
selection. Public timings use microseconds for the connection interval and
milliseconds for the supervision timeout rather than Bluetooth wire units.

Security policy is configured before `BLEInitialize` with
`BLEConfigureSecurity`. Applications can select their input/output capability,
bonding, MITM authentication, Secure Connections, and Secure-Connections-only
mode. Protected characteristic and subscription permissions use portable GATT
flags. Pairing input and numeric comparison are delivered through
`EVENT_BLEPASSKEYACTION`; encryption, authentication, bonding, and negotiated
key size are reported by `EVENT_BLESECURITYCHANGED`. Bond keys are persisted in
ESP32 NVS and can be inspected or removed with `BLEBondCount` and
`BLEBondAt`, an individual bond's `Forget` method, and `BLEForgetAllBonds`.
Active links can be inspected with `BLEConnectionCount`, `BLEConnectionAt`, or
`BLEConnectionInfo`. Parameter and PHY changes produce ordinary BlitzMax
events, while RSSI and current PHY can also be queried synchronously.

`BRL.Socket` now uses the native embedded network stack on both targets. ESP32
uses ESP-IDF/lwIP BSD sockets and Pico retains its raw-lwIP adapter, while UDP,
TCP, address lookup, byte availability, socket options, and deferred readable,
writable, accept, close, and error events have the same BlitzMax API. Network
components are linked only when an application imports Wi-Fi or `Pub.Net`.
`examples/wifi_scan.bmx` scans without credentials and
`examples/wifi_esp32.bmx` exercises target-specific radio controls and a WPA2
test SoftAP without requiring an upstream network. `examples/wifi_live.bmx`
reads an SSID and password from the first two lines of `sd::wifi.conf`, then
checks association, DHCP, DNS, UDP/NTP, and TCP/HTTP without embedding secrets
in the application. `examples/network_failure.bmx` uses the same SD file to
exercise authentication rejection, recovery, disconnect/reconnect, DNS and
bind failures, timeouts, teardown guards, and repeated socket/event reuse.
`examples/socket_loopback.bmx` tests local UDP, TCP,
address conversion, byte availability, and readiness
events without requiring a wireless connection.

## Build

`bmk` finds ESP-IDF through `IDF_PATH`, the `esp32.idf` custom option, or an
installation under `~/.espressif`. Build an application with:

```sh
bmk makeapp -a -r -l esp32 -g xtensa -heap 64k -o hello examples/hello_world.bmx
```

Select an ESP32-S3 with `-board esp32s3` (the alias `s3` is also accepted), or
select The Pi Hut board with `-board baguette_s3`. Without `-board`, the
original ESP32 is used unless `esp32.target` is set in `custom.bmk`.

The `baguette_s3` profile selects the board's 8 MB flash and native USB
Serial/JTAG console, and supplies its documented I2C, SPI, UART, MicroSD, RGB
LED, button, and pin mappings. The `baguette_c3` profile selects its 4 MB flash,
native USB Serial/JTAG console, I2C, SPI, UART, Qwiic, RGB LED, button and pin
mappings. It intentionally has no SD-card resource. Its flash layout provides
two 1.5 MiB OTA application slots and 952 KiB of internal LittleFS storage.

Build for the Baguette C3 with:

```sh
bmk makeapp -a -r -l esp32 -g riscv32 -board baguette_c3 -heap 64k -o hello examples/hello_world.bmx
```

Inspect a profile without connecting hardware:

```sh
bmk boardinfo -l esp32 -board baguette_s3
```

Profiles are directories under `boards`, containing a versioned `board.ini`,
an optional `pins.csv`, and board-local build defaults. See
`boards/README.md` for the schema. Add private or third-party profile roots
with `esp32.board.dirs` in `custom.bmk` or `ESP32_BOARD_DIRS`; board names and
aliases must be unique across all roots.

The build publishes `hello.elf`, `hello.bin`, and `hello.map`. ESP-IDF keeps
the bootloader, partition table, flash arguments, and per-application
`sdkconfig` in the source file's `.bmx` build directory.

Pass `-x` to build, upload, reset, and start the application in one command.
ESP-IDF automatically selects the serial device when exactly one compatible
board is attached. Set `ESPPORT` or the `esp32.port` custom option when a
specific device must be selected.

Inspect a connected device without building or flashing an application with:

```sh
bmk deviceinfo -l esp32
```

This reports hardware facts obtained through ESP-IDF's `esptool`. An optional
`-board` value is shown separately as build configuration and is not inferred
from the chip. If its configured image flash size differs from the detected
physical flash, the report includes a warning.

The current runtime is single-threaded. Its first managed-arena acquisition
binds it to the current FreeRTOS task on ESP32 CPU 0. Later managed operations
from another task, CPU 1, or interrupt context are rejected and counted by
`ManagedContextViolationCount()`.

## Native callback contract

Native integrations must enter BlitzMax through
`bmx_esp32_managed_callback_dispatch()`. The dispatcher invokes callbacks only
on the CPU-0 task that owns the managed arena. Interrupt handlers and other
FreeRTOS tasks must enqueue plain native event data and let the owner task drain
it; they must never allocate, collect, throw, or call BlitzMax code directly.

Raw managed pointers are borrowed for the duration of a native call. Native
code that keeps an Object after the call returns must retain it with
`bmx_embedded_object_root_retain()` on the owner task and release the returned
token there with `bmx_embedded_object_root_release()`. Strings and arrays must
remain reachable through that rooted Object rather than being retained as
untracked raw pointers. A managed exception must be caught before returning
through the native callback boundary.

`examples/managed_smoke.bmx` provides a quick managed-runtime check.
`examples/managed_stress.bmx` repeatedly exercises automatic and explicit
collection, cyclic garbage, inheritance, object and array tracing, strings,
fragment reuse, and retained graph integrity.
`examples/managed_fault_safety.bmx` checks the native boundary matrix: valid
owner-task dispatch, null and foreign-task callback rejection, foreign-task
allocation rejection, rejected foreign-task root retention/release, root
survival, later owner-task release, and heap integrity.
`examples/managed_oom.bmx` deliberately exhausts the arena and verifies
automatic collection and retry, raw-memory release and reuse, failed-resize
preservation, retained managed roots, catchable Object/Array/String allocation
failures, finalizer-exception unwinding, and successful allocation after
recovery.
