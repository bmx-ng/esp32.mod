' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP32 Zigbee v2 stack and standard-device support for ESP32-H2 and ESP32-C6.
about: This module provides a configurable stack lifecycle, a safe native event
queue, commissioning controls, network information, and standard On/Off devices.
The diagnostic ZigbeeBringup functions remain for the initial hardware test.
End Rem
Module ESP32.Network.Zigbee
?esp32

ModuleInfo "Version: 0.5"
ModuleInfo "License: zlib/libpng"

Private
Extern "C"
	Function _ZigbeeSdkVersion:Int() = "bmx_esp32_zigbee_sdk_version"
	Function _ZigbeeBringupStart:Int() = "bmx_esp32_zigbee_bringup_start"
	Function _ZigbeeBringupToggle:Int() = "bmx_esp32_zigbee_bringup_toggle"
	Function _ZigbeeBringupRead:Int() = "bmx_esp32_zigbee_bringup_read"
End Extern
Public

Rem
bbdoc: Returns the linked Zigbee SDK major version for bring-up diagnostics.
End Rem
Function ZigbeeSdkMajorVersion:Int()
	Return _ZigbeeSdkVersion()
End Function

Rem
bbdoc: Starts the initial H2 coordinator/light or C6 end-device/switch radio test.
about: The board determines the role. This diagnostic function is temporary while
the configurable public stack API is developed. Returns an ESP-IDF error code.
End Rem
Function ZigbeeBringupStart:Int()
	Return _ZigbeeBringupStart()
End Function

Rem
bbdoc: Sends a bound On/Off Toggle command from the C6 switch test.
about: Returns an ESP-IDF error code; ESP_ERR_INVALID_STATE means that the
switch has not yet joined and bound to the light.
End Rem
Function ZigbeeBringupToggle:Int()
	Return _ZigbeeBringupToggle()
End Function

Rem
bbdoc: Reads the remote light's On/Off attribute from the C6 switch test.
about: The response is reported to the serial log by the Zigbee callback.
Returns an ESP-IDF error code for request submission.
End Rem
Function ZigbeeBringupRead:Int()
	Return _ZigbeeBringupRead()
End Function

' The v2 stack calls native callbacks on its own task. This API only transfers
' primitive records across that boundary; managed objects stay on the caller.

Rem
bbdoc: Build-time Zigbee role for forming a network.
End Rem
Const ZigbeeRoleCoordinator:Int = 1
Rem
bbdoc: Build-time Zigbee role for forwarding traffic and admitting children.
End Rem
Const ZigbeeRoleRouter:Int = 2
Rem
bbdoc: Build-time Zigbee role for a non-routing end device.
End Rem
Const ZigbeeRoleEndDevice:Int = 3
Rem
bbdoc: End-device role prepared for low-power operation.
about: This uses the end-device stack library. Application-managed sleep policy
will be added after wake and polling behaviour has been tested on hardware.
End Rem
Const ZigbeeRoleSleepyEndDevice:Int = 4

Rem
bbdoc: Standard Home Automation On/Off light endpoint template.
End Rem
Const ZigbeeDeviceOnOffLight:Int = 1
Rem
bbdoc: Standard Home Automation On/Off switch endpoint template.
End Rem
Const ZigbeeDeviceOnOffSwitch:Int = 2

Rem
bbdoc: Zigbee stack is stopped.
End Rem
Const ZigbeeStateStopped:Int = 0
Rem
bbdoc: Zigbee stack task is starting.
End Rem
Const ZigbeeStateStarting:Int = 1
Rem
bbdoc: Zigbee stack is running and accepts commands.
End Rem
Const ZigbeeStateRunning:Int = 2
Rem
bbdoc: Zigbee stack is stopping; wait for a stopped event before restarting.
End Rem
Const ZigbeeStateStopping:Int = 3

Rem
bbdoc: The Zigbee stack has started.
End Rem
Const ZigbeeEventStarted:Int = 1
Rem
bbdoc: The Zigbee stack completed its startup or reboot procedure.
End Rem
Const ZigbeeEventStartup:Int = 2
Rem
bbdoc: Network formation completed; inspect the status field.
End Rem
Const ZigbeeEventFormed:Int = 3
Rem
bbdoc: Network steering completed; inspect the status field.
End Rem
Const ZigbeeEventJoined:Int = 4
Rem
bbdoc: Permit-joining duration changed; value is seconds remaining.
End Rem
Const ZigbeeEventPermitJoin:Int = 5
Rem
bbdoc: A device announced itself on the network.
End Rem
Const ZigbeeEventDeviceAnnounced:Int = 6
Rem
bbdoc: An endpoint attribute changed.
End Rem
Const ZigbeeEventAttribute:Int = 7
Rem
bbdoc: A remote attribute read response arrived.
End Rem
Const ZigbeeEventRead:Int = 8
Rem
bbdoc: The device left its network.
End Rem
Const ZigbeeEventLeft:Int = 9
Rem
bbdoc: The Zigbee stack stopped and can be started again.
End Rem
Const ZigbeeEventStopped:Int = 10
Rem
bbdoc: A native Zigbee operation failed; inspect status and signal.
End Rem
Const ZigbeeEventError:Int = 11
Rem
bbdoc: A matching On/Off server was found; shortAddress and endpoint identify it.
End Rem
Const ZigbeeEventOnOffMatch:Int = 12
Rem
bbdoc: A bind request completed; status zero means success.
End Rem
Const ZigbeeEventOnOffBound:Int = 13
Rem
bbdoc: A remote Configure Reporting request completed; status zero means success.
End Rem
Const ZigbeeEventReportingConfigured:Int = 14
Rem
bbdoc: A remote attribute report arrived; On/Off reports carry value zero or one.
End Rem
Const ZigbeeEventReport:Int = 15

Rem
bbdoc: One Zigbee event copied from a native bounded queue.
about: Fields not relevant to an event kind are zero. For an On/Off attribute,
value is the Boolean state. Polling never invokes managed code on the Zigbee task.
End Rem
Struct SZigbeeEvent
	Rem
	bbdoc: One of the ZigbeeEvent constants.
	End Rem
	Field kind:Int
	Rem
	bbdoc: SDK status for the event; zero means success.
	End Rem
	Field status:Int
	Rem
	bbdoc: Raw ESP Zigbee SDK signal identifier, when applicable.
	End Rem
	Field signal:Int
	Rem
	bbdoc: Local destination endpoint for attribute events.
	End Rem
	Field endpoint:Int
	Rem
	bbdoc: Cluster identifier for attribute events.
	End Rem
	Field cluster:Int
	Rem
	bbdoc: Attribute identifier for attribute events.
	End Rem
	Field attribute:Int
	Rem
	bbdoc: On/Off value or permit-joining duration, according to kind.
	End Rem
	Field value:Int
	Rem
	bbdoc: Network address of an announced device or this device after commissioning.
	End Rem
	Field shortAddress:Int
	Rem
	bbdoc: Network PAN identifier after commissioning.
	End Rem
	Field panId:Int
	Rem
	bbdoc: IEEE 802.15.4 channel after commissioning.
	End Rem
	Field channel:Int
	Rem
	bbdoc: True when a startup event found no saved Zigbee network state.
	End Rem
	Field factoryNew:Int
End Struct

Rem
bbdoc: Current Zigbee network identity and persistence state.
End Rem
Struct SZigbeeNetworkInfo
	Rem
	bbdoc: True when the device belongs to a Zigbee network.
	End Rem
	Field joined:Int
	Rem
	bbdoc: True when Zigbee persistent state is factory new.
	End Rem
	Field factoryNew:Int
	Rem
	bbdoc: Current 16-bit PAN identifier.
	End Rem
	Field panId:Int
	Rem
	bbdoc: Current 16-bit network address.
	End Rem
	Field shortAddress:Int
	Rem
	bbdoc: Current IEEE 802.15.4 channel.
	End Rem
	Field channel:Int
	Rem
	bbdoc: Current 64-bit extended PAN identifier.
	End Rem
	Field extendedPanId:Long
End Struct

Private
Extern "C"
	Function _ZigbeeCoreStart:Int(role:Int, device:Int, endpoint:Int, primaryMask:UInt, ..
		secondaryMask:UInt, autoCommission:Int) = "bmx_esp32_zigbee_core_start"
	Function _ZigbeeCoreState:Int() = "bmx_esp32_zigbee_core_state"
	Function _ZigbeeCoreStop:Int() = "bmx_esp32_zigbee_core_stop"
	Function _ZigbeeCoreForm:Int() = "bmx_esp32_zigbee_core_form"
	Function _ZigbeeCoreJoin:Int() = "bmx_esp32_zigbee_core_join"
	Function _ZigbeeCorePermitJoin:Int(seconds:Int) = "bmx_esp32_zigbee_core_permit_join"
	Function _ZigbeeCoreReset:Int() = "bmx_esp32_zigbee_core_reset"
	Function _ZigbeeCoreNetwork:Int(joined:Int Var, factoryNew:Int Var, panId:Int Var, ..
		shortAddress:Int Var, channel:Int Var, extendedPanId:Long Var) = "bmx_esp32_zigbee_core_network"
	Function _ZigbeeCoreOnOff:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, command:Int) = "bmx_esp32_zigbee_core_onoff"
	Function _ZigbeeCoreReadOnOff:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int) = "bmx_esp32_zigbee_core_read_onoff"
	Function _ZigbeeCorePoll:Int(kind:Int Var, status:Int Var, signal:Int Var, endpoint:Int Var, ..
		cluster:Int Var, attribute:Int Var, value:Int Var, shortAddress:Int Var, ..
		panId:Int Var, channel:Int Var, factoryNew:Int Var) = "bmx_esp32_zigbee_core_poll"
	Function _ZigbeeCoreDropped:UInt() = "bmx_esp32_zigbee_core_dropped"
	Function _ZigbeeCoreFindOnOff:Int() = "bmx_esp32_zigbee_core_find_onoff"
	Function _ZigbeeCoreBindOnOff:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int) = "bmx_esp32_zigbee_core_bind_onoff"
	Function _ZigbeeCoreBoundOnOff:Int(sourceEndpoint:Int, command:Int) = "bmx_esp32_zigbee_core_bound_onoff"
	Function _ZigbeeCoreSetLocalOnOff:Int(endpoint:Int, on:Int) = "bmx_esp32_zigbee_core_set_local_onoff"
	Function _ZigbeeCoreConfigureOnOffReporting:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, minInterval:Int, maxInterval:Int) = ..
		"bmx_esp32_zigbee_core_configure_onoff_reporting"
End Extern
Public

Rem
bbdoc: Starts the Zigbee stack with a standard light or switch endpoint.
param: Coordinator, router, end-device or sleepy-end-device role.
param: Standard on/off light or switch device template.
param: Zigbee endpoint number from 1 to 240.
param: Primary channel bitmask; bit n selects IEEE 802.15.4 channel n (11 to 26).
param: Secondary channel bitmask used after primary-channel scanning.
param: True to form or seek a network automatically when factory new.
returns: ESP_OK when the asynchronous start was accepted, otherwise an ESP-IDF error code.
about: Coordinator and router need a `zczr` stack build; end devices need `zed`.
Set BMX_ESP32_ZIGBEE_STACK_MODE before bmk to override the board default. The
stack's callbacks are delivered through ZigbeePollEvent.
End Rem
Function ZigbeeStart:Int(role:Int, device:Int, endpoint:Int, primaryMask:UInt = 1 Shl 15, ..
		secondaryMask:UInt = 0, autoCommission:Int = True)
	Return _ZigbeeCoreStart(role, device, endpoint, primaryMask, secondaryMask, autoCommission)
End Function

Rem
bbdoc: Returns the current local Zigbee stack state.
End Rem
Function ZigbeeState:Int()
	Return _ZigbeeCoreState()
End Function

Rem
bbdoc: Requests a clean asynchronous Zigbee stack shutdown.
about: Poll for ZigbeeEventStopped before starting again.
End Rem
Function ZigbeeStop:Int()
	Return _ZigbeeCoreStop()
End Function

Rem
bbdoc: Requests network formation for a coordinator.
about: Call after a successful ZigbeeEventStartup when autoCommission is False.
End Rem
Function ZigbeeFormNetwork:Int()
	Return _ZigbeeCoreForm()
End Function

Rem
bbdoc: Starts network steering to join an available Zigbee network.
about: Call after a successful ZigbeeEventStartup when autoCommission is False.
End Rem
Function ZigbeeJoinNetwork:Int()
	Return _ZigbeeCoreJoin()
End Function

Rem
bbdoc: Allows new devices to join for the given number of seconds.
param: Duration from 1 to 254 seconds, or zero to close joining.
End Rem
Function ZigbeePermitJoining:Int(seconds:Int)
	Return _ZigbeeCorePermitJoin(seconds)
End Function

Rem
bbdoc: Leaves the network and clears Zigbee persistent state.
about: This only takes effect while joined to a network. The SDK retains its
outgoing network frame counter. This operation is asynchronous; poll for
ZigbeeEventLeft before starting a new commissioning flow.
End Rem
Function ZigbeeFactoryReset:Int()
	Return _ZigbeeCoreReset()
End Function

Rem
bbdoc: Gets the current network identity from the running stack.
returns: ESP_OK on success, or an ESP-IDF error code.
End Rem
Function ZigbeeGetNetworkInfo:Int(info:SZigbeeNetworkInfo Var)
	Return _ZigbeeCoreNetwork(info.joined, info.factoryNew, info.panId, ..
		info.shortAddress, info.channel, info.extendedPanId)
End Function

Rem
bbdoc: Sends an On/Off cluster command to a 16-bit network address.
param: Local source endpoint containing the On/Off client cluster.
param: Remote 16-bit network address.
param: Remote On/Off server endpoint.
param: Zero turns off, one turns on, and two toggles.
returns: ESP_OK when the SDK accepts the command for transmission.
End Rem
Function ZigbeeSendOnOff:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, command:Int)
	Return _ZigbeeCoreOnOff(sourceEndpoint, destinationAddress, destinationEndpoint, command)
End Function

Rem
bbdoc: Requests the remote On/Off attribute value.
about: Poll for a ZigbeeEventRead carrying cluster 6, attribute 0. The return
value only reports whether the request was accepted for transmission.
End Rem
Function ZigbeeReadOnOff:Int(sourceEndpoint:Int, destinationAddress:Int, destinationEndpoint:Int)
	Return _ZigbeeCoreReadOnOff(sourceEndpoint, destinationAddress, destinationEndpoint)
End Function

Rem
bbdoc: Searches the network for Home Automation On/Off server endpoints.
about: Each result arrives as ZigbeeEventOnOffMatch with shortAddress and
endpoint. This is an asynchronous broadcast request.
End Rem
Function ZigbeeFindOnOffLights:Int()
	Return _ZigbeeCoreFindOnOff()
End Function

Rem
bbdoc: Binds a local On/Off endpoint to a remote On/Off endpoint.
about: The destination short address must resolve to an IEEE address in the
stack's address table. Bind switch to light for commands; bind light to switch
for attribute reports. Poll for ZigbeeEventOnOffBound to confirm the result.
End Rem
Function ZigbeeBindOnOff:Int(sourceEndpoint:Int, destinationAddress:Int, destinationEndpoint:Int)
	Return _ZigbeeCoreBindOnOff(sourceEndpoint, destinationAddress, destinationEndpoint)
End Function

Rem
bbdoc: Sends Off, On or Toggle through the switch endpoint's binding table.
about: Command zero turns off, one turns on, and two toggles. Bind the switch
first; a zero return only confirms submission to the SDK.
End Rem
Function ZigbeeSendBoundOnOff:Int(sourceEndpoint:Int, command:Int)
	Return _ZigbeeCoreBoundOnOff(sourceEndpoint, command)
End Function

Rem
bbdoc: Sets a local On/Off light endpoint's server attribute.
about: Pass zero for Off or one for On. This updates the Zigbee data model;
the application is responsible for applying the state to its physical output.
End Rem
Function ZigbeeSetLocalOnOff:Int(endpoint:Int, on:Int)
	Return _ZigbeeCoreSetLocalOnOff(endpoint, on)
End Function

Rem
bbdoc: Requests periodic and change-based On/Off reports from a remote light.
about: Call on a switch endpoint after discovering a light. Intervals are in
seconds; minimum may be zero, and maximum may be 65535 to disable periodic
reports. Poll for ZigbeeEventReportingConfigured and ZigbeeEventReport.
End Rem
Function ZigbeeConfigureOnOffReporting:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, minInterval:Int, maxInterval:Int)
	Return _ZigbeeCoreConfigureOnOffReporting(sourceEndpoint, destinationAddress, ..
		destinationEndpoint, minInterval, maxInterval)
End Function

Rem
bbdoc: Removes the oldest event from the native Zigbee queue.
returns: True if an event was returned; False if the queue is empty.
about: Native callbacks copy primitive values into a bounded queue. The
application thread is the only place this function creates a BlitzMax value.
End Rem
Function ZigbeePollEvent:Int(event:SZigbeeEvent Var)
	Return _ZigbeeCorePoll(event.kind, event.status, event.signal, event.endpoint, ..
		event.cluster, event.attribute, event.value, event.shortAddress, event.panId, ..
		event.channel, event.factoryNew)
End Function

Rem
bbdoc: Returns the number of Zigbee events discarded because the queue filled.
about: This count resets on each ZigbeeStart and saturates at the UInt maximum.
End Rem
Function ZigbeeDroppedEvents:UInt()
	Return _ZigbeeCoreDropped()
End Function
?
