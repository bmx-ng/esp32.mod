# Build Zigbee applications with `ESP32.Network.Zigbee`

This guide shows how to make an ESP32-C6 or ESP32-H2 application join a Zigbee
network, expose a device, and exchange commands and attributes. Those are the
two targets currently enabled by this BlitzMax module's build integration;
other Espressif chips also have Zigbee-capable 802.15.4 radios. Start with the
two short programs below, then use the recipes for the features your device
needs. The [module bbdocs](../network.mod/zigbee.mod/zigbee.bmx) describe every
public function and constant; the [hardware test record](zigbee-validation.md)
tracks what has been exercised on boards.

For a first app, read [build setup](#before-you-build), run the
[light](#a-minimal-coordinator-and-light) and
[switch](#a-minimal-end-device-switch), then use the
[event pattern](#the-event-and-result-pattern). The remaining sections are
recipes you can consult as needed.

The module uses ESP Zigbee SDK 2.1.0 with ESP-IDF 6.1. Importing it adds the
pinned Zigbee component only to applications that use Zigbee. The first online
build downloads the component; later builds use the cache.

## Before you build

A coordinator forms a network and admits devices. A router forwards traffic.
An end device joins an existing network; a sleepy end device may turn off its
radio between activity. Choose the matching build mode:

| Runtime role | `ZigbeeStart` role | `esp32.zigbee.stack` |
| --- | --- | --- |
| Coordinator | `ZigbeeRoleCoordinator` | `zczr` |
| Router | `ZigbeeRoleRouter` | `zczr` |
| End device | `ZigbeeRoleEndDevice` | `zed` |
| Sleepy end device | `ZigbeeRoleSleepyEndDevice` | `zed` |

The board default is `zczr` on H2 and `zed` on C6. Put the following beside an
app as `pre.bmk`, or use `<output-name>.pre.bmk` to configure one output:

```bmk
setoption esp32.zigbee.stack zczr
```

Use `zed` for an end-device app. Set an installation default in
`bin/custom.bmk` if useful. An app option overrides that default. The older
`BMX_ESP32_ZIGBEE_STACK_MODE` environment setting remains supported, but the
bmk option takes precedence. The [light](../examples/zigbee_core_light.bmx)
and [switch](../examples/zigbee_core_switch.bmx) examples have app-specific
configuration files for their roles. `ZigbeeStart` rejects a role that does
not match the linked stack.

A Zigbee import also selects a Zigbee partition table with `zb_storage`,
`zb_fct`, NVS, OTA and LittleFS partitions. Other applications retain their
normal board partition configuration. To supply a Zigbee table:

```bmk
setoption esp32.zigbee.partitions "partitions.csv"
```

The path is absolute or relative to the app source directory. The table must
contain `zb_storage` as `data,nvs`. The older
`BMX_ESP32_ZIGBEE_PARTITIONS` setting is still supported. For example, from
`mod/esp32.mod` build a light for the C6 with:

```sh
../../bin/bmk makeapp -r -l esp32 -g riscv32 -board xiao_esp32c6 examples/zigbee_core_light.bmx
```

Add `-x` to flash and run with the board's configured `ESPPORT`. The H2 may
need a physical reset after flashing.

## A minimal coordinator and light

This is a complete On/Off light with a simulated output. Replace the comment
with the GPIO or driver call for your lamp. `ZigbeeStart` returns when startup
has been **accepted**; events report the later network result.

```bmx
SuperStrict
Framework BRL.StandardIO
Import ESP32.Network.Zigbee

If ZigbeeStart(ZigbeeRoleCoordinator, ZigbeeDeviceOnOffLight, 10) Then
    RuntimeError "Could not start Zigbee"
End If
Local event:SZigbeeEvent
While True
    While ZigbeePollEvent(event)
        Select event.kind
            Case ZigbeeEventFormed
                If event.status = 0 Then ZigbeePermitJoining(180)
            Case ZigbeeEventAttribute
                If event.endpoint = 10 And event.cluster = 6 And event.attribute = 0 Then
                    Print "Light is on: " + event.value
                    ' Set the physical output here.
                End If
        End Select
    Wend
    Delay 20
Wend
```

The default primary channel mask selects channel 15. `ZigbeeStart` can take a
different primary mask, a secondary mask, and `autoCommission=False` if you
want to call `ZigbeeFormNetwork` or `ZigbeeJoinNetwork` yourself after a
successful `ZigbeeEventStartup`.

To expose a second light, register its endpoint before starting:

```bmx
If ZigbeeAddEndpoint(ZigbeeDeviceOnOffLight, 11) Then RuntimeError "Endpoint failed"
' Then call ZigbeeStart(..., 10).
```

You can register up to eight extra standard endpoints. Endpoint numbers must
be distinct. Their definitions survive stop/start cycles; call
`ZigbeeClearExtraEndpoints` while stopped to remove them.

## A minimal end-device switch

Build this app with `esp32.zigbee.stack zed`. The switch finds a light, then
sends one Toggle command to its discovered network address and endpoint.
Store both fields from the match event; network addresses can change.

```bmx
SuperStrict
Framework BRL.StandardIO
Import ESP32.Network.Zigbee

If ZigbeeStart(ZigbeeRoleEndDevice, ZigbeeDeviceOnOffSwitch, 1) Then
    RuntimeError "Could not start Zigbee"
End If
Local event:SZigbeeEvent
Local found:Int
While True
    While ZigbeePollEvent(event)
        If event.kind = ZigbeeEventJoined And event.status = 0 Then
            ZigbeeFindOnOffLights()
        End If
        If event.kind = ZigbeeEventOnOffMatch And Not found Then
            found = True
            ZigbeeSendOnOff(1, event.shortAddress, event.endpoint, 2) ' Toggle.
        End If
    Wend
    Delay 20
Wend
```

A saved network may produce a successful `ZigbeeEventStartup` without a new
`ZigbeeEventJoined`; handle that path in a long-running app. If a join fails,
retry `ZigbeeJoinNetwork` after a delay. The full
[switch example](../examples/zigbee_core_switch.bmx) shows both cases.

## The event and result pattern

Calls returning zero usually mean local configuration succeeded or the SDK
accepted an asynchronous request. For a remote operation, inspect its later
event and `event.status`. Drain events often: the native queue holds 64
records, and `ZigbeeDroppedEvents()` counts records lost when it fills.

```bmx
Local event:SZigbeeEvent
While ZigbeePollEvent(event)
    If event.kind = ZigbeeEventRead And event.status = 0 Then
        Print "Attribute " + event.attribute + " = " + event.value
    End If
Wend
If ZigbeeDroppedEvents() Then Print "Zigbee events were dropped"
```

`event.shortAddress` is a 16-bit network address. `event.endpoint` is an
endpoint, and `event.cluster` and `event.attribute` identify a ZCL value.
Scalar values use `event.value` with `dataType` and `valueSize`; strings use
`text`; octet strings, arrays, and structures use `bytes`. A negative
`valueSize` means a variable value was invalid or exceeded the module limit.
Fields unrelated to an event kind are zero. `ZigbeePollEvent` creates managed
values on the application thread, not in the native Zigbee callback.

To stop and restart, wait for `ZigbeeEventStopped` after `ZigbeeStop`.
`ZigbeeFactoryReset` leaves the network and clears saved Zigbee state; wait for
`ZigbeeEventLeft` before recommissioning. The outgoing frame counter is kept.

## Find, bind, read, and report

The general discovery call finds any server cluster on a profile. Each match
arrives as `ZigbeeEventMatch` with its address, endpoint, and cluster:

```bmx
ZigbeeFindServers(ZigbeeClusterTemperatureMeasurement) ' HA profile 0x0104.
' In the event loop:
If event.kind = ZigbeeEventMatch And ..
        event.cluster = ZigbeeClusterTemperatureMeasurement Then
    ZigbeeReadAttribute(1, event.shortAddress, event.endpoint, ..
        ZigbeeClusterTemperatureMeasurement, 0)
End If
```

Pass a second profile ID for a custom profile. `ZigbeeFindOnOffLights` and
`ZigbeeReadOnOff` are shortcuts for common On/Off use.

A binding gives the stack a destination for commands or reports. Bind the
switch client to a light server for bound commands:

```bmx
ZigbeeBindOnOff(1, lightAddress, lightEndpoint)
' After ZigbeeEventOnOffBound reports status zero:
ZigbeeSendBoundOnOff(1, 2) ' Toggle.
```

For reports, bind the **server** endpoint back to the client's endpoint, then
configure reporting from the client. For example, after a temperature sensor
has announced itself to a coordinator, the sensor application can call:

```bmx
ZigbeeBindCluster(12, clientAddress, clientEndpoint, ..
    ZigbeeClusterTemperatureMeasurement)
```

The client can then request reports on the measured-value attribute:

```bmx
ZigbeeConfigureReporting(1, sensorAddress, 12, ..
    ZigbeeClusterTemperatureMeasurement, 0, ZigbeeTypeInt16, 1, 300, 10)
' The final 10 means a change of 0.10 °C in this attribute's raw units.
```

Watch `ZigbeeEventReportingConfigured` and `ZigbeeEventReport`. Intervals are
seconds; a maximum of 65535 disables periodic reports. Reporting currently
supports scalar values. `ZigbeeConfigureOnOffReporting` is a convenience
wrapper for the On/Off attribute.

A remote write has two outcomes: the immediate submission result and a later
`ZigbeeEventWrite` containing the remote status. A successful ZCL write
response may carry attribute ID `0xFFFF`, so correlate by request order too:

```bmx
ZigbeeWriteScalar(1, address, endpoint, ZigbeeClusterThermostat, ..
    ZigbeeThermostatHeatingSetpointAttribute, ZigbeeTypeInt16, 2150)
' 2150 represents 21.50 °C.
```

## Standard cluster recipes

Register optional server or client clusters **before** `ZigbeeStart` on an
existing endpoint. The same `ZigbeeClusterServer` and `ZigbeeClusterClient`
constants select cluster roles. Clear functions remove registrations while
stopped. The examples below show the calls relevant to each feature; lines
marked "after startup" belong in a successful `ZigbeeEventStartup` handler.

### Measurements and battery

A temperature sensor template already has Temperature Measurement. Update it
from a sensor reading in hundredths of a degree Celsius:

```bmx
ZigbeeStart(ZigbeeRoleEndDevice, ZigbeeDeviceTemperatureSensor, 12)
' In the event loop, after startup succeeds:
If event.kind = ZigbeeEventStartup And event.status = 0 Then
    ZigbeeSetLocalTemperature(12, 2345) ' 23.45 °C.
End If
```

Other measurement clusters attach to a configured endpoint:

```bmx
ZigbeeAddHumidityCluster(12, ZigbeeClusterServer)
ZigbeeAddIlluminanceCluster(12, ZigbeeClusterServer)
ZigbeeAddPressureCluster(12, ZigbeeClusterServer)
ZigbeeAddOccupancyCluster(12, ZigbeeClusterServer, ZigbeeOccupancySensorPIR)
' Start Zigbee, then update readings:
ZigbeeSetLocalHumidity(12, 4550)     ' 45.50%.
ZigbeeSetLocalIlluminance(12, 20001) ' ZCL encoded illuminance, not raw lux.
ZigbeeSetLocalPressure(12, 101)      ' Whole kilopascals.
ZigbeeSetLocalOccupancy(12, 1)       ' Occupied; zero means unoccupied.
```

Read each server's attribute zero using its cluster constant. Humidity is
hundredths of a percent (`UInt16`); pressure is whole kPa (`Int16`);
occupancy is a `Bitmap8`. Illuminance uses the Zigbee encoded `UInt16`
MeasuredValue, and 65535 represents unavailable; the setter accepts 0–65534.
Pressure `-32768` also represents unavailable.

Battery and AC electrical measurements use different units:

```bmx
ZigbeeAddPowerConfigurationCluster(12, ZigbeeClusterServer)
ZigbeeAddElectricalMeasurementCluster(12, ZigbeeClusterServer)
' After startup:
ZigbeeSetLocalBatteryVoltage(12, 30)     ' 3.0 V (tenths).
ZigbeeSetLocalBatteryPercentage(12, 150) ' 75% (half-percent units).
ZigbeeSetLocalRmsVoltage(12, 230)        ' Volts.
ZigbeeSetLocalRmsCurrent(12, 250)        ' Milliamps; divisor is 1000.
ZigbeeSetLocalActivePower(12, 60)        ' Watts.
```

Battery voltage 255 and percentage 255 mean unknown. The battery attribute
IDs are `ZigbeeBatteryVoltageAttribute` and
`ZigbeeBatteryPercentageAttribute`; the electrical IDs are
`ZigbeeRmsVoltageAttribute`, `ZigbeeRmsCurrentAttribute`, and
`ZigbeeActivePowerAttribute`.

### Light level, color, and heating

A light endpoint can expose Level and x/y Color Control:

```bmx
ZigbeeAddLevelControlCluster(10, ZigbeeClusterServer)
ZigbeeAddColorControlCluster(10, ZigbeeClusterServer)
' After startup, publish the actual output state:
ZigbeeSetLocalLevel(10, 180)         ' 0–254.
ZigbeeSetLocalColor(10, 20000, 30000) ' CIE x/y scaled to 0–65535.
```

A matching client sends a movement request. `transitionTenths` is in tenths
of a second; `True` selects the Level command variant that also changes On/Off:

```bmx
ZigbeeMoveToLevel(1, lightAddress, 10, 180, 10, True)
ZigbeeMoveToColor(1, lightAddress, 10, 20000, 30000, 10)
```

The server data model does not drive LEDs. Apply the requested state to the
output and publish the observed state. The Thermostat template is heating only:

```bmx
ZigbeeAddThermostatCluster(12, ZigbeeClusterServer)
' After startup:
ZigbeeSetLocalThermostatTemperature(12, 2300) ' 23.00 °C.
ZigbeeSetLocalHeatingSetpoint(12, 2100)     ' 21.00 °C.
```

The occupied heating setpoint is writable with `ZigbeeWriteScalar` and
`ZigbeeTypeInt16`, as shown above. Both thermostat values use hundredths of a
degree Celsius.

### Door locks and window coverings

Select the standard server or controller device template at startup. Incoming
commands on a server appear as `ZigbeeEventActuatorCommand`. The application
must respond within five seconds; otherwise the stack replies with failure:

```bmx
If event.kind = ZigbeeEventActuatorCommand Then
    ' Check event.cluster and event.value, then ask the physical actuator.
    ZigbeeCompleteActuatorCommand(event.signal, ZigbeeCommandSuccess)
    ' Publish observed state only after the actuator accepted the action.
End If
```

`event.cluster` selects Door Lock or Window Covering. For covering commands,
`event.value` is the command ID and `event.attribute` is its target. The
sender's endpoint is in `event.dataType`. A controller can send:

```bmx
ZigbeeSendDoorLock(1, lockAddress, lockEndpoint, 1) ' Lock; zero unlocks.
ZigbeeSendWindowCovering(1, shadeAddress, shadeEndpoint, ..
    ZigbeeCoveringGoToLiftPercentage, 50)
```

These calls confirm submission, not physical motion. Publish the resulting
state with `ZigbeeSetLocalDoorLockState` or `ZigbeeSetLocalCoveringLift`.
Door Lock controller responses arrive as `ZigbeeEventDoorLockResponse`.

### IAS Zone enrollment and alarms

A contact or motion sensor needs an IAS Zone server. The coordinator/CIE needs
an IAS Zone client on its chosen endpoint:

```bmx
ZigbeeAddIasZoneCluster(12, ZigbeeClusterServer, ZigbeeIasZoneContactSwitch)
ZigbeeAddIasZoneCluster(13, ZigbeeClusterClient)
```

The sensor obtains the CIE's IEEE address and requests enrollment. On the CIE,
respond to each request with an assigned zone ID:

```bmx
ZigbeeRequestIasEnrollment(12, cieAddress, 13, cieIeee)
' CIE event loop:
If event.kind = ZigbeeEventIasEnrollRequest Then
    ZigbeeRespondIasEnrollment(event.endpoint, event.shortAddress, ..
        event.dataType, 0, 3)
End If
```

After `ZigbeeEventIasEnrollResponse` reports success, the sensor can notify
Alarm1 and Tamper together. The CIE receives `ZigbeeEventIasStatus`:

```bmx
ZigbeeSetLocalIasZoneStatus(12, ZigbeeIasZoneAlarm1 | ZigbeeIasZoneTamper)
```

## Custom attributes and profiles

Use a custom server cluster ID from `0x8000` to `0xFFFF`. Register the cluster
before its attributes and before `ZigbeeStart`. The limits are eight custom
clusters and 32 custom attributes:

```bmx
ZigbeeAddCustomCluster(10, $fc00, ZigbeeClusterServer)
ZigbeeAddCustomScalar(10, $fc00, 1, ZigbeeTypeUInt16, ..
    ZigbeeAttributeRead | ZigbeeAttributeWrite | ZigbeeAttributeReport, 42)
' After startup, publish a changed local value:
ZigbeeSetCustomScalar(10, $fc00, 1, 43)
```

A client registers the same cluster with `ZigbeeClusterClient`, discovers it
with `ZigbeeFindServers($fc00)`, and can read or write the attribute. Scalar
values include Boolean, integer, bitmap, and enum types up to 32 bits.

Use a short string for up to 64 UTF-8 bytes, or octets for binary data:

```bmx
ZigbeeAddCustomString(10, $fc00, 2, ZigbeeAttributeRead | ZigbeeAttributeWrite, "ready")
Local payload:Byte[] = [Byte($00), Byte($80), Byte($ff)]
ZigbeeAddCustomOctets(10, $fc00, 3, ZigbeeAttributeRead | ZigbeeAttributeWrite, payload)
' From a client after discovery:
ZigbeeWriteString(1, address, 10, $fc00, 2, "hello")
ZigbeeWriteOctets(1, address, 10, $fc00, 3, payload)
```

`ZigbeeAddCustomLongString` and `ZigbeeAddCustomLongOctets`, plus their Set
and Write counterparts, handle up to 256 bytes. Incoming text is in
`event.text`; octets are in `event.bytes`. Remote peers or the transport may
impose a smaller practical limit.

Arrays and structures take **complete ZCL-encoded value bytes**, not a
BlitzMax array of typed elements. An array starts with element type and a
little-endian 16-bit count. A structure starts with a 16-bit count, followed
by type/value pairs:

```bmx
Local values:Byte[] = [Byte(ZigbeeTypeUInt16), Byte(2), Byte(0), ..
    Byte(11), Byte(0), Byte(13), Byte(0)] ' UInt16 array [11, 13].
ZigbeeAddCustomArray(10, $fc00, 4, ..
    ZigbeeAttributeRead | ZigbeeAttributeWrite, values)
' From the client: one attribute per write command.
ZigbeeWriteArray(1, address, 10, $fc00, 4, values)
```

`ZigbeeAddCustomStructure`, `ZigbeeSetCustomStructure`, and
`ZigbeeWriteStructure` use the corresponding structure encoding. Both forms
allow up to 256 encoded bytes and four nesting levels. Reads and incoming
writes deliver the encoding in `event.bytes`. ZCL does not permit reporting
these types. Structured element access and ZCL set/bag types are outside this
API.

For a non-Home-Automation profile, create a custom endpoint, then add its
clusters. Both ends need the same profile ID for discovery and exchange:

```bmx
ZigbeeAddCustomEndpoint(13, $c0de, $1234, 1)
ZigbeeAddCustomCluster(13, $fc01, ZigbeeClusterServer)
' After startup, on a client with a matching profile:
ZigbeeFindServers($fc01, $c0de)
```

## Groups

A light endpoint includes a Groups server; a switch includes a Groups client.
Group IDs are 1–65527. Add a light endpoint to a group, then address the group:

```bmx
ZigbeeAddGroup(1, lightAddress, lightEndpoint, 4660)
' After ZigbeeEventGroupAdded reports success:
ZigbeeSendGroupOnOff(1, 4660, 2) ' Toggle all group members.
```

`ZigbeeViewGroup` checks one ID. `ZigbeeGetGroupMembership` yields zero or
more `ZigbeeEventGroupMember` events followed by
`ZigbeeEventGroupMembershipComplete`; its `value` is the count and
`attribute` is remaining capacity. `ZigbeeRemoveGroup` removes one membership.
`ZigbeeRemoveAllGroups` has no specific response, so query membership to
verify removal. Group commands have no individual device responses.

## Security and key rotation

Install-code commissioning is optional. Enable it **while stopped** on both
coordinator and joiner. For a new network, start with `autoCommission=False`
and provision the joiner's own IEEE address and install code after
`ZigbeeEventStarted`, before forming or joining:

```bmx
ZigbeeEnableInstallCodes()
ZigbeeStart(ZigbeeRoleCoordinator, ZigbeeDeviceOnOffLight, 10, ..
    1 Shl 15, 0, False)
' After ZigbeeEventStarted, with the joiner's IEEE address and code:
ZigbeeAddJoinerInstallCode(joinerIeee, installCode)
ZigbeeRequireInstallCodes()
ZigbeeFormNetwork()
```

On the joiner, call `ZigbeeSetLocalInstallCode(installCode)` after its
`ZigbeeEventStarted`, then `ZigbeeJoinNetwork()`. An install code is 8, 10,
14, or 18 bytes **including** the two-byte CCITT-16 CRC. Obtain a running
device's IEEE address with `ZigbeeGetIEEEAddress`. Keep production codes out
of source control. `ZigbeeRequireLinkKeyExchange(True)` can require a link-key
exchange on both sides before commissioning;
`ZigbeeAllowWellKnownKeyRejoins(False)` tightens rejoin policy on a
coordinator. Saved network members are unaffected by a new joining policy.

To rotate a network key on a running coordinator, generate a fresh 16-byte key
and choose a new sequence number from 0–255:

```bmx
ZigbeeBroadcastNetworkKey(newKey, sequence)
' Allow all devices, especially sleepy ones, time to receive the update.
ZigbeeBroadcastNetworkKeySwitch(sequence)
ZigbeeSwitchNetworkKey(newKey, sequence)
```

The broadcast calls confirm stack submission, not receipt by every device.
Verify traffic from every member after the switch and after restart. See the
[key rotation example](../examples/zigbee_key_rotation_light.bmx) for a
one-shot test with a hardware-generated key.

## Firmware updates over Zigbee

Register an OTA client or server cluster before startup. A client identifies
its currently running firmware by manufacturer, image type, and increasing
version number:

```bmx
ZigbeeAddOtaClient(12, $1234, 1, 1) ' Version 1.
' After finding an OTA server:
ZigbeeOtaQuery(12, serverAddress, serverEndpoint)
```

The server supplies a complete Zigbee OTA file from memory that stays valid
until removal or shutdown, then may notify the client:

```bmx
ZigbeeAddOtaServer(10)
' After startup; imagePtr points to a complete OTA file:
ZigbeeOtaServeImage(10, imagePtr, imageSize)
ZigbeeOtaNotify(10, clientAddress, clientEndpoint, $1234, 1, 2)
```

The client checks a matching, newer file, writes its raw firmware element to
the inactive ESP32 OTA slot, validates it, and reboots after the upgrade
response. Use an OTA partition layout. With rollback enabled, the new app must
call `OTAMarkRunningImageValid` after its startup checks. Monitor
`ZigbeeEventOta`: `signal` gives the progress stage, `value` gives a version
or transferred byte count, and `attribute` gives the total when known.
The supported package is the mandatory 56-byte header without optional fields
and one uncompressed firmware element. The [OTA client and server
examples](../examples/zigbee_ota_server.bmx) include packaging and a two-board
test; use secure boot and signed firmware according to the product's trust
model.

## Sleep and network persistence

`ZigbeeRoleSleepyEndDevice` uses a `zed` build, turns off the receiver while
idle, and allows ESP-IDF light sleep. Yield between application work. The
normal `ZigbeeRoleEndDevice` does not enable light sleep. The sleepy role uses
an eight-second keepalive; actual current depends on the board and attached
peripherals and has not been measured. A router or coordinator should remain
awake to serve the mesh.

For deep sleep, stop Zigbee, wait for `ZigbeeEventStopped`, then call
`ESP32.System.Power.DeepSleepForMilliseconds`. Wake restarts the app, so
re-register endpoints and attributes before `ZigbeeStart`; the saved network
state is restored. See the [deep-sleep sensor
example](../examples/zigbee_deep_sleep_sensor.bmx). On the tested Waveshare
H2, native USB serial did not reliably reconnect after deep sleep; a UART
console through its CH343 bridge remained observable.

## Complete examples and tested behavior

- [Coordinator light](../examples/zigbee_core_light.bmx): endpoints, sensors,
  reports, groups, and custom values.
- [End-device switch](../examples/zigbee_core_switch.bmx): discovery, bindings,
  reads, writes, and reports.
- [Lifecycle](../examples/zigbee_core_lifecycle.bmx): two start/stop cycles.
- [IAS sensor](../examples/zigbee_ias_zone_sensor.bmx): enrollment, status, and
  array/structure exchange.
- [Door Lock, covering, and IAS smoke test](../examples/zigbee_extended_smoke.bmx):
  local loopback for actuator commands and long values.
- [OTA client](../examples/zigbee_ota_client_v1.bmx) and
  [OTA server](../examples/zigbee_ota_server.bmx): two-board firmware update.
- [Sleepy switch](../examples/zigbee_sleepy_switch.bmx) and
  [deep-sleep sensor](../examples/zigbee_deep_sleep_sensor.bmx): power modes.

The [validation record](zigbee-validation.md) distinguishes two-board checks
from C6 loopback checks. Door Lock, Window Covering, and long custom values
have loopback coverage; IAS enrollment and notifications, composite values,
OTA, and key rotation have C6/H2 two-board coverage. Third-party device
interoperability and actual sleep current remain to be measured.
