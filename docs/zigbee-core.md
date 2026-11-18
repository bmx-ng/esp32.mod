# Zigbee stack API

`ESP32.Network.Zigbee` uses the pinned ESP Zigbee SDK 2.1.0 component with
ESP-IDF 6.1. It currently provides a configurable stack lifecycle, network
commissioning, a bounded event queue, and standard Home Automation On/Off
light, switch, and temperature sensor endpoints. Importing the module adds
the pinned Zigbee component only to applications that use it. The component
manager downloads it on the first online build and caches it for later builds.

## Build configuration

The Zigbee import selects its own `sdkconfig.zigbee.<mode>` and partition table.
Without the import, a normal application keeps its board configuration and may
use any valid partition layout for that board. The Zigbee default partition
layout includes `zb_storage` and `zb_fct`, alongside the normal NVS, OTA and
LittleFS partitions. `esp32.zigbee.partitions` selects a custom table; its path
may be absolute or relative to the application's source directory and must name
a table containing `zb_storage` as `data,nvs`.

The build selects `zczr` (coordinator/router) on H2 and `zed` (end device) on
C6 by default. Set `esp32.zigbee.stack` to `zczr` or `zed` in
`bin/custom.bmk` for an installation default, or in `pre.bmk` beside the
source for an application override. Use `<output-name>.pre.bmk` to target one
application output. For example, a switch application's file can contain:

```bmk
setoption esp32.zigbee.stack zed
```

The light and switch examples include their own app-specific `pre.bmk` files,
so they choose the correct library on either board. Each mode has its own
sdkconfig variant, so switching roles rebuilds the correct SDK
library. A coordinator or router role requires `zczr`; an end-device role
requires `zed`. `ZigbeeStart` rejects a role that does not match the linked
library.

Set a custom partition table in the same configuration file with
`setoption esp32.zigbee.partitions "partitions.csv"`. The bmk options take
precedence over the older `BMX_ESP32_ZIGBEE_STACK_MODE` and
`BMX_ESP32_ZIGBEE_PARTITIONS` environment variables, which remain available
for existing build scripts.

## Application flow

Call `ZigbeeStart(role, device, endpoint, primaryMask, secondaryMask,
autoCommission)`. Call `ZigbeeAddEndpoint` before starting to register up
to eight more standard light, switch, or temperature sensor endpoints. The
extra endpoint list is retained across stop/start cycles;
`ZigbeeClearExtraEndpoints` clears it while stopped. The default channel mask
selects channel 15. Startup is
asynchronous; process `ZigbeePollEvent` from the application thread. With
auto commissioning enabled and no saved Zigbee network, a coordinator forms a
network, while a router or end device attempts to join one. A coordinator
starts network steering after formation. With auto commissioning disabled,
wait for a successful `ZigbeeEventStartup` and call `ZigbeeFormNetwork` or
`ZigbeeJoinNetwork` explicitly. `ZigbeePermitJoining` opens or closes joining.
`ZigbeeGetNetworkInfo` returns current network details.

## Install-code commissioning

Install-code commissioning is optional and disabled by default. Both the
coordinator and joiner must call `ZigbeeEnableInstallCodes()` while stopped,
before `ZigbeeStart`. The setting persists across stop/start cycles until
changed. This uses the Zigbee stack configuration; it is independent of the
`bmk` stack-mode and partition options.

For a new network, start the coordinator with `autoCommission=False` and wait
for `ZigbeeEventStarted`. Then call
`ZigbeeAddJoinerInstallCode(joinerIEEEAddress, code)` for each prospective
joiner and `ZigbeeRequireInstallCodes()`. After those calls succeed, use
`ZigbeeFormNetwork` and open joining with `ZigbeePermitJoining` once formation
completes. For an existing network, provision and require codes before
opening a joining window. `ZigbeeGetIEEEAddress`
returns a running device's 64-bit IEEE address; obtain the joiner's address
before provisioning it on the coordinator. `ZigbeeRemoveJoinerInstallCode`
removes a provisioned code for future joins, and
`ZigbeeRequireInstallCodes(False)` permits ordinary commissioning again.

On a router or end device, start with `autoCommission=False`, wait for
`ZigbeeEventStarted`, call `ZigbeeSetLocalInstallCode(code)`, then
`ZigbeeJoinNetwork`. The API accepts 8, 10, 14, or 18 bytes, including the
two-byte CCITT-16 CRC specified by Zigbee BDB. Keep each device's code private
and provision its own code on the coordinator. Never embed production codes
in an example or commit them to source control. Successful API calls return
zero; check each result before opening or attempting a join. A device already
on a saved network may not recommission; clear its Zigbee network state when
testing a new joining policy.

The native Zigbee task copies event values into a fixed 64-record queue.
`ZigbeePollEvent` transfers one record to a BlitzMax value on the application
thread. Check `ZigbeeDroppedEvents` if the application might be unable to poll
fast enough. `ZigbeeStop` requests an asynchronous shutdown; wait for
`ZigbeeEventStopped` before restarting. `ZigbeeFactoryReset` clears saved
Zigbee network state, while the SDK retains the outgoing frame counter.

The On/Off switch template has a client cluster. `ZigbeeFindOnOffLights`
broadcasts a match descriptor request. Each `ZigbeeEventOnOffMatch` supplies a
light's short address and endpoint. `ZigbeeBindOnOff` binds the switch to that
light, and `ZigbeeEventOnOffBound` confirms the result. `ZigbeeSendBoundOnOff`
uses that binding for Off, On, or Toggle. `ZigbeeSendOnOff` also permits direct
sending to a 16-bit network address; `ZigbeeReadOnOff` requests the remote
On/Off attribute. A successful return means the SDK accepted the request,
and a later `ZigbeeEventRead` carries a read response. The light template has
an On/Off server and reports changes as `ZigbeeEventAttribute`.
`ZigbeeSetLocalOnOff` lets the application update that server attribute when
its own hardware controls the light. The application must also update its
physical output.

The temperature sensor template exposes the standard Temperature Measurement
cluster (0x0402). Call `ZigbeeSetLocalTemperature` with a measurement in
hundredths of a degree Celsius. Remote clients can discover it with
`ZigbeeFindServers(ZigbeeClusterTemperatureMeasurement)` and read attribute
zero with `ZigbeeReadAttribute`; a successful read event has type `Int16`.
For reports, bind the sensor's server cluster to the client's endpoint and
request reporting with `ZigbeeConfigureReporting` using `ZigbeeTypeInt16`.
The examples use a simulated value that alternates between 23.45 °C and
23.55 °C; the switch receives those changes as `ZigbeeEventReport`.

Call `ZigbeeAddHumidityCluster(endpoint, ZigbeeClusterServer)` before starting
to add a Relative Humidity Measurement server to an existing endpoint. Add the
client role on a remote reader's endpoint. `ZigbeeSetLocalHumidity` accepts a
measurement from 0 to 10000 in hundredths of a percent. A client discovers
cluster 0x0405 with `ZigbeeFindServers`, reads attribute zero as `UInt16`, and
can request reports with `ZigbeeConfigureReporting`. The examples attach the
server to the temperature endpoint and alternate between 45.50% and 45.60%.
`ZigbeeClearHumidityClusters` removes these additions while stopped.

Call `ZigbeeAddIlluminanceCluster(endpoint, role)` before starting to add an
Illuminance Measurement server or client to an existing endpoint. Use
`ZigbeeSetLocalIlluminance` with the ZCL encoded 16-bit MeasuredValue; 65535
means unavailable, and the setter accepts 0 through 65534. Clients can
discover cluster 0x0400, read attribute zero as `UInt16`, and request reports
with `ZigbeeConfigureReporting`. The examples alternate encoded values 20001
and 20101, and the switch checks both readback and a changed report.
`ZigbeeClearIlluminanceClusters` removes these additions while stopped.

Call `ZigbeeAddPressureCluster(endpoint, role)` before starting to add a
Pressure Measurement server or client. `ZigbeeSetLocalPressure` updates its
signed 16-bit MeasuredValue in whole kilopascals; -32768 means unavailable.
A client can discover cluster 0x0403, read attribute zero as `Int16`, and
configure reports on that attribute.

Call `ZigbeeAddOccupancyCluster(endpoint, role, sensorType)` before starting
to add an Occupancy Sensing server or client. The default sensor type is PIR;
ultrasonic, combined, and physical-contact types are also supported.
`ZigbeeSetLocalOccupancy` sets attribute zero to zero (unoccupied) or one
(occupied). Clients can discover cluster 0x0406, read the `Bitmap8` attribute,
and request reports. `ZigbeeClearPressureClusters` and
`ZigbeeClearOccupancyClusters` remove their respective additions while stopped.

`ZigbeeAddPowerConfigurationCluster(endpoint, role)` adds the standard Power
Configuration cluster to any configured endpoint. A server exposes battery
voltage (attribute 0x0020, tenths of a volt) and percentage remaining
(attribute 0x0021, half-percent units). `ZigbeeSetLocalBatteryVoltage` accepts
0 to 254 or 255 for unknown; `ZigbeeSetLocalBatteryPercentage` accepts 0 to
200 or 255 for unknown. A client can discover the server, read both attributes
as `UInt8`, and configure percentage reporting. The light example uses
simulated 3.0 V and 74 to 75 percent readings.

Before starting, `ZigbeeAddCustomCluster` can attach a custom server or client
cluster (ID 0x8000 through 0xFFFF) to any configured endpoint.
`ZigbeeAddCustomScalar` adds a readable, optionally writable and reportable
Boolean or 8/16/32-bit scalar attribute to a custom server cluster. Up to
eight custom clusters and 32 custom attributes may be configured;
`ZigbeeClearCustomClusters` clears them while stopped. The C6 light example
adds cluster 0xFC00 on endpoint 11, with a writable 16-bit attribute. The H2
switch discovers the cluster, writes the attribute, and reads it back.
`ZigbeeSetCustomScalar` updates a local custom server attribute from the
application, so a remote client can read its new value.
`ZigbeeAddCustomString` registers a ZCL character string attribute with up to
64 UTF-8 bytes. `ZigbeeSetCustomString` updates it locally, and
`ZigbeeWriteString` writes a remote string attribute. The switch example writes
`Zigbee é` and reads it back; the light then restores `ready` locally, which
the switch reads again. `ZigbeeConfigureReporting` currently handles scalar
types only.

`ZigbeeAddCustomOctets` registers a binary octet-string attribute with up to
64 bytes, including zero and non-UTF-8 byte values. `ZigbeeWriteOctets`
writes a remote attribute and `ZigbeeSetCustomOctets` updates a local one.
An incoming read or attribute event carries its bytes in `SZigbeeEvent.bytes`
with `dataType` `ZigbeeTypeOctets` and `valueSize` set to the byte count.
The public `ZigbeeTypeBitmap8`, `ZigbeeTypeBitmap16`, `ZigbeeTypeEnum8`, and
`ZigbeeTypeEnum16` constants name types already accepted by the scalar read,
write, and report paths.

`ZigbeeAddCustomEndpoint` adds an endpoint with an application-chosen profile
ID, device ID, and four-bit version. It includes a Basic server; add other
clusters with `ZigbeeAddCustomCluster`. Up to eight custom endpoints can be
registered before `ZigbeeStart`, alongside the eight extra standard endpoints.
`ZigbeeClearCustomEndpoints` removes their identities while stopped; clear
their custom clusters separately. To discover a non-HA endpoint, pass its
profile ID as the second argument to `ZigbeeFindServers`. A source endpoint
with the same profile and a matching client cluster can then read its remote
attributes. The examples use profile `0xC0DE` on endpoints 13 and 2.

`ZigbeeFindServers` broadcasts a match request for any server cluster and
returns `ZigbeeEventMatch` records with address, endpoint, and cluster.
`ZigbeeBindCluster` binds any local cluster to a remote endpoint, with a
`ZigbeeEventBound` response. The On/Off functions remain convenience wrappers;
the switch example uses the general discovery and binding calls.

`ZigbeeConfigureOnOffReporting` requests change and periodic reports from a
light. The response arrives as `ZigbeeEventReportingConfigured`; incoming
reports arrive as `ZigbeeEventReport`. The light needs a binding from its On/Off
server endpoint back to the switch's client endpoint so reports have a
destination. The light example creates this binding when it sees the switch
announce itself. `ZigbeeConfigureReporting` accepts any supported scalar
attribute and its ZCL type, intervals, and numeric change threshold. The
switch example uses that general form for the On/Off attribute.

The light template includes a Groups server. The switch template adds a Groups
client alongside its On/Off client. `ZigbeeAddGroup` and `ZigbeeRemoveGroup`
manage a remote light's membership, with response events for the outcome.
`ZigbeeViewGroup` checks one group and returns `ZigbeeEventGroupViewed` with its
ID and status. `ZigbeeGetGroupMembership` lists all groups on one remote
endpoint: zero or more `ZigbeeEventGroupMember` records are followed by
`ZigbeeEventGroupMembershipComplete`, whose value is the number of groups and
whose attribute field is the remaining group table capacity. The group query
events identify the remote device with `shortAddress` and the local client
endpoint with `endpoint`.
`ZigbeeRemoveAllGroups` clears every group on one remote endpoint. It has no
specific response; request its membership list afterward to confirm the table
is empty.
`ZigbeeSendGroupOnOff` sends an On/Off command to every endpoint in the group.
The switch example adds group 4660, queries its membership, sends group
toggles, clears the table, then adds the group again.

`ZigbeeReadAttribute` reads one attribute from any remote cluster. A
`ZigbeeEventRead` provides its ZCL `dataType`, `valueSize`, and `value` for
Boolean, 8-bit, 16-bit, and 32-bit integer, bitmap, enum, and raw data types.
Signed values are sign-extended in `value`; for a 32-bit unsigned value, use
its raw bit pattern. For `ZigbeeTypeString`, `text` holds the decoded UTF-8
value and `valueSize` counts its bytes. For `ZigbeeTypeOctets`, `bytes` holds
the binary value. A string longer than 64 bytes has `valueSize` -1 and no
`text` or `bytes`; an unsupported type has `valueSize` zero. The same
fields are available on incoming attribute reports. `ZigbeeWriteScalar`
writes one of those scalar types, with a `ZigbeeEventWrite` response carrying
the remote status. The switch example reads the light's cluster revision and
writes then reads back its IdentifyTime attribute.

## Zigbee OTA firmware updates

`ZigbeeAddOtaClient(endpoint, manufacturer, imageType, version)` adds the OTA
Upgrade client cluster before `ZigbeeStart`. A matching server can notify the
client, or the client can discover the server with `ZigbeeFindServers(0x0019)`
and call `ZigbeeOtaQuery`. The native client checks the Zigbee OTA file header,
requires a strictly newer version and one raw firmware sub-element, and streams
that firmware into the inactive ESP32 OTA slot. ESP-IDF validates the firmware
image before it becomes the boot partition. The client reboots after the
server's upgrade response. This path requires an OTA partition layout. With
bootloader rollback enabled, the updated application must call
`OTAMarkRunningImageValid` after its startup checks; the reference client does
so after Zigbee starts successfully.

`ZigbeeAddOtaServer(endpoint)` adds an image server. Once running,
`ZigbeeOtaServeImage` registers one complete Zigbee OTA file from stable memory
such as `IncbinPtr`; retain that memory until `ZigbeeOtaRemoveImage` or
`ZigbeeStop`. `ZigbeeOtaNotify` announces the image to a device. An
`SZigbeeEvent` with kind `ZigbeeEventOta` reports client or server activity:
`dataType` is the cluster role (client or server), `signal` is one of the
`ZigbeeOta*` progress constants, `value` is a version for query responses or
byte progress during transfer, and `attribute` is the total byte count when
known. `status` is zero on successful progress. A query response uses signal
`ZigbeeOtaQueryResponse` and its ZCL status in `status`.

The reference H2 client has separate version 1 and version 2 examples. From
`mod/esp32.mod`, build both client versions and package the version 2 application
image:

```sh
../../bin/bmk makeapp -r -l esp32 -g riscv32 -board waveshare_h2_dev_kit_n4 examples/zigbee_ota_client_v1.bmx
../../bin/bmk makeapp -r -l esp32 -g riscv32 -board waveshare_h2_dev_kit_n4 examples/zigbee_ota_client_v2.bmx
python3 tests/zigbee_ota_pack.py examples/zigbee_ota_client_v2.bin \
  examples/assets/zigbee_ota_update.ota \
  --manufacturer 0x1234 --image-type 1 --version 2
```

Flash the C6 server after packaging, then flash version 1 to the H2:

```sh
../../bin/bmk makeapp -r -x -l esp32 -g riscv32 -board xiao_esp32c6 examples/zigbee_ota_server.bmx
../../bin/bmk makeapp -r -x -l esp32 -g riscv32 -board waveshare_h2_dev_kit_n4 examples/zigbee_ota_client_v1.bmx
```

The server embeds the generated `.ota` package. The client discovers the server,
queries for the newer image, downloads it, and boots version 2. Run
`tests/zigbee_ota_pair.py` with `--server` and `--client` serial ports and
`--require-mark-valid` to check the transfer and rollback confirmation. Start
the monitor before flashing version 1 if you want the complete startup log. The
monitor requires PySerial, available in the ESP-IDF Python environment. The
generated package is ignored by Git.

This implementation accepts the mandatory 56-byte Zigbee OTA header without
optional fields and a single uncompressed firmware element, without additional
signature or delta sub-elements. Production deployments should use
ESP32 secure boot and signed firmware as appropriate to their trust model.

Hardware validation on a XIAO ESP32-C6 server and Waveshare ESP32-H2 client
completed the version 1 to version 2 transfer, image validation, boot slot
activation, reboot, and rollback confirmation.

## Examples

- `examples/zigbee_core_light.bmx`: coordinator with two light endpoints, a
  sensor endpoint with temperature, humidity, illuminance, pressure, and
  occupancy measurements, custom scalar, text, and binary attributes, and a
  custom-profile endpoint; run with a `zczr` build on H2 or C6.
- `examples/zigbee_core_switch.bmx`: end-device switch with discovery, bound
  commands, reporting, groups, and custom attribute reads and writes; run with
  a `zed` build on H2 or C6.
- `examples/zigbee_core_lifecycle.bmx`: two bounded start/stop cycles.
- `examples/zigbee_sleepy_switch.bmx`: sleepy end-device switch that yields
  between bound On/Off commands; run with a `zed` build on H2 or C6.
- `examples/zigbee_ota_client_v1.bmx`, `zigbee_ota_client_v2.bmx`, and
  `zigbee_ota_server.bmx`: two-board OTA transfer and reboot proof.

Use each board's `ESPPORT` and the normal `bmk makeapp -a -r -x -l esp32 -g
riscv32 -board <board> <example>` flow. The H2 may need a manual reset after
flashing. If changing a board between coordinator and end-device roles, clear
its saved Zigbee network state before attempting to form or join a new
network. The saved network belongs to the previous role and topology.

## Scope

The public API covers multiple standard On/Off light, switch, or temperature
sensor endpoints, optional Relative Humidity, Illuminance, Pressure Measurement,
Occupancy Sensing, and Power Configuration clusters, custom endpoint profiles, and
custom scalar, short string, and octet-string attributes. Other standard
cluster templates, additional nonscalar attribute types, further
security controls, and application-managed deep sleep are further work.
`ZigbeeRoleSleepyEndDevice` enables ESP-IDF automatic light sleep and turns the
Zigbee receiver off while idle. A `zed` build enables power management and
tickless idle, but a normal `ZigbeeRoleEndDevice` does not enable light sleep.
The application must yield between work, and may configure additional wake
sources through `ESP32.System.Power`. The role uses an eight-second keepalive.
Actual sleep current depends on the board and application peripherals and has
not yet been measured.

## Hardware checks on 18 November 2026

- The C6 exposed Power Configuration battery voltage and percentage on its
  sensor endpoint. The H2 discovered the cluster, read 3.0 V and 74 to 75
  percent, and received a changed percentage report. The full pair regression
  passed with `--require-battery` alongside all earlier checks.
- The C6 exposed Pressure Measurement (0x0403) and Occupancy Sensing (0x0406)
  on endpoint 12. The H2 discovered both, read pressure as `Int16` and
  occupancy as `Bitmap8`, and received changed reports. A custom octet-string
  attribute carried the binary sequence `01 00 FE FF` through a remote write
  and readback; the light's local setter restored `00 80 FF`. The full
  two-board regression passed with `--require-pressure --require-occupancy
  --require-octets` alongside all earlier checks.
- With a fresh Zigbee storage partition on the H2 end device, the C6
  coordinator accepted its provisioned 18-byte install code and the H2 joined
  and sent bound On/Off toggles. The coordinator returned success from both
  `ZigbeeAddJoinerInstallCode` and `ZigbeeRequireInstallCodes`.
- With the same coordinator requiring install codes, an H2 flashed without
  install-code setup and with fresh Zigbee storage reported `joined=0` on six
  consecutive network checks. This verified the required-code policy blocked
  ordinary joining in the test setup.
- After restoring the normal examples and clearing saved Zigbee network state
  on both boards, the C6 and H2 formed and joined an ordinary network. The
  full `tests/zigbee_core_pair.py` regression passed, including On/Off,
  reporting, groups, custom clusters and profiles, UTF-8 strings, and
  temperature, humidity, and illuminance readings and reports.
- The H2 sleepy switch joined its saved network and sent two bound On/Off
  toggles 15 seconds apart. The C6 coordinator/light observed both state
  changes, from Off to On and back to Off, while the switch continued to
  receive reports. Both the normal and sleepy end-device examples built with
  the new `zed` power-management configuration.

## Hardware checks on 17 November 2026

- The H2 coordinator/light and C6 end-device/switch pair toggled both states
  and read back the remote On/Off attribute through the public API.
- The H2 completed two start/stop cycles without rebooting.
- Both reverse role builds succeeded: H2 with `zed`, C6 with `zczr`. After
  clearing only their Zigbee storage partitions, the C6 coordinator/light and
  H2 end-device/switch pair toggled both states and read back the attribute.
- The H2 switch subsequently used match descriptor discovery and binding to
  toggle the C6 light through the binding table, then read back its state.
- The C6 light built and flashed with a local attribute initialization call.
  The two-board bound-toggle and readback regression still passed.
- After the C6 light bound its server endpoint back to the H2 switch,
  `tests/zigbee_core_pair.py --require-report` passed with a received On/Off
  report as well as toggles and a read response.
- `tests/zigbee_core_pair.py --require-report --require-group` passed after the
  H2 switch added the C6 light to a group and sent a group-addressed toggle.
- `tests/zigbee_core_pair.py --require-report --require-group --require-scalar
  --require-write` passed after the H2 switch read the C6 light's 16-bit
  cluster revision, wrote its 16-bit IdentifyTime attribute, and read back
  the changed value.
- The same pair test passed after the switch moved to general reporting,
  discovery, and binding calls.
- With the C6 light exposing endpoints 10 and 11, the H2 switch discovered
  both. `tests/zigbee_core_pair.py --require-extra-endpoint` passed alongside
  the reporting, group, scalar read, and write checks.
- With custom cluster 0xFC00 on endpoint 11, the H2 switch discovered the
  cluster and wrote then read back the 16-bit value 43.
- The C6 application then updated that local custom attribute to 44;
  `tests/zigbee_core_pair.py --require-local-update` confirmed the H2 read
  back 44 while the other pair checks still passed.

## Hardware check on 18 November 2026

- The C6 coordinator/light exposed a temperature sensor on endpoint 12 and
  set its measured value to 23.45 °C. The H2 end-device/switch discovered the
  Temperature Measurement server and read attribute zero as a signed 16-bit
  value of 2345. `tests/zigbee_core_pair.py --require-temperature` passed
  alongside the reporting, group, scalar, write, extra endpoint, custom
  cluster, and local update checks.
- The C6 bound its temperature server to the H2 client, which configured
  reporting. The H2 received a signed 16-bit report after the simulated
  measurement changed to 23.55 °C.
- With an optional Relative Humidity Measurement server on endpoint 12, the
  H2 discovered cluster 0x0405, read 45.50% as an unsigned 16-bit value, and
  received a report when the C6 changed its simulated value to 45.60%.
  `tests/zigbee_core_pair.py --require-humidity` passed alongside all earlier
  pair checks.
- The H2 queried group 4660 with View Group and requested the light endpoint's
  full membership list. Both responses confirmed membership; the list response
  reported one group and 15 remaining slots. The full pair check passed with
  `--require-group-query` alongside the earlier checks.
- The C6 registered manufacturer profile `0xC0DE` on endpoint 13 with custom
  cluster `0xFC01` and a 16-bit value of 77. The H2 registered a matching client
  endpoint under the same profile, discovered the server, and read back 77.
  `tests/zigbee_core_pair.py --require-custom-profile` passed alongside all
  earlier pair checks.
- The C6 custom cluster exposed a short string attribute. The H2 wrote
  `Zigbee é`, read back its nine UTF-8 bytes, then read `ready` after the C6
  changed the attribute locally. The full pair check passed with
  `--require-string` alongside all earlier checks.
- The H2 sent Remove All Groups to the C6 light, then queried membership and
  received a list with zero groups. The full pair check passed with
  `--require-group-clear` alongside all earlier checks.
- The C6 exposed Illuminance Measurement on its sensor endpoint. The H2
  discovered it, read encoded value 20001, and received a changed report of
  20101. The full pair check passed with `--require-illuminance` alongside
  all earlier checks.
