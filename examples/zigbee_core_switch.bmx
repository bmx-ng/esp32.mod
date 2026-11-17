SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

Local result:Int = ZigbeeStart(ZigbeeRoleEndDevice, ZigbeeDeviceOnOffSwitch, 1)
If result Then RuntimeError "Zigbee start failed: " + result
Print "Zigbee end-device/switch starting"

Local joined:Int
Local bound:Int
Local reportingConfigured:Int
Local groupReady:Int
Local lightAddress:Int
Local lightEndpoint:Int
Local nextFind:UInt = MilliSecs() + 1000
Local nextReporting:UInt = MilliSecs() + 1000
Local nextGroup:UInt = MilliSecs() + 1000
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
			" cluster=" + event.cluster + " attribute=" + event.attribute + " type=" + event.dataType + ..
			" size=" + event.valueSize
		If event.kind = ZigbeeEventJoined And event.status = 0 Then joined = True
		If event.kind = ZigbeeEventStartup And Not event.factoryNew Then joined = True
		If event.kind = ZigbeeEventOnOffMatch Then
			lightAddress = event.shortAddress
			lightEndpoint = event.endpoint
			result = ZigbeeBindOnOff(1, lightAddress, lightEndpoint)
			Print "Bind submission=" + result
		End If
		If event.kind = ZigbeeEventOnOffBound And event.status = 0 Then
			bound = True
		End If
		If event.kind = ZigbeeEventReportingConfigured And event.status = 0 Then
			reportingConfigured = True
		End If
		If event.kind = ZigbeeEventGroupAdded And event.status = 0 Then groupReady = True
	Wend
	If joined And Not bound And MilliSecs() >= nextFind Then
		result = ZigbeeFindOnOffLights()
		Print "Find light submission=" + result
		nextFind = MilliSecs() + 10000
	End If
	If bound And Not reportingConfigured And MilliSecs() >= nextReporting Then
		result = ZigbeeConfigureOnOffReporting(1, lightAddress, lightEndpoint, 0, 5)
		Print "Reporting submission=" + result
		nextReporting = MilliSecs() + 10000
	End If
	If bound And Not groupReady And MilliSecs() >= nextGroup Then
		result = ZigbeeAddGroup(1, lightAddress, lightEndpoint, 4660)
		Print "Add group submission=" + result
		nextGroup = MilliSecs() + 10000
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
