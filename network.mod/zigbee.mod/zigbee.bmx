' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP32 Zigbee v2 stack and standard-device support for ESP32-H2 and ESP32-C6.
about: This module provides a configurable stack lifecycle, a safe native event
queue, commissioning controls, network information, and standard On/Off devices.
End Rem
Module ESP32.Network.Zigbee
?esp32

ModuleInfo "Version: 1.10"
ModuleInfo "License: zlib/libpng"

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
bbdoc: End-device role that allows automatic light sleep between Zigbee activity.
about: Requires a zed stack build. The radio turns off while idle and ESP-IDF
enters light sleep when all tasks are idle. Application tasks must yield for
sleep to occur; wake sources may be configured through ESP32.System.Power.
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
bbdoc: Standard Home Automation temperature sensor endpoint template.
End Rem
Const ZigbeeDeviceTemperatureSensor:Int = 3

Rem
bbdoc: Temperature Measurement cluster identifier.
End Rem
Const ZigbeeClusterTemperatureMeasurement:Int = $0402
Rem
bbdoc: Relative Humidity Measurement cluster identifier.
End Rem
Const ZigbeeClusterRelativeHumidityMeasurement:Int = $0405
Rem
bbdoc: Illuminance Measurement cluster identifier.
End Rem
Const ZigbeeClusterIlluminanceMeasurement:Int = $0400
Rem
bbdoc: Pressure Measurement cluster identifier.
End Rem
Const ZigbeeClusterPressureMeasurement:Int = $0403
Rem
bbdoc: Occupancy Sensing cluster identifier.
End Rem
Const ZigbeeClusterOccupancySensing:Int = $0406
Rem
bbdoc: Power Configuration cluster identifier, including battery status.
End Rem
Const ZigbeeClusterPowerConfiguration:Int = $0001
Const ZigbeeBatteryVoltageAttribute:Int = $0020
Const ZigbeeBatteryPercentageAttribute:Int = $0021
Rem
bbdoc: Electrical Measurement cluster identifier.
End Rem
Const ZigbeeClusterElectricalMeasurement:Int = $0b04
Const ZigbeeRmsVoltageAttribute:Int = $0505
Const ZigbeeRmsCurrentAttribute:Int = $0508
Const ZigbeeActivePowerAttribute:Int = $050b
Const ZigbeeAcCurrentDivisorAttribute:Int = $0603
Rem
bbdoc: Level Control cluster identifier.
End Rem
Const ZigbeeClusterLevelControl:Int = $0008
Const ZigbeeCurrentLevelAttribute:Int = $0000
Rem
bbdoc: Color Control cluster and current CIE x/y attribute identifiers.
End Rem
Const ZigbeeClusterColorControl:Int = $0300
Const ZigbeeCurrentColorXAttribute:Int = $0003
Const ZigbeeCurrentColorYAttribute:Int = $0004
Rem
bbdoc: OTA Upgrade cluster identifier.
End Rem
Const ZigbeeClusterOtaUpgrade:Int = $0019

Rem
bbdoc: PIR occupancy sensor type.
End Rem
Const ZigbeeOccupancySensorPIR:Int = 0
Rem
bbdoc: Ultrasonic occupancy sensor type.
End Rem
Const ZigbeeOccupancySensorUltrasonic:Int = 1
Rem
bbdoc: Combined PIR and ultrasonic occupancy sensor type.
End Rem
Const ZigbeeOccupancySensorPIRUltrasonic:Int = 2
Rem
bbdoc: Physical-contact occupancy sensor type.
End Rem
Const ZigbeeOccupancySensorContact:Int = 3

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
bbdoc: A matching server was found; cluster identifies the matched cluster.
End Rem
Const ZigbeeEventMatch:Int = 12
Rem
bbdoc: A bind request completed; status zero means success.
End Rem
Const ZigbeeEventOnOffBound:Int = 13
Rem
bbdoc: A cluster bind completed; endpoint and cluster identify its source.
End Rem
Const ZigbeeEventBound:Int = 13
Rem
bbdoc: A remote Configure Reporting request completed; status zero means success.
End Rem
Const ZigbeeEventReportingConfigured:Int = 14
Rem
bbdoc: A remote attribute report arrived; On/Off reports carry value zero or one.
End Rem
Const ZigbeeEventReport:Int = 15
Rem
bbdoc: A remote endpoint replied to Add Group; value is the group identifier.
End Rem
Const ZigbeeEventGroupAdded:Int = 16
Rem
bbdoc: A remote endpoint replied to Remove Group; value is the group identifier.
End Rem
Const ZigbeeEventGroupRemoved:Int = 17
Rem
bbdoc: A remote Write Attribute request completed.
End Rem
Const ZigbeeEventWrite:Int = 18
Rem
bbdoc: A remote Groups server replied to View Group; value is the group ID.
End Rem
Const ZigbeeEventGroupViewed:Int = 19
Rem
bbdoc: One group ID from a Get Group Membership response, in value.
End Rem
Const ZigbeeEventGroupMember:Int = 20
Rem
bbdoc: End of a Get Group Membership response; value is count and attribute is remaining capacity.
End Rem
Const ZigbeeEventGroupMembershipComplete:Int = 21
Rem
bbdoc: OTA Upgrade progress or a query response; see signal and dataType.
End Rem
Const ZigbeeEventOta:Int = 22

Const ZigbeeOtaQueryResponse:Int = -1
Const ZigbeeOtaStart:Int = 0
Const ZigbeeOtaReceiving:Int = 1
Const ZigbeeOtaSending:Int = 2
Const ZigbeeOtaCheck:Int = 3
Const ZigbeeOtaApply:Int = 4
Const ZigbeeOtaFinish:Int = 5
Const ZigbeeOtaAbort:Int = 6

Rem
bbdoc: ZCL Boolean scalar data type.
End Rem
Const ZigbeeTypeBool:Int = $10
Rem
bbdoc: ZCL 8-bit bitmap data type.
End Rem
Const ZigbeeTypeBitmap8:Int = $18
Rem
bbdoc: ZCL 16-bit bitmap data type.
End Rem
Const ZigbeeTypeBitmap16:Int = $19
Rem
bbdoc: ZCL unsigned 8-bit scalar data type.
End Rem
Const ZigbeeTypeUInt8:Int = $20
Rem
bbdoc: ZCL unsigned 16-bit scalar data type.
End Rem
Const ZigbeeTypeUInt16:Int = $21
Rem
bbdoc: ZCL unsigned 32-bit scalar data type.
End Rem
Const ZigbeeTypeUInt32:Int = $23
Rem
bbdoc: ZCL signed 8-bit scalar data type.
End Rem
Const ZigbeeTypeInt8:Int = $28
Rem
bbdoc: ZCL signed 16-bit scalar data type.
End Rem
Const ZigbeeTypeInt16:Int = $29
Rem
bbdoc: ZCL signed 32-bit scalar data type.
End Rem
Const ZigbeeTypeInt32:Int = $2b
Rem
bbdoc: ZCL 8-bit enumeration data type.
End Rem
Const ZigbeeTypeEnum8:Int = $30
Rem
bbdoc: ZCL 16-bit enumeration data type.
End Rem
Const ZigbeeTypeEnum16:Int = $31

Rem
bbdoc: ZCL length-prefixed binary octet string data type.
End Rem
Const ZigbeeTypeOctets:Int = $41

Rem
bbdoc: ZCL length-prefixed UTF-8 character string data type.
End Rem
Const ZigbeeTypeString:Int = $42

Rem
bbdoc: Maximum UTF-8 byte length handled by Zigbee string attributes.
End Rem
Const ZigbeeStringMaxBytes:Int = 64
Rem
bbdoc: Maximum binary byte length handled by Zigbee octet-string attributes.
End Rem
Const ZigbeeOctetsMaxBytes:Int = 64

Rem
bbdoc: Server role for a custom cluster.
End Rem
Const ZigbeeClusterServer:Int = 1
Rem
bbdoc: Client role for a custom cluster.
End Rem
Const ZigbeeClusterClient:Int = 2
Rem
bbdoc: Custom attribute can be read remotely.
End Rem
Const ZigbeeAttributeRead:Int = 1
Rem
bbdoc: Custom attribute can be written remotely.
End Rem
Const ZigbeeAttributeWrite:Int = 2
Rem
bbdoc: Custom attribute can be reported.
End Rem
Const ZigbeeAttributeReport:Int = 4

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
	bbdoc: Attribute identifier, or remaining group capacity on ZigbeeEventGroupMembershipComplete.
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
	Rem
	bbdoc: ZCL data type for a scalar attribute event.
	End Rem
	Field dataType:Int
	Rem
	bbdoc: Number of value bytes copied; negative means a string exceeded 64 bytes.
	End Rem
	Field valueSize:Int
	Rem
	bbdoc: Text for a ZigbeeTypeString attribute event.
	End Rem
	Field text:String
	Rem
	bbdoc: Binary data for a ZigbeeTypeOctets attribute event; Null for other types.
	End Rem
	Field bytes:Byte[]
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
	Function _ZigbeeCoreAddEndpoint:Int(device:Int, endpoint:Int) = "bmx_esp32_zigbee_core_add_endpoint"
	Function _ZigbeeCoreClearEndpoints:Int() = "bmx_esp32_zigbee_core_clear_endpoints"
	Function _ZigbeeCoreAddCustomEndpoint:Int(endpoint:Int, profileId:Int, deviceId:Int, ..
		version:Int) = "bmx_esp32_zigbee_core_add_custom_endpoint"
	Function _ZigbeeCoreClearCustomEndpoints:Int() = ..
		"bmx_esp32_zigbee_core_clear_custom_endpoints"
	Function _ZigbeeCoreAddHumidityCluster:Int(endpoint:Int, role:Int) = ..
		"bmx_esp32_zigbee_core_add_humidity_cluster"
	Function _ZigbeeCoreClearHumidityClusters:Int() = ..
		"bmx_esp32_zigbee_core_clear_humidity_clusters"
	Function _ZigbeeCoreAddIlluminanceCluster:Int(endpoint:Int, role:Int) = ..
		"bmx_esp32_zigbee_core_add_illuminance_cluster"
	Function _ZigbeeCoreClearIlluminanceClusters:Int() = ..
		"bmx_esp32_zigbee_core_clear_illuminance_clusters"
	Function _ZigbeeCoreAddPressureCluster:Int(endpoint:Int, role:Int) = ..
		"bmx_esp32_zigbee_core_add_pressure_cluster"
	Function _ZigbeeCoreClearPressureClusters:Int() = ..
		"bmx_esp32_zigbee_core_clear_pressure_clusters"
	Function _ZigbeeCoreAddOccupancyCluster:Int(endpoint:Int, role:Int, sensorType:Int) = ..
		"bmx_esp32_zigbee_core_add_occupancy_cluster"
	Function _ZigbeeCoreClearOccupancyClusters:Int() = ..
		"bmx_esp32_zigbee_core_clear_occupancy_clusters"
	Function _ZigbeeCoreAddPowerConfigCluster:Int(endpoint:Int, role:Int) = ..
		"bmx_esp32_zigbee_core_add_power_config_cluster"
	Function _ZigbeeCoreClearPowerConfigClusters:Int() = ..
		"bmx_esp32_zigbee_core_clear_power_config_clusters"
	Function _ZigbeeCoreAddElectricalMeasurementCluster:Int(endpoint:Int, role:Int) = ..
		"bmx_esp32_zigbee_core_add_electrical_measurement_cluster"
	Function _ZigbeeCoreClearElectricalMeasurementClusters:Int() = ..
		"bmx_esp32_zigbee_core_clear_electrical_measurement_clusters"
	Function _ZigbeeCoreAddLevelCluster:Int(endpoint:Int, role:Int) = ..
		"bmx_esp32_zigbee_core_add_level_cluster"
	Function _ZigbeeCoreClearLevelClusters:Int() = ..
		"bmx_esp32_zigbee_core_clear_level_clusters"
	Function _ZigbeeCoreAddColorControlCluster:Int(endpoint:Int, role:Int) = ..
		"bmx_esp32_zigbee_core_add_color_control_cluster"
	Function _ZigbeeCoreClearColorControlClusters:Int() = ..
		"bmx_esp32_zigbee_core_clear_color_control_clusters"
	Function _ZigbeeCoreAddOtaClient:Int(endpoint:Int, manufacturer:Int, imageType:Int, ..
		version:UInt) = "bmx_esp32_zigbee_core_add_ota_client"
	Function _ZigbeeCoreAddOtaServer:Int(endpoint:Int) = "bmx_esp32_zigbee_core_add_ota_server"
	Function _ZigbeeCoreClearOtaClusters:Int() = "bmx_esp32_zigbee_core_clear_ota_clusters"
	Function _ZigbeeCoreOtaServeImage:Int(endpoint:Int, image:Byte Ptr, size:UInt) = ..
		"bmx_esp32_zigbee_core_ota_serve_image"
	Function _ZigbeeCoreOtaRemoveImage:Int(endpoint:Int) = ..
		"bmx_esp32_zigbee_core_ota_remove_image"
	Function _ZigbeeCoreOtaNotify:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, manufacturer:Int, imageType:Int, version:UInt) = ..
		"bmx_esp32_zigbee_core_ota_notify"
	Function _ZigbeeCoreOtaQuery:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int) = "bmx_esp32_zigbee_core_ota_query"
	Function _ZigbeeCoreAddCustomCluster:Int(endpoint:Int, clusterId:Int, role:Int) = ..
		"bmx_esp32_zigbee_core_add_custom_cluster"
	Function _ZigbeeCoreAddCustomScalar:Int(endpoint:Int, clusterId:Int, attributeId:Int, ..
		dataType:Int, access:Int, value:Int) = "bmx_esp32_zigbee_core_add_custom_scalar"
	Function _ZigbeeCoreAddCustomString:Int(endpoint:Int, clusterId:Int, attributeId:Int, ..
		access:Int, text:Byte Ptr, length:Int) = "bmx_esp32_zigbee_core_add_custom_string"
	Function _ZigbeeCoreAddCustomOctets:Int(endpoint:Int, clusterId:Int, attributeId:Int, ..
		access:Int, bytes:Byte Ptr, length:Int) = "bmx_esp32_zigbee_core_add_custom_octets"
	Function _ZigbeeCoreClearCustomClusters:Int() = "bmx_esp32_zigbee_core_clear_custom_clusters"
	Function _ZigbeeCoreStart:Int(role:Int, device:Int, endpoint:Int, primaryMask:UInt, ..
		secondaryMask:UInt, autoCommission:Int) = "bmx_esp32_zigbee_core_start"
	Function _ZigbeeCoreState:Int() = "bmx_esp32_zigbee_core_state"
	Function _ZigbeeCoreEnableInstallCodes:Int(enabled:Int) = ..
		"bmx_esp32_zigbee_core_enable_install_codes"
	Function _ZigbeeCoreIEEEAddress:Int(address:Long Var) = ..
		"bmx_esp32_zigbee_core_ieee_address"
	Function _ZigbeeCoreRequireInstallCodes:Int(required:Int) = ..
		"bmx_esp32_zigbee_core_require_install_codes"
	Function _ZigbeeCoreRequireLinkKeyExchange:Int(required:Int) = ..
		"bmx_esp32_zigbee_core_require_link_key_exchange"
	Function _ZigbeeCoreAllowWellKnownKeyRejoins:Int(allow:Int) = ..
		"bmx_esp32_zigbee_core_allow_well_known_key_rejoins"
	Function _ZigbeeCoreAddJoinerInstallCode:Int(address:Long, code:Byte Ptr, length:Int) = ..
		"bmx_esp32_zigbee_core_add_joiner_install_code"
	Function _ZigbeeCoreRemoveJoinerInstallCode:Int(address:Long) = ..
		"bmx_esp32_zigbee_core_remove_joiner_install_code"
	Function _ZigbeeCoreSetLocalInstallCode:Int(code:Byte Ptr, length:Int) = ..
		"bmx_esp32_zigbee_core_set_local_install_code"
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
		panId:Int Var, channel:Int Var, factoryNew:Int Var, dataType:Int Var, ..
		valueSize:Int Var, text:Byte Ptr, textCapacity:Int) = "bmx_esp32_zigbee_core_poll"
	Function _ZigbeeCoreDropped:UInt() = "bmx_esp32_zigbee_core_dropped"
	Function _ZigbeeCoreFindOnOff:Int() = "bmx_esp32_zigbee_core_find_onoff"
	Function _ZigbeeCoreFindServers:Int(profileId:Int, clusterId:Int) = ..
		"bmx_esp32_zigbee_core_find_servers"
	Function _ZigbeeCoreBindOnOff:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int) = "bmx_esp32_zigbee_core_bind_onoff"
	Function _ZigbeeCoreBindCluster:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int) = "bmx_esp32_zigbee_core_bind_cluster"
	Function _ZigbeeCoreBoundOnOff:Int(sourceEndpoint:Int, command:Int) = "bmx_esp32_zigbee_core_bound_onoff"
	Function _ZigbeeCoreSetLocalOnOff:Int(endpoint:Int, on:Int) = "bmx_esp32_zigbee_core_set_local_onoff"
	Function _ZigbeeCoreSetLocalTemperature:Int(endpoint:Int, centiCelsius:Int) = ..
		"bmx_esp32_zigbee_core_set_local_temperature"
	Function _ZigbeeCoreSetLocalHumidity:Int(endpoint:Int, centiPercent:Int) = ..
		"bmx_esp32_zigbee_core_set_local_humidity"
	Function _ZigbeeCoreSetLocalIlluminance:Int(endpoint:Int, encodedValue:Int) = ..
		"bmx_esp32_zigbee_core_set_local_illuminance"
	Function _ZigbeeCoreSetLocalPressure:Int(endpoint:Int, kilopascals:Int) = ..
		"bmx_esp32_zigbee_core_set_local_pressure"
	Function _ZigbeeCoreSetLocalOccupancy:Int(endpoint:Int, occupied:Int) = ..
		"bmx_esp32_zigbee_core_set_local_occupancy"
	Function _ZigbeeCoreSetLocalBatteryVoltage:Int(endpoint:Int, decivolts:Int) = ..
		"bmx_esp32_zigbee_core_set_local_battery_voltage"
	Function _ZigbeeCoreSetLocalBatteryPercentage:Int(endpoint:Int, halfPercent:Int) = ..
		"bmx_esp32_zigbee_core_set_local_battery_percentage"
	Function _ZigbeeCoreSetLocalRmsVoltage:Int(endpoint:Int, volts:Int) = ..
		"bmx_esp32_zigbee_core_set_local_rms_voltage"
	Function _ZigbeeCoreSetLocalRmsCurrent:Int(endpoint:Int, milliamps:Int) = ..
		"bmx_esp32_zigbee_core_set_local_rms_current"
	Function _ZigbeeCoreSetLocalActivePower:Int(endpoint:Int, watts:Int) = ..
		"bmx_esp32_zigbee_core_set_local_active_power"
	Function _ZigbeeCoreSetLocalLevel:Int(endpoint:Int, level:Int) = ..
		"bmx_esp32_zigbee_core_set_local_level"
	Function _ZigbeeCoreMoveToLevel:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, level:Int, transitionTenths:Int, withOnOff:Int) = ..
		"bmx_esp32_zigbee_core_move_to_level"
	Function _ZigbeeCoreSetLocalColor:Int(endpoint:Int, x:Int, y:Int) = ..
		"bmx_esp32_zigbee_core_set_local_color"
	Function _ZigbeeCoreMoveToColor:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, x:Int, y:Int, transitionTenths:Int) = ..
		"bmx_esp32_zigbee_core_move_to_color"
	Function _ZigbeeCoreSetCustomScalar:Int(endpoint:Int, clusterId:Int, attributeId:Int, ..
		value:Int) = "bmx_esp32_zigbee_core_set_custom_scalar"
	Function _ZigbeeCoreSetCustomString:Int(endpoint:Int, clusterId:Int, attributeId:Int, ..
		text:Byte Ptr, length:Int) = "bmx_esp32_zigbee_core_set_custom_string"
	Function _ZigbeeCoreSetCustomOctets:Int(endpoint:Int, clusterId:Int, attributeId:Int, ..
		bytes:Byte Ptr, length:Int) = "bmx_esp32_zigbee_core_set_custom_octets"
	Function _ZigbeeCoreWriteString:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int, attributeId:Int, text:Byte Ptr, ..
		length:Int) = "bmx_esp32_zigbee_core_write_string"
	Function _ZigbeeCoreWriteOctets:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int, attributeId:Int, bytes:Byte Ptr, ..
		length:Int) = "bmx_esp32_zigbee_core_write_octets"
	Function _ZigbeeCoreConfigureOnOffReporting:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, minInterval:Int, maxInterval:Int) = ..
		"bmx_esp32_zigbee_core_configure_onoff_reporting"
	Function _ZigbeeCoreConfigureReporting:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int, attributeId:Int, dataType:Int, ..
		minInterval:Int, maxInterval:Int, reportableChange:Int) = ..
		"bmx_esp32_zigbee_core_configure_reporting"
	Function _ZigbeeCoreAddGroup:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, groupId:Int) = "bmx_esp32_zigbee_core_add_group"
	Function _ZigbeeCoreRemoveGroup:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, groupId:Int) = "bmx_esp32_zigbee_core_remove_group"
	Function _ZigbeeCoreRemoveAllGroups:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int) = "bmx_esp32_zigbee_core_remove_all_groups"
	Function _ZigbeeCoreViewGroup:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, groupId:Int) = "bmx_esp32_zigbee_core_view_group"
	Function _ZigbeeCoreGetGroupMembership:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int) = "bmx_esp32_zigbee_core_get_group_membership"
	Function _ZigbeeCoreGroupOnOff:Int(sourceEndpoint:Int, groupId:Int, command:Int) = ..
		"bmx_esp32_zigbee_core_group_onoff"
	Function _ZigbeeCoreReadAttribute:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int, attributeId:Int) = ..
		"bmx_esp32_zigbee_core_read_attribute"
	Function _ZigbeeCoreWriteScalar:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int, attributeId:Int, dataType:Int, value:Int) = ..
		"bmx_esp32_zigbee_core_write_scalar"
End Extern
Public

Rem
bbdoc: Adds another standard endpoint before ZigbeeStart.
about: Call while the stack is stopped. Up to eight extra endpoints are kept
across stop/start cycles until ZigbeeClearExtraEndpoints is called. Endpoint
numbers must be distinct, including from the primary endpoint in ZigbeeStart.
End Rem
Function ZigbeeAddEndpoint:Int(device:Int, endpoint:Int)
	Return _ZigbeeCoreAddEndpoint(device, endpoint)
End Function

Rem
bbdoc: Adds another standard On/Off light or switch endpoint before ZigbeeStart.
about: Use ZigbeeAddEndpoint to add any supported standard device template.
End Rem
Function ZigbeeAddOnOffEndpoint:Int(device:Int, endpoint:Int)
	Return ZigbeeAddEndpoint(device, endpoint)
End Function

Rem
bbdoc: Removes all extra standard endpoints while the stack is stopped.
End Rem
Function ZigbeeClearExtraEndpoints:Int()
	Return _ZigbeeCoreClearEndpoints()
End Function

Rem
bbdoc: Adds an endpoint with an application-defined profile and device identity.
about: Call while stopped, before ZigbeeStart. Up to eight custom endpoints
may be added alongside the standard endpoints. The profile ID is 1 to 65534,
device ID is 0 to 65535, and version is 0 to 15. The endpoint receives a Basic
server cluster; add application clusters with ZigbeeAddCustomCluster. Endpoint
numbers must be unique, including the primary ZigbeeStart endpoint.
End Rem
Function ZigbeeAddCustomEndpoint:Int(endpoint:Int, profileId:Int, deviceId:Int, version:Int = 0)
	Return _ZigbeeCoreAddCustomEndpoint(endpoint, profileId, deviceId, version)
End Function

Rem
bbdoc: Removes all custom endpoint identities while the stack is stopped.
about: Custom clusters registered on those endpoints must be cleared separately.
End Rem
Function ZigbeeClearCustomEndpoints:Int()
	Return _ZigbeeCoreClearCustomEndpoints()
End Function

Rem
bbdoc: Adds a standard Relative Humidity Measurement server or client cluster.
about: Call before ZigbeeStart for an endpoint supplied to ZigbeeStart or
ZigbeeAddEndpoint. A server reports humidity in hundredths of a percent;
a client can discover, read and receive reports from a remote server.
End Rem
Function ZigbeeAddHumidityCluster:Int(endpoint:Int, role:Int)
	Return _ZigbeeCoreAddHumidityCluster(endpoint, role)
End Function

Rem
bbdoc: Removes configured Relative Humidity Measurement clusters while stopped.
End Rem
Function ZigbeeClearHumidityClusters:Int()
	Return _ZigbeeCoreClearHumidityClusters()
End Function

Rem
bbdoc: Adds a standard Illuminance Measurement server or client cluster.
about: Call before ZigbeeStart for a configured endpoint. A client can discover,
read, and receive reports from a remote server.
End Rem
Function ZigbeeAddIlluminanceCluster:Int(endpoint:Int, role:Int)
	Return _ZigbeeCoreAddIlluminanceCluster(endpoint, role)
End Function

Rem
bbdoc: Removes configured Illuminance Measurement clusters while stopped.
End Rem
Function ZigbeeClearIlluminanceClusters:Int()
	Return _ZigbeeCoreClearIlluminanceClusters()
End Function

Rem
bbdoc: Adds a standard Pressure Measurement server or client cluster.
about: Register it on a configured endpoint before ZigbeeStart. A server's
MeasuredValue attribute is a signed 16-bit integer in kilopascals.
End Rem
Function ZigbeeAddPressureCluster:Int(endpoint:Int, role:Int)
	Return _ZigbeeCoreAddPressureCluster(endpoint, role)
End Function

Rem
bbdoc: Removes configured Pressure Measurement clusters while stopped.
End Rem
Function ZigbeeClearPressureClusters:Int()
	Return _ZigbeeCoreClearPressureClusters()
End Function

Rem
bbdoc: Adds a standard Occupancy Sensing server or client cluster.
about: Register it on a configured endpoint before ZigbeeStart. For a server,
sensorType selects PIR (default), ultrasonic, combined, or physical contact.
The Occupancy attribute is a one-bit bitmap in an 8-bit value.
End Rem
Function ZigbeeAddOccupancyCluster:Int(endpoint:Int, role:Int, ..
		sensorType:Int = ZigbeeOccupancySensorPIR)
	Return _ZigbeeCoreAddOccupancyCluster(endpoint, role, sensorType)
End Function

Rem
bbdoc: Removes configured Occupancy Sensing clusters while stopped.
End Rem
Function ZigbeeClearOccupancyClusters:Int()
	Return _ZigbeeCoreClearOccupancyClusters()
End Function

Rem
bbdoc: Adds a Power Configuration server or client cluster to a configured endpoint.
about: The server exposes BatteryVoltage (0x0020) and
BatteryPercentageRemaining (0x0021). Register before ZigbeeStart.
End Rem
Function ZigbeeAddPowerConfigurationCluster:Int(endpoint:Int, role:Int)
	Return _ZigbeeCoreAddPowerConfigCluster(endpoint, role)
End Function

Rem
bbdoc: Removes Power Configuration clusters while the stack is stopped.
End Rem
Function ZigbeeClearPowerConfigurationClusters:Int()
	Return _ZigbeeCoreClearPowerConfigClusters()
End Function

Rem
bbdoc: Adds an AC Electrical Measurement server or client cluster to an endpoint.
about: A server exposes RMS voltage, RMS current, and active power. Register
before ZigbeeStart. The current divisor is 1000, so raw current is milliamps.
End Rem
Function ZigbeeAddElectricalMeasurementCluster:Int(endpoint:Int, role:Int)
	Return _ZigbeeCoreAddElectricalMeasurementCluster(endpoint, role)
End Function

Rem
bbdoc: Removes Electrical Measurement clusters while the stack is stopped.
End Rem
Function ZigbeeClearElectricalMeasurementClusters:Int()
	Return _ZigbeeCoreClearElectricalMeasurementClusters()
End Function

Rem
bbdoc: Adds a Level Control server or client cluster to a configured endpoint.
about: Register before ZigbeeStart. A server exposes CurrentLevel (0x0000).
End Rem
Function ZigbeeAddLevelControlCluster:Int(endpoint:Int, role:Int)
	Return _ZigbeeCoreAddLevelCluster(endpoint, role)
End Function

Rem
bbdoc: Removes Level Control clusters while the stack is stopped.
End Rem
Function ZigbeeClearLevelControlClusters:Int()
	Return _ZigbeeCoreClearLevelClusters()
End Function

Rem
bbdoc: Adds a Color Control server or client cluster to a configured endpoint.
about: Register before ZigbeeStart. The server supports CIE x/y color coordinates.
End Rem
Function ZigbeeAddColorControlCluster:Int(endpoint:Int, role:Int)
	Return _ZigbeeCoreAddColorControlCluster(endpoint, role)
End Function

Rem
bbdoc: Removes Color Control clusters while the stack is stopped.
End Rem
Function ZigbeeClearColorControlClusters:Int()
	Return _ZigbeeCoreClearColorControlClusters()
End Function

Rem
bbdoc: Adds one OTA Upgrade client on a configured endpoint before ZigbeeStart.
about: manufacturer and imageType identify firmware for this device. version is
the running image's monotonically increasing file version. The client accepts
a newer matching Zigbee OTA file with one raw firmware sub-element, validates
the ESP image, writes the inactive OTA slot, and reboots after activation.
End Rem
Function ZigbeeAddOtaClient:Int(endpoint:Int, manufacturer:Int, imageType:Int, version:UInt)
	Return _ZigbeeCoreAddOtaClient(endpoint, manufacturer, imageType, version)
End Function

Rem
bbdoc: Adds one OTA Upgrade image server on a configured endpoint before ZigbeeStart.
End Rem
Function ZigbeeAddOtaServer:Int(endpoint:Int)
	Return _ZigbeeCoreAddOtaServer(endpoint)
End Function

Rem
bbdoc: Removes configured OTA Upgrade clusters while stopped.
End Rem
Function ZigbeeClearOtaClusters:Int()
	Return _ZigbeeCoreClearOtaClusters()
End Function

Rem
bbdoc: Adds a complete Zigbee OTA file to a running image server.
about: The file must have a Zigbee OTA header and one raw firmware sub-element.
image must point to stable memory until ZigbeeOtaRemoveImage or ZigbeeStop;
IncbinPtr is suitable. Only one image can be served at a time.
End Rem
Function ZigbeeOtaServeImage:Int(endpoint:Int, image:Byte Ptr, size:UInt)
	Return _ZigbeeCoreOtaServeImage(endpoint, image, size)
End Function

Rem
bbdoc: Removes the current OTA image from a running image server.
End Rem
Function ZigbeeOtaRemoveImage:Int(endpoint:Int)
	Return _ZigbeeCoreOtaRemoveImage(endpoint)
End Function

Rem
bbdoc: Notifies a remote OTA client that a served image is available.
about: Call after ZigbeeOtaServeImage. The manufacturer, image type and version
must match the image header. The client then queries and downloads the image.
End Rem
Function ZigbeeOtaNotify:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, manufacturer:Int, imageType:Int, version:UInt)
	Return _ZigbeeCoreOtaNotify(sourceEndpoint, destinationAddress, ..
		destinationEndpoint, manufacturer, imageType, version)
End Function

Rem
bbdoc: Requests a newer image from a discovered remote OTA Upgrade server.
about: The configured client manufacturer, image type and version are sent.
The response appears as ZigbeeEventOta with signal ZigbeeOtaQueryResponse.
End Rem
Function ZigbeeOtaQuery:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int)
	Return _ZigbeeCoreOtaQuery(sourceEndpoint, destinationAddress, destinationEndpoint)
End Function

Rem
bbdoc: Adds a custom server or client cluster to a configured endpoint before ZigbeeStart.
about: Custom cluster identifiers are 0x8000 to 0xFFFF. The endpoint must be
the primary endpoint passed to ZigbeeStart or one registered with
ZigbeeAddEndpoint or ZigbeeAddCustomEndpoint. Up to eight custom clusters are supported.
End Rem
Function ZigbeeAddCustomCluster:Int(endpoint:Int, clusterId:Int, role:Int)
	Return _ZigbeeCoreAddCustomCluster(endpoint, clusterId, role)
End Function

Rem
bbdoc: Adds a scalar attribute to a custom server cluster before ZigbeeStart.
about: Combine ZigbeeAttributeRead, ZigbeeAttributeWrite, and
ZigbeeAttributeReport for access. The cluster must already be registered with
ZigbeeAddCustomCluster. Up to 32 custom attributes are supported.
End Rem
Function ZigbeeAddCustomScalar:Int(endpoint:Int, clusterId:Int, attributeId:Int, ..
		dataType:Int, access:Int, value:Int)
	Return _ZigbeeCoreAddCustomScalar(endpoint, clusterId, attributeId, dataType, ..
		access, value)
End Function

Rem
bbdoc: Adds a UTF-8 string attribute to a custom server cluster before ZigbeeStart.
about: At most 64 UTF-8 bytes are accepted. Combine ZigbeeAttributeRead and
ZigbeeAttributeWrite for access. String report configuration is not yet supported.
End Rem
Function ZigbeeAddCustomString:Int(endpoint:Int, clusterId:Int, attributeId:Int, ..
		access:Int, value:String)
	If value = Null Then Return -5
	Local length:Size_T
	Local bytes:Byte Ptr = value.ToUTF8String(length)
	If length > ZigbeeStringMaxBytes Then
		MemFree(bytes)
		Return -5
	End If
	Local result:Int = _ZigbeeCoreAddCustomString(endpoint, clusterId, attributeId, ..
		access, bytes, Int(length))
	MemFree(bytes)
	Return result
End Function

Rem
bbdoc: Adds a binary octet-string attribute to a custom server cluster.
about: Register its cluster first and call while stopped. The value may contain
zero bytes or any byte values, up to 64 bytes. Combine ZigbeeAttributeRead
and ZigbeeAttributeWrite for access. Octet-string reporting is not supported.
End Rem
Function ZigbeeAddCustomOctets:Int(endpoint:Int, clusterId:Int, attributeId:Int, ..
		access:Int, value:Byte[])
	If Not value Then Return -5
	If value.length > ZigbeeOctetsMaxBytes Then Return -5
	Return _ZigbeeCoreAddCustomOctets(endpoint, clusterId, attributeId, access, ..
		value, value.length)
End Function

Rem
bbdoc: Removes custom clusters and attributes while the stack is stopped.
End Rem
Function ZigbeeClearCustomClusters:Int()
	Return _ZigbeeCoreClearCustomClusters()
End Function

Rem
bbdoc: Starts the Zigbee stack with a supported standard endpoint.
param: Coordinator, router, end-device or sleepy-end-device role.
param: Standard On/Off light, On/Off switch, or temperature sensor device template.
param: Zigbee endpoint number from 1 to 240.
param: Primary channel bitmask; bit n selects IEEE 802.15.4 channel n (11 to 26).
param: Secondary channel bitmask used after primary-channel scanning.
param: True to form or seek a network automatically when factory new.
returns: ESP_OK when the asynchronous start was accepted, otherwise an ESP-IDF error code.
about: Coordinator and router need a `zczr` stack build; end devices need `zed`.
Set esp32.zigbee.stack to zczr or zed in custom.bmk or an app pre.bmk file to
override the board default. The stack's callbacks are delivered through
ZigbeePollEvent.
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
bbdoc: Enables or disables install-code commissioning support for the next stack start.
about: Call while stopped, before ZigbeeStart. The setting persists across
stop/start cycles until changed. Both the trust center and a joining device
must enable it to use install codes. The default is disabled.
End Rem
Function ZigbeeEnableInstallCodes:Int(enabled:Int = True)
	Return _ZigbeeCoreEnableInstallCodes(enabled)
End Function

Rem
bbdoc: Reads this device's 64-bit Zigbee IEEE address.
about: Call after ZigbeeEventStarted. The result is returned in address; its
bit pattern is the device EUI-64, even when Long prints as a negative number.
End Rem
Function ZigbeeGetIEEEAddress:Int(address:Long Var)
	Return _ZigbeeCoreIEEEAddress(address)
End Function

Rem
bbdoc: Makes a running coordinator require install codes from new joiners.
about: First enable install codes before ZigbeeStart. For a new network,
start with autoCommission=False, call this after ZigbeeEventStarted, and
then form the network. Pass False to allow ordinary commissioning again.
Existing network members are unaffected.
End Rem
Function ZigbeeRequireInstallCodes:Int(required:Int = True)
	Return _ZigbeeCoreRequireInstallCodes(required)
End Function

Rem
bbdoc: Requires trust-center link-key exchange when a device joins.
about: Call on the running coordinator and joiner after ZigbeeEventStarted,
before forming or joining a new network. The default stack policy is unchanged
unless this function is called. A saved network may bypass new commissioning.
End Rem
Function ZigbeeRequireLinkKeyExchange:Int(required:Int = True)
	Return _ZigbeeCoreRequireLinkKeyExchange(required)
End Function

Rem
bbdoc: Allows or blocks rejoins using the Zigbee well-known link key.
about: Call on the running coordinator before permitting a rejoin. False
requires a device-specific link key for that rejoin. The current setting can
be changed while running; this function does not remove existing members.
End Rem
Function ZigbeeAllowWellKnownKeyRejoins:Int(allow:Int)
	Return _ZigbeeCoreAllowWellKnownKeyRejoins(allow)
End Function

Rem
bbdoc: Provisions a joiner's install code in the running coordinator.
param: Joiner's 64-bit Zigbee IEEE address, as returned by ZigbeeGetIEEEAddress.
param: Install-code bytes including the two-byte CRC; length must be 8, 10, 14 or 18.
about: Enable install codes before ZigbeeStart and call after
ZigbeeEventStarted, before the joiner commissions. Supply the full code,
including its CRC. Provision each device's own code; do not share a single
code across devices.
End Rem
Function ZigbeeAddJoinerInstallCode:Int(address:Long, code:Byte[])
	If Not code Then Return -5
	Return _ZigbeeCoreAddJoinerInstallCode(address, code, code.length)
End Function

Rem
bbdoc: Removes a joiner's install code from the running coordinator.
about: This affects future commissioning attempts; it does not remove an
already joined device from the network.
End Rem
Function ZigbeeRemoveJoinerInstallCode:Int(address:Long)
	Return _ZigbeeCoreRemoveJoinerInstallCode(address)
End Function

Rem
bbdoc: Sets this router or end device's install code for commissioning.
param: Install-code bytes including the two-byte CRC; length must be 8, 10, 14 or 18.
about: Enable install codes before ZigbeeStart, start with autoCommission=False,
then call this after ZigbeeEventStarted and before ZigbeeJoinNetwork. Supply
the full code, including its CRC.
End Rem
Function ZigbeeSetLocalInstallCode:Int(code:Byte[])
	If Not code Then Return -5
	Return _ZigbeeCoreSetLocalInstallCode(code, code.length)
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
bbdoc: Searches the network for endpoints with a server for a cluster.
about: Results arrive as ZigbeeEventMatch with shortAddress, endpoint, and
cluster. The default profile is Home Automation (0x0104).
End Rem
Function ZigbeeFindServers:Int(clusterId:Int, profileId:Int = $0104)
	Return _ZigbeeCoreFindServers(profileId, clusterId)
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
bbdoc: Binds one local cluster to a remote endpoint.
about: The remote short address must resolve to an IEEE address in the stack's
address table. Poll for ZigbeeEventBound; its cluster and endpoint identify the
request. Bind server to client for attribute reports, or client to server for
bound commands.
End Rem
Function ZigbeeBindCluster:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int)
	Return _ZigbeeCoreBindCluster(sourceEndpoint, destinationAddress, ..
		destinationEndpoint, clusterId)
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
bbdoc: Updates a local temperature sensor's measured value.
about: Pass temperature in hundredths of a degree Celsius, from -27315 to
32767. The application obtains the measurement from its own sensor. Use
ZigbeeReadAttribute with cluster ZigbeeClusterTemperatureMeasurement and
attribute zero to read it from another device.
End Rem
Function ZigbeeSetLocalTemperature:Int(endpoint:Int, centiCelsius:Int)
	Return _ZigbeeCoreSetLocalTemperature(endpoint, centiCelsius)
End Function

Rem
bbdoc: Updates a local Relative Humidity Measurement server's measured value.
about: Pass humidity in hundredths of a percent, from 0 to 10000. The endpoint
must have a server cluster registered with ZigbeeAddHumidityCluster. Attribute
zero can be read remotely and configured for reporting with ZigbeeTypeUInt16.
End Rem
Function ZigbeeSetLocalHumidity:Int(endpoint:Int, centiPercent:Int)
	Return _ZigbeeCoreSetLocalHumidity(endpoint, centiPercent)
End Function

Rem
bbdoc: Updates a local Illuminance Measurement server's encoded value.
about: Pass the ZCL 16-bit MeasuredValue (0 to 65534); 65535 represents an
invalid or unavailable measurement. Read attribute zero remotely as
ZigbeeTypeUInt16 and configure reporting on the same attribute.
End Rem
Function ZigbeeSetLocalIlluminance:Int(endpoint:Int, encodedValue:Int)
	Return _ZigbeeCoreSetLocalIlluminance(endpoint, encodedValue)
End Function

Rem
bbdoc: Updates a local Pressure Measurement server's MeasuredValue.
about: The value is an integer number of kilopascals from -32767 to 32767;
-32768 means unavailable. Read or configure reporting on attribute zero as
ZigbeeTypeInt16.
End Rem
Function ZigbeeSetLocalPressure:Int(endpoint:Int, kilopascals:Int)
	Return _ZigbeeCoreSetLocalPressure(endpoint, kilopascals)
End Function

Rem
bbdoc: Updates a local Occupancy Sensing server's Occupancy bitmap.
about: Pass zero for unoccupied or one for occupied. Read or configure
reporting on attribute zero as ZigbeeTypeBitmap8.
End Rem
Function ZigbeeSetLocalOccupancy:Int(endpoint:Int, occupied:Int)
	Return _ZigbeeCoreSetLocalOccupancy(endpoint, occupied)
End Function

Rem
bbdoc: Updates the local battery voltage in tenths of a volt.
about: Values 0 to 254 represent 0.0 to 25.4 volts; 255 means unknown.
End Rem
Function ZigbeeSetLocalBatteryVoltage:Int(endpoint:Int, decivolts:Int)
	Return _ZigbeeCoreSetLocalBatteryVoltage(endpoint, decivolts)
End Function

Rem
bbdoc: Updates the local battery percentage in half-percent units.
about: Values 0 to 200 represent 0 to 100 percent; 255 means unknown.
End Rem
Function ZigbeeSetLocalBatteryPercentage:Int(endpoint:Int, halfPercent:Int)
	Return _ZigbeeCoreSetLocalBatteryPercentage(endpoint, halfPercent)
End Function

Rem
bbdoc: Updates a local Electrical Measurement server's RMS voltage in volts.
about: Range 0 to 65534; 65535 means unavailable.
End Rem
Function ZigbeeSetLocalRmsVoltage:Int(endpoint:Int, volts:Int)
	Return _ZigbeeCoreSetLocalRmsVoltage(endpoint, volts)
End Function

Rem
bbdoc: Updates a local Electrical Measurement server's RMS current in milliamps.
about: Range 0 to 65534; 65535 means unavailable. The server advertises a
current divisor of 1000.
End Rem
Function ZigbeeSetLocalRmsCurrent:Int(endpoint:Int, milliamps:Int)
	Return _ZigbeeCoreSetLocalRmsCurrent(endpoint, milliamps)
End Function

Rem
bbdoc: Updates a local Electrical Measurement server's active power in watts.
about: Range -32767 to 32767; -32768 means unavailable.
End Rem
Function ZigbeeSetLocalActivePower:Int(endpoint:Int, watts:Int)
	Return _ZigbeeCoreSetLocalActivePower(endpoint, watts)
End Function

Rem
bbdoc: Sets a local Level Control server's CurrentLevel attribute.
about: Level is 0 to 254. Update the physical output in the application.
End Rem
Function ZigbeeSetLocalLevel:Int(endpoint:Int, level:Int)
	Return _ZigbeeCoreSetLocalLevel(endpoint, level)
End Function

Rem
bbdoc: Sends Move To Level to a remote Level Control server.
about: Level is 0 to 254, transitionTenths is in tenths of a second, and
withOnOff selects the variant that also controls the On/Off cluster. A zero
result means the request was accepted for transmission.
End Rem
Function ZigbeeMoveToLevel:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, level:Int, transitionTenths:Int = 0, withOnOff:Int = False)
	Return _ZigbeeCoreMoveToLevel(sourceEndpoint, destinationAddress, ..
		destinationEndpoint, level, transitionTenths, withOnOff)
End Function

Rem
bbdoc: Sets a local Color Control server's CurrentX and CurrentY attributes.
about: Coordinates are unsigned CIE 1931 x/y values scaled to 0 through 65535.
Update the physical output in the application.
End Rem
Function ZigbeeSetLocalColor:Int(endpoint:Int, x:Int, y:Int)
	Return _ZigbeeCoreSetLocalColor(endpoint, x, y)
End Function

Rem
bbdoc: Sends Move To Color to a remote Color Control server.
about: Coordinates are unsigned CIE 1931 x/y values scaled to 0 through 65535.
transitionTenths is in tenths of a second. A zero result means the request
was accepted for transmission.
End Rem
Function ZigbeeMoveToColor:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, x:Int, y:Int, transitionTenths:Int = 0)
	Return _ZigbeeCoreMoveToColor(sourceEndpoint, destinationAddress, ..
		destinationEndpoint, x, y, transitionTenths)
End Function

Rem
bbdoc: Updates one local custom scalar server attribute.
about: The attribute must have been registered before ZigbeeStart. The value
uses its registered ZCL type. For a 32-bit unsigned attribute, pass the raw
bit pattern in value. This updates the Zigbee data model; the application
must also update any physical output.
End Rem
Function ZigbeeSetCustomScalar:Int(endpoint:Int, clusterId:Int, attributeId:Int, value:Int)
	Return _ZigbeeCoreSetCustomScalar(endpoint, clusterId, attributeId, value)
End Function

Rem
bbdoc: Updates a local custom UTF-8 string attribute.
about: The string attribute must be registered before ZigbeeStart and be at most
64 UTF-8 bytes long.
End Rem
Function ZigbeeSetCustomString:Int(endpoint:Int, clusterId:Int, attributeId:Int, value:String)
	If value = Null Then Return -5
	Local length:Size_T
	Local bytes:Byte Ptr = value.ToUTF8String(length)
	If length > ZigbeeStringMaxBytes Then
		MemFree(bytes)
		Return -5
	End If
	Local result:Int = _ZigbeeCoreSetCustomString(endpoint, clusterId, attributeId, ..
		bytes, Int(length))
	MemFree(bytes)
	Return result
End Function

Rem
bbdoc: Updates a local custom binary octet-string attribute.
about: The attribute must have been registered before ZigbeeStart. At most
64 bytes are accepted.
End Rem
Function ZigbeeSetCustomOctets:Int(endpoint:Int, clusterId:Int, attributeId:Int, value:Byte[])
	If Not value Then Return -5
	If value.length > ZigbeeOctetsMaxBytes Then Return -5
	Return _ZigbeeCoreSetCustomOctets(endpoint, clusterId, attributeId, value, value.length)
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
bbdoc: Requests reporting for one scalar attribute on a remote endpoint.
about: The ZCL dataType must match the remote attribute. Intervals are seconds;
maxInterval 65535 disables periodic reports. For numeric attributes,
reportableChange is the minimum value change that triggers a report. The
server needs a binding to the requesting client endpoint to send reports.
Poll for ZigbeeEventReportingConfigured and ZigbeeEventReport.
End Rem
Function ZigbeeConfigureReporting:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int, attributeId:Int, dataType:Int, ..
		minInterval:Int, maxInterval:Int, reportableChange:Int = 0)
	Return _ZigbeeCoreConfigureReporting(sourceEndpoint, destinationAddress, ..
		destinationEndpoint, clusterId, attributeId, dataType, minInterval, ..
		maxInterval, reportableChange)
End Function

Rem
bbdoc: Adds a remote Groups server endpoint to a group.
about: Group identifiers from 1 to 65527 are accepted. Poll for
ZigbeeEventGroupAdded to check the remote response.
End Rem
Function ZigbeeAddGroup:Int(sourceEndpoint:Int, destinationAddress:Int, destinationEndpoint:Int, groupId:Int)
	Return _ZigbeeCoreAddGroup(sourceEndpoint, destinationAddress, destinationEndpoint, groupId)
End Function

Rem
bbdoc: Removes a remote Groups server endpoint from a group.
about: Poll for ZigbeeEventGroupRemoved to check the remote response.
End Rem
Function ZigbeeRemoveGroup:Int(sourceEndpoint:Int, destinationAddress:Int, destinationEndpoint:Int, groupId:Int)
	Return _ZigbeeCoreRemoveGroup(sourceEndpoint, destinationAddress, destinationEndpoint, groupId)
End Function

Rem
bbdoc: Removes every group association from one remote Groups server endpoint.
about: This ZCL command has no specific response. Query membership afterward to
confirm the endpoint's group table is empty.
End Rem
Function ZigbeeRemoveAllGroups:Int(sourceEndpoint:Int, destinationAddress:Int, destinationEndpoint:Int)
	Return _ZigbeeCoreRemoveAllGroups(sourceEndpoint, destinationAddress, destinationEndpoint)
End Function

Rem
bbdoc: Queries whether a remote Groups server belongs to one group.
about: Poll for ZigbeeEventGroupViewed. Status zero means the group exists;
value contains the requested group ID.
End Rem
Function ZigbeeViewGroup:Int(sourceEndpoint:Int, destinationAddress:Int, destinationEndpoint:Int, groupId:Int)
	Return _ZigbeeCoreViewGroup(sourceEndpoint, destinationAddress, destinationEndpoint, groupId)
End Function

Rem
bbdoc: Requests every group ID on a remote Groups server endpoint.
about: Poll for zero or more ZigbeeEventGroupMember records, followed by
ZigbeeEventGroupMembershipComplete. The complete event's value is the number
of group IDs and its attribute field is remaining table capacity. All records
carry the local endpoint and remote shortAddress. Check ZigbeeDroppedEvents
if the application might not drain the bounded event queue quickly enough.
End Rem
Function ZigbeeGetGroupMembership:Int(sourceEndpoint:Int, destinationAddress:Int, destinationEndpoint:Int)
	Return _ZigbeeCoreGetGroupMembership(sourceEndpoint, destinationAddress, destinationEndpoint)
End Function

Rem
bbdoc: Sends an On/Off command to all endpoints in a Zigbee group.
about: Command zero turns off, one turns on, and two toggles. The source must
be a local On/Off switch endpoint. No per-device response is expected from a
group-addressed command.
End Rem
Function ZigbeeSendGroupOnOff:Int(sourceEndpoint:Int, groupId:Int, command:Int)
	Return _ZigbeeCoreGroupOnOff(sourceEndpoint, groupId, command)
End Function

Rem
bbdoc: Requests one attribute from any remote cluster.
about: The response is a ZigbeeEventRead. Boolean, 8-bit, 16-bit and 32-bit
integer, bitmap, enum and raw data types are copied into value with their
ZCL dataType and valueSize. Character strings appear in text and octet strings
in bytes. Other types report valueSize zero.
End Rem
Function ZigbeeReadAttribute:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int, attributeId:Int)
	Return _ZigbeeCoreReadAttribute(sourceEndpoint, destinationAddress, ..
		destinationEndpoint, clusterId, attributeId)
End Function

Rem
bbdoc: Writes one remote Boolean or 8/16/32-bit scalar attribute.
about: dataType is a ZCL scalar type; value holds the integer or its raw
32-bit bit pattern. Poll for ZigbeeEventWrite to inspect the remote status.
End Rem
Function ZigbeeWriteScalar:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int, attributeId:Int, dataType:Int, value:Int)
	Return _ZigbeeCoreWriteScalar(sourceEndpoint, destinationAddress, ..
		destinationEndpoint, clusterId, attributeId, dataType, value)
End Function

Rem
bbdoc: Writes one remote UTF-8 string attribute.
about: At most 64 UTF-8 bytes are accepted. Poll for ZigbeeEventWrite to
inspect the remote status.
End Rem
Function ZigbeeWriteString:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int, attributeId:Int, value:String)
	If value = Null Then Return -5
	Local length:Size_T
	Local bytes:Byte Ptr = value.ToUTF8String(length)
	If length > ZigbeeStringMaxBytes Then
		MemFree(bytes)
		Return -5
	End If
	Local result:Int = _ZigbeeCoreWriteString(sourceEndpoint, destinationAddress, ..
		destinationEndpoint, clusterId, attributeId, bytes, Int(length))
	MemFree(bytes)
	Return result
End Function

Rem
bbdoc: Writes one remote binary octet-string attribute.
about: At most 64 bytes are accepted. Poll for ZigbeeEventWrite to inspect
the remote status.
End Rem
Function ZigbeeWriteOctets:Int(sourceEndpoint:Int, destinationAddress:Int, ..
		destinationEndpoint:Int, clusterId:Int, attributeId:Int, value:Byte[])
	If Not value Then Return -5
	If value.length > ZigbeeOctetsMaxBytes Then Return -5
	Return _ZigbeeCoreWriteOctets(sourceEndpoint, destinationAddress, destinationEndpoint, ..
		clusterId, attributeId, value, value.length)
End Function

Rem
bbdoc: Removes the oldest event from the native Zigbee queue.
returns: True if an event was returned; False if the queue is empty.
about: Native callbacks copy bounded values into a queue. The
application thread is the only place this function creates a BlitzMax value.
End Rem
Function ZigbeePollEvent:Int(event:SZigbeeEvent Var)
	Local bytes:Byte[ZigbeeStringMaxBytes]
	Local found:Int = _ZigbeeCorePoll(event.kind, event.status, event.signal, event.endpoint, ..
		event.cluster, event.attribute, event.value, event.shortAddress, event.panId, ..
		event.channel, event.factoryNew, event.dataType, event.valueSize, bytes, ..
		ZigbeeStringMaxBytes)
	If found Then
		event.text = ""
		event.bytes = Null
		If event.dataType = ZigbeeTypeString And event.valueSize > 0 Then
			event.text = String.FromUTF8Bytes(bytes, event.valueSize)
		Else If event.dataType = ZigbeeTypeOctets And event.valueSize >= 0 And ..
				event.valueSize <= ZigbeeOctetsMaxBytes Then
			event.bytes = New Byte[event.valueSize]
			If event.valueSize > 0 Then MemCopy(Varptr event.bytes[0], Varptr bytes[0], ..
				Size_T(event.valueSize))
		End If
	End If
	Return found
End Function

Rem
bbdoc: Returns the number of Zigbee events discarded because the queue filled.
about: This count resets on each ZigbeeStart and saturates at the UInt maximum.
End Rem
Function ZigbeeDroppedEvents:UInt()
	Return _ZigbeeCoreDropped()
End Function
?
