SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

If ZigbeeAddCustomCluster(1, $fc00, ZigbeeClusterClient) Then RuntimeError "Custom client cluster failed"
If ZigbeeAddHumidityCluster(1, ZigbeeClusterClient) Then RuntimeError "Humidity client cluster failed"
Local result:Int = ZigbeeStart(ZigbeeRoleEndDevice, ZigbeeDeviceOnOffSwitch, 1)
If result Then RuntimeError "Zigbee start failed: " + result
Print "Zigbee end-device/switch starting"

Local joined:Int
Local bound:Int
Local bindPending:Int
Local reportingConfigured:Int
Local temperatureReportingConfigured:Int
Local humidityReportingConfigured:Int
Local groupReady:Int
Local lightAddress:Int
Local lightEndpoint:Int
Local customAddress:Int
Local customEndpoint:Int
Local customFound:Int
Local temperatureAddress:Int
Local temperatureEndpoint:Int
Local temperatureFound:Int
Local humidityAddress:Int
Local humidityEndpoint:Int
Local humidityFound:Int
Local nextFind:UInt = MilliSecs() + 1000
Local nextCustomFind:UInt = MilliSecs() + 2000
Local nextTemperatureFind:UInt = MilliSecs() + 2500
Local nextTemperatureRead:UInt
Local nextHumidityFind:UInt = MilliSecs() + 2750
Local nextHumidityRead:UInt
Local nextCustomWrite:UInt = MilliSecs() + 8000
Local nextCustomRead:UInt
Local nextReporting:UInt = MilliSecs() + 1000
Local nextTemperatureReporting:UInt = MilliSecs() + 1000
Local nextHumidityReporting:UInt = MilliSecs() + 1000
Local nextGroup:UInt = MilliSecs() + 1000
Local nextGroupView:UInt
Local nextGroupMembership:UInt
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
			" size=" + event.valueSize
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
			If Not customFound Then nextCustomRead = MilliSecs() + 100
			customFound = True
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
		If event.kind = ZigbeeEventBound And event.cluster = 6 Then
			bindPending = False
			If event.status = 0 Then bound = True
		End If
		If event.kind = ZigbeeEventReportingConfigured And event.status = 0 Then
			If event.cluster = 6 Then reportingConfigured = True
			If event.cluster = ZigbeeClusterTemperatureMeasurement Then temperatureReportingConfigured = True
			If event.cluster = ZigbeeClusterRelativeHumidityMeasurement Then humidityReportingConfigured = True
		End If
		If event.kind = ZigbeeEventGroupAdded And event.status = 0 Then
			groupReady = True
			nextGroupView = MilliSecs() + 100
			nextGroupMembership = MilliSecs() + 300
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
	If bound And Not groupReady And MilliSecs() >= nextGroup Then
		result = ZigbeeAddGroup(1, lightAddress, lightEndpoint, 4660)
		Print "Add group submission=" + result
		nextGroup = MilliSecs() + 10000
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
