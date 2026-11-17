# Zigbee stack API

`ESP32.Network.Zigbee` uses the pinned ESP Zigbee SDK 2.1.0 component with
ESP-IDF 6.1. It currently provides a configurable stack lifecycle, network
commissioning, a bounded event queue, and standard Home Automation On/Off
light and switch endpoints. See `zigbee-bringup.md` for the earlier diagnostic
probe and the optional component integration.

## Build configuration

The Zigbee import selects its own `sdkconfig.zigbee.<mode>` and partition table.
Without the import, a normal application keeps its board configuration and may
use any valid partition layout for that board. The Zigbee default partition
layout includes `zb_storage` and `zb_fct`, alongside the normal NVS, OTA and
LittleFS partitions. `BMX_ESP32_ZIGBEE_PARTITIONS` selects a custom table; its
absolute path must name a table containing `zb_storage` as `data,nvs`.

The build selects `zczr` (coordinator/router) on H2 and `zed` (end device) on
C6 by default. Either board can select the other mode with
`BMX_ESP32_ZIGBEE_STACK_MODE=zczr` or `=zed` before running `bmk`. Each mode
has its own sdkconfig variant, so switching roles rebuilds the correct SDK
library. A coordinator or router role requires `zczr`; an end-device role
requires `zed`. `ZigbeeStart` rejects a role that does not match the linked
library.

## Application flow

Call `ZigbeeStart(role, device, endpoint, primaryMask, secondaryMask,
autoCommission)`. The default channel mask selects channel 15. Startup is
asynchronous; process `ZigbeePollEvent` from the application thread. With
auto commissioning enabled and no saved Zigbee network, a coordinator forms a
network, while a router or end device attempts to join one. A coordinator
starts network steering after formation. With auto commissioning disabled,
wait for a successful `ZigbeeEventStartup` and call `ZigbeeFormNetwork` or
`ZigbeeJoinNetwork` explicitly. `ZigbeePermitJoining` opens or closes joining.
`ZigbeeGetNetworkInfo` returns current network details.

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

`ZigbeeConfigureOnOffReporting` requests change and periodic reports from a
light. The response arrives as `ZigbeeEventReportingConfigured`; incoming
reports arrive as `ZigbeeEventReport`. The light needs a binding from its On/Off
server endpoint back to the switch's client endpoint so reports have a
destination. The light example creates this binding when it sees the switch
announce itself.

## Examples

- `examples/zigbee_core_light.bmx`: coordinator/light on H2.
- `examples/zigbee_core_switch.bmx`: end-device/switch on C6.
- `examples/zigbee_core_lifecycle.bmx`: two bounded start/stop cycles.

Use each board's `ESPPORT` and the normal `bmk makeapp -a -r -x -l esp32 -g
riscv32 -board <board> <example>` flow. The H2 may need a manual reset after
flashing. If changing a board between coordinator and end-device roles, clear
its saved Zigbee network state before attempting to form or join a new
network. The saved network belongs to the previous role and topology.

## Scope

The public API currently covers one standard On/Off endpoint. Generic endpoint
and cluster registration, arbitrary attribute types, generic reporting,
group commands, security configuration, OTA, and application-managed sleep
are further work. `ZigbeeRoleSleepyEndDevice` currently configures an end
device with a longer keepalive; it does not enter low-power sleep itself.

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
