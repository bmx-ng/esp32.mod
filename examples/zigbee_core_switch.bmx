SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

If ZigbeeAddCustomCluster(1, $fc00, ZigbeeClusterClient) Then RuntimeError "Custom client cluster failed"
If ZigbeeAddHumidityCluster(1, ZigbeeClusterClient) Then RuntimeError "Humidity client cluster failed"
If ZigbeeAddIlluminanceCluster(1, ZigbeeClusterClient) Then RuntimeError "Illuminance client cluster failed"
If ZigbeeAddPressureCluster(1, ZigbeeClusterClient) Then RuntimeError "Pressure client cluster failed"
If ZigbeeAddOccupancyCluster(1, ZigbeeClusterClient) Then RuntimeError "Occupancy client cluster failed"
If ZigbeeAddCustomEndpoint(2, $c0de, $1235, 1) Then RuntimeError "Custom client endpoint failed"
If ZigbeeAddCustomCluster(2, $fc01, ZigbeeClusterClient) Then RuntimeError "Custom profile client cluster failed"
Local result:Int = ZigbeeStart(ZigbeeRoleEndDevice, ZigbeeDeviceOnOffSwitch, 1)
If result Then RuntimeError "Zigbee start failed: " + result
Print "Zigbee end-device/switch starting"

Local joined:Int
Local bound:Int
Local bindPending:Int
Local reportingConfigured:Int
Local temperatureReportingConfigured:Int
Local humidityReportingConfigured:Int
Local illuminanceReportingConfigured:Int
Local pressureReportingConfigured:Int
Local occupancyReportingConfigured:Int
Local groupReady:Int
Local lightAddress:Int
Local lightEndpoint:Int
Local customAddress:Int
Local customEndpoint:Int
Local customFound:Int
Local customProfileAddress:Int
Local customProfileEndpoint:Int
Local customProfileFound:Int
Local temperatureAddress:Int
Local temperatureEndpoint:Int
Local temperatureFound:Int
Local humidityAddress:Int
Local humidityEndpoint:Int
Local humidityFound:Int
Local illuminanceAddress:Int
Local illuminanceEndpoint:Int
Local illuminanceFound:Int
Local pressureAddress:Int
Local pressureEndpoint:Int
Local pressureFound:Int
Local occupancyAddress:Int
Local occupancyEndpoint:Int
Local occupancyFound:Int
Local nextFind:UInt = MilliSecs() + 1000
Local nextCustomFind:UInt = MilliSecs() + 2000
Local nextCustomProfileFind:UInt = MilliSecs() + 2250
Local nextCustomProfileRead:UInt
Local nextTemperatureFind:UInt = MilliSecs() + 2500
Local nextTemperatureRead:UInt
Local nextHumidityFind:UInt = MilliSecs() + 2750
Local nextHumidityRead:UInt
Local nextIlluminanceFind:UInt = MilliSecs() + 3000
Local nextIlluminanceRead:UInt
Local nextPressureFind:UInt = MilliSecs() + 3250
Local nextPressureRead:UInt
Local nextOccupancyFind:UInt = MilliSecs() + 3500
Local nextOccupancyRead:UInt
Local nextCustomWrite:UInt = MilliSecs() + 8000
Local nextCustomRead:UInt
Local nextStringWrite:UInt = MilliSecs() + 9000
Local nextStringRead:UInt
Local nextOctetsWrite:UInt = MilliSecs() + 11000
Local nextOctetsRead:UInt
Local octetsWriteSeen:Int
Local nextReporting:UInt = MilliSecs() + 1000
Local nextTemperatureReporting:UInt = MilliSecs() + 1000
Local nextHumidityReporting:UInt = MilliSecs() + 1000
Local nextIlluminanceReporting:UInt = MilliSecs() + 1000
Local nextPressureReporting:UInt = MilliSecs() + 1000
Local nextOccupancyReporting:UInt = MilliSecs() + 1000
Local nextGroup:UInt = MilliSecs() + 1000
Local nextGroupView:UInt
Local nextGroupMembership:UInt
Local nextRemoveAllGroups:UInt = MilliSecs() + 20000
Local nextEmptyGroupQuery:UInt
Local awaitingEmptyGroups:Int
Local nextRevision:UInt = MilliSecs() + 3000
Local nextIdentifyWrite:UInt = MilliSecs() + 5000
Local nextIdentifyRead:UInt
Local nextToggle:UInt = MilliSecs() + 10000
Local nextRead:UInt
Local event:SZigbeeEvent
While True
	While ZigbeePollEvent(event)
		Print "Zigbee event " + event.kind + " status=" + event.status + ..
			" pan=" + event.panId + " channel=" + event.channel + " value=" + event.value + ..
			" endpoint=" + event.endpoint + " cluster=" + event.cluster + " attribute=" + event.attribute + " type=" + event.dataType + ..
			" size=" + event.valueSize + " text=" + event.text
		If event.kind = ZigbeeEventRead And event.status = 0 And event.cluster = $fc00 And ..
				event.attribute = 3 And event.dataType = ZigbeeTypeOctets And event.bytes Then
			Select event.bytes.length
				Case 4
					If event.bytes[0] = 1 And event.bytes[1] = 0 And ..
							event.bytes[2] = $fe And event.bytes[3] = $ff Then
						Print "Octets read valid=1"
						octetsWriteSeen = True
					End If
				Case 3
					If octetsWriteSeen And event.bytes[0] = 0 And ..
							event.bytes[1] = $80 And event.bytes[2] = $ff Then
						Print "Octets restored valid=1"
					End If
			End Select
		End If
		If event.kind = ZigbeeEventJoined And event.status = 0 Then joined = True
		If event.kind = ZigbeeEventStartup And Not event.factoryNew Then joined = True
		If event.kind = ZigbeeEventMatch And event.cluster = 6 Then
			If Not bound And Not bindPending Then
				lightAddress = event.shortAddress
				lightEndpoint = event.endpoint
				result = ZigbeeBindCluster(1, lightAddress, lightEndpoint, 6)
				Print "Bind submission=" + result
				If result = 0 Then bindPending = True
			End If
		End If
		If event.kind = ZigbeeEventMatch And event.cluster = $fc00 Then
			customAddress = event.shortAddress
			customEndpoint = event.endpoint
			If Not customFound Then
				nextCustomRead = MilliSecs() + 100
				nextStringRead = MilliSecs() + 200
			End If
			customFound = True
		End If
		If event.kind = ZigbeeEventMatch And event.cluster = $fc01 Then
			customProfileAddress = event.shortAddress
			customProfileEndpoint = event.endpoint
			customProfileFound = True
			nextCustomProfileRead = MilliSecs() + 100
		End If
		If event.kind = ZigbeeEventMatch And event.cluster = ZigbeeClusterTemperatureMeasurement Then
			temperatureAddress = event.shortAddress
			temperatureEndpoint = event.endpoint
			temperatureFound = True
			nextTemperatureRead = MilliSecs() + 200
		End If
		If event.kind = ZigbeeEventMatch And event.cluster = ZigbeeClusterRelativeHumidityMeasurement Then
			humidityAddress = event.shortAddress
			humidityEndpoint = event.endpoint
			humidityFound = True
			nextHumidityRead = MilliSecs() + 200
		End If
		If event.kind = ZigbeeEventMatch And event.cluster = ZigbeeClusterIlluminanceMeasurement Then
			illuminanceAddress = event.shortAddress
			illuminanceEndpoint = event.endpoint
			illuminanceFound = True
			nextIlluminanceRead = MilliSecs() + 200
		End If
		If event.kind = ZigbeeEventMatch And event.cluster = ZigbeeClusterPressureMeasurement Then
			pressureAddress = event.shortAddress
			pressureEndpoint = event.endpoint
			pressureFound = True
			nextPressureRead = MilliSecs() + 200
		End If
		If event.kind = ZigbeeEventMatch And event.cluster = ZigbeeClusterOccupancySensing Then
			occupancyAddress = event.shortAddress
			occupancyEndpoint = event.endpoint
			occupancyFound = True
			nextOccupancyRead = MilliSecs() + 200
		End If
		If event.kind = ZigbeeEventBound And event.cluster = 6 Then
			bindPending = False
			If event.status = 0 Then bound = True
		End If
		If event.kind = ZigbeeEventReportingConfigured And event.status = 0 Then
			If event.cluster = 6 Then reportingConfigured = True
			If event.cluster = ZigbeeClusterTemperatureMeasurement Then temperatureReportingConfigured = True
			If event.cluster = ZigbeeClusterRelativeHumidityMeasurement Then humidityReportingConfigured = True
			If event.cluster = ZigbeeClusterIlluminanceMeasurement Then illuminanceReportingConfigured = True
			If event.cluster = ZigbeeClusterPressureMeasurement Then pressureReportingConfigured = True
			If event.cluster = ZigbeeClusterOccupancySensing Then occupancyReportingConfigured = True
		End If
		If event.kind = ZigbeeEventGroupAdded And event.status = 0 Then
			groupReady = True
			nextGroupView = MilliSecs() + 100
			nextGroupMembership = MilliSecs() + 300
		End If
		If event.kind = ZigbeeEventGroupMembershipComplete And awaitingEmptyGroups Then
			Print "Group table empty=" + Int(event.status = 0 And event.value = 0)
			awaitingEmptyGroups = False
		End If
	Wend
	If joined And MilliSecs() >= nextFind Then
		result = ZigbeeFindServers(6)
		Print "Find light submission=" + result
		nextFind = MilliSecs() + 15000
	End If
	If joined And MilliSecs() >= nextCustomFind Then
		result = ZigbeeFindServers($fc00)
		Print "Find custom cluster submission=" + result
		nextCustomFind = MilliSecs() + 15000
	End If
	If joined And MilliSecs() >= nextCustomProfileFind Then
		result = ZigbeeFindServers($fc01, $c0de)
		Print "Find custom profile submission=" + result
		nextCustomProfileFind = MilliSecs() + 15000
	End If
	If customProfileFound And nextCustomProfileRead And MilliSecs() >= nextCustomProfileRead Then
		result = ZigbeeReadAttribute(2, customProfileAddress, customProfileEndpoint, $fc01, 1)
		Print "Custom profile read submission=" + result
		nextCustomProfileRead = MilliSecs() + 15000
	End If
	If joined And MilliSecs() >= nextTemperatureFind Then
		result = ZigbeeFindServers(ZigbeeClusterTemperatureMeasurement)
		Print "Find temperature submission=" + result
		nextTemperatureFind = MilliSecs() + 15000
	End If
	If joined And MilliSecs() >= nextHumidityFind Then
		result = ZigbeeFindServers(ZigbeeClusterRelativeHumidityMeasurement)
		Print "Find humidity submission=" + result
		nextHumidityFind = MilliSecs() + 15000
	End If
	If joined And MilliSecs() >= nextIlluminanceFind Then
		result = ZigbeeFindServers(ZigbeeClusterIlluminanceMeasurement)
		Print "Find illuminance submission=" + result
		nextIlluminanceFind = MilliSecs() + 15000
	End If
	If joined And MilliSecs() >= nextPressureFind Then
		result = ZigbeeFindServers(ZigbeeClusterPressureMeasurement)
		Print "Find pressure submission=" + result
		nextPressureFind = MilliSecs() + 15000
	End If
	If joined And MilliSecs() >= nextOccupancyFind Then
		result = ZigbeeFindServers(ZigbeeClusterOccupancySensing)
		Print "Find occupancy submission=" + result
		nextOccupancyFind = MilliSecs() + 15000
	End If
	If temperatureFound And nextTemperatureRead And MilliSecs() >= nextTemperatureRead Then
		result = ZigbeeReadAttribute(1, temperatureAddress, temperatureEndpoint, ..
			ZigbeeClusterTemperatureMeasurement, 0)
		Print "Temperature read submission=" + result
		nextTemperatureRead = MilliSecs() + 15000
	End If
	If humidityFound And nextHumidityRead And MilliSecs() >= nextHumidityRead Then
		result = ZigbeeReadAttribute(1, humidityAddress, humidityEndpoint, ..
			ZigbeeClusterRelativeHumidityMeasurement, 0)
		Print "Humidity read submission=" + result
		nextHumidityRead = MilliSecs() + 15000
	End If
	If illuminanceFound And nextIlluminanceRead And MilliSecs() >= nextIlluminanceRead Then
		result = ZigbeeReadAttribute(1, illuminanceAddress, illuminanceEndpoint, ..
			ZigbeeClusterIlluminanceMeasurement, 0)
		Print "Illuminance read submission=" + result
		nextIlluminanceRead = MilliSecs() + 15000
	End If
	If pressureFound And nextPressureRead And MilliSecs() >= nextPressureRead Then
		result = ZigbeeReadAttribute(1, pressureAddress, pressureEndpoint, ..
			ZigbeeClusterPressureMeasurement, 0)
		Print "Pressure read submission=" + result
		nextPressureRead = MilliSecs() + 15000
	End If
	If occupancyFound And nextOccupancyRead And MilliSecs() >= nextOccupancyRead Then
		result = ZigbeeReadAttribute(1, occupancyAddress, occupancyEndpoint, ..
			ZigbeeClusterOccupancySensing, 0)
		Print "Occupancy read submission=" + result
		nextOccupancyRead = MilliSecs() + 15000
	End If
	If customFound And MilliSecs() >= nextCustomWrite Then
		result = ZigbeeWriteScalar(1, customAddress, customEndpoint, $fc00, 1, ZigbeeTypeUInt16, 43)
		Print "Custom write submission=" + result
		If result = 0 Then nextCustomRead = MilliSecs() + 200
		nextCustomWrite = MilliSecs() + 15000
	End If
	If nextCustomRead And MilliSecs() >= nextCustomRead Then
		result = ZigbeeReadAttribute(1, customAddress, customEndpoint, $fc00, 1)
		Print "Custom read submission=" + result
		nextCustomRead = MilliSecs() + 5000
	End If
	If customFound And nextStringWrite And MilliSecs() >= nextStringWrite Then
		result = ZigbeeWriteString(1, customAddress, customEndpoint, $fc00, 2, "Zigbee "+Chr($e9))
		Print "String write submission=" + result
		If result = 0 Then nextStringRead = MilliSecs() + 300
		nextStringWrite = MilliSecs() + 25000
	End If
	If nextStringRead And MilliSecs() >= nextStringRead Then
		result = ZigbeeReadAttribute(1, customAddress, customEndpoint, $fc00, 2)
		Print "String read submission=" + result
		nextStringRead = MilliSecs() + 3000
	End If
	If customFound And MilliSecs() >= nextOctetsWrite Then
		Local payload:Byte[] = [Byte(1), Byte(0), Byte($fe), Byte($ff)]
		result = ZigbeeWriteOctets(1, customAddress, customEndpoint, $fc00, 3, payload)
		Print "Octets write submission=" + result
		If result = 0 Then nextOctetsRead = MilliSecs() + 300
		nextOctetsWrite = MilliSecs() + 30000
	End If
	If nextOctetsRead And MilliSecs() >= nextOctetsRead Then
		result = ZigbeeReadAttribute(1, customAddress, customEndpoint, $fc00, 3)
		Print "Octets read submission=" + result
		nextOctetsRead = MilliSecs() + 3000
	End If
	If bound And Not reportingConfigured And MilliSecs() >= nextReporting Then
		result = ZigbeeConfigureReporting(1, lightAddress, lightEndpoint, 6, 0, ZigbeeTypeBool, 0, 5)
		Print "Reporting submission=" + result
		nextReporting = MilliSecs() + 10000
	End If
	If temperatureFound And Not temperatureReportingConfigured And MilliSecs() >= nextTemperatureReporting Then
		result = ZigbeeConfigureReporting(1, temperatureAddress, temperatureEndpoint, ..
			ZigbeeClusterTemperatureMeasurement, 0, ZigbeeTypeInt16, 0, 5, 5)
		Print "Temperature reporting submission=" + result
		nextTemperatureReporting = MilliSecs() + 10000
	End If
	If humidityFound And Not humidityReportingConfigured And MilliSecs() >= nextHumidityReporting Then
		result = ZigbeeConfigureReporting(1, humidityAddress, humidityEndpoint, ..
			ZigbeeClusterRelativeHumidityMeasurement, 0, ZigbeeTypeUInt16, 0, 5, 5)
		Print "Humidity reporting submission=" + result
		nextHumidityReporting = MilliSecs() + 10000
	End If
	If illuminanceFound And Not illuminanceReportingConfigured And MilliSecs() >= nextIlluminanceReporting Then
		result = ZigbeeConfigureReporting(1, illuminanceAddress, illuminanceEndpoint, ..
			ZigbeeClusterIlluminanceMeasurement, 0, ZigbeeTypeUInt16, 0, 5, 50)
		Print "Illuminance reporting submission=" + result
		nextIlluminanceReporting = MilliSecs() + 10000
	End If
	If pressureFound And Not pressureReportingConfigured And MilliSecs() >= nextPressureReporting Then
		result = ZigbeeConfigureReporting(1, pressureAddress, pressureEndpoint, ..
			ZigbeeClusterPressureMeasurement, 0, ZigbeeTypeInt16, 0, 5, 1)
		Print "Pressure reporting submission=" + result
		nextPressureReporting = MilliSecs() + 10000
	End If
	If occupancyFound And Not occupancyReportingConfigured And MilliSecs() >= nextOccupancyReporting Then
		result = ZigbeeConfigureReporting(1, occupancyAddress, occupancyEndpoint, ..
			ZigbeeClusterOccupancySensing, 0, ZigbeeTypeBitmap8, 0, 5)
		Print "Occupancy reporting submission=" + result
		nextOccupancyReporting = MilliSecs() + 10000
	End If
	If bound And Not groupReady And MilliSecs() >= nextGroup Then
		result = ZigbeeAddGroup(1, lightAddress, lightEndpoint, 4660)
		Print "Add group submission=" + result
		nextGroup = MilliSecs() + 10000
	End If
	If groupReady And MilliSecs() >= nextRemoveAllGroups Then
		result = ZigbeeRemoveAllGroups(1, lightAddress, lightEndpoint)
		Print "Remove all groups submission=" + result
		If result = 0 Then
			groupReady = False
			nextGroupView = 0
			nextGroupMembership = 0
			nextEmptyGroupQuery = MilliSecs() + 1500
			nextGroup = MilliSecs() + 5000
		End If
		nextRemoveAllGroups = MilliSecs() + 30000
	End If
	If nextEmptyGroupQuery And MilliSecs() >= nextEmptyGroupQuery Then
		result = ZigbeeGetGroupMembership(1, lightAddress, lightEndpoint)
		Print "Empty group query submission=" + result
		If result = 0 Then awaitingEmptyGroups = True
		nextEmptyGroupQuery = 0
	End If
	If groupReady And nextGroupView And MilliSecs() >= nextGroupView Then
		result = ZigbeeViewGroup(1, lightAddress, lightEndpoint, 4660)
		Print "View group submission=" + result
		nextGroupView = MilliSecs() + 15000
	End If
	If groupReady And nextGroupMembership And MilliSecs() >= nextGroupMembership Then
		result = ZigbeeGetGroupMembership(1, lightAddress, lightEndpoint)
		Print "Group membership submission=" + result
		nextGroupMembership = MilliSecs() + 15000
	End If
	If bound And MilliSecs() >= nextRevision Then
		result = ZigbeeReadAttribute(1, lightAddress, lightEndpoint, 6, $fffd)
		Print "Cluster revision read submission=" + result
		nextRevision = MilliSecs() + 15000
	End If
	If bound And MilliSecs() >= nextIdentifyWrite Then
		result = ZigbeeWriteScalar(1, lightAddress, lightEndpoint, 3, 0, ZigbeeTypeUInt16, 3)
		Print "Identify write submission=" + result
		If result = 0 Then nextIdentifyRead = MilliSecs() + 200
		nextIdentifyWrite = MilliSecs() + 15000
	End If
	If nextIdentifyRead And MilliSecs() >= nextIdentifyRead Then
		result = ZigbeeReadAttribute(1, lightAddress, lightEndpoint, 3, 0)
		Print "Identify read submission=" + result
		nextIdentifyRead = 0
	End If
	If joined And bound And MilliSecs() >= nextToggle Then
		If groupReady Then
			result = ZigbeeSendGroupOnOff(1, 4660, 2)
			Print "Group toggle submission=" + result
		Else
			result = ZigbeeSendBoundOnOff(1, 2)
			Print "Toggle submission=" + result
		End If
		nextToggle = MilliSecs() + 10000
		nextRead = MilliSecs() + 500
	End If
	If nextRead And MilliSecs() >= nextRead Then
		result = ZigbeeReadOnOff(1, lightAddress, lightEndpoint)
		Print "Read submission=" + result
		nextRead = 0
	End If
	Delay 20
Wend
