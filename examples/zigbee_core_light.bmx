SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

If ZigbeeAddOnOffEndpoint(ZigbeeDeviceOnOffLight, 11) Then RuntimeError "Second light endpoint failed"
If ZigbeeAddCustomCluster(11, $fc00, ZigbeeClusterServer) Then RuntimeError "Custom cluster failed"
If ZigbeeAddCustomScalar(11, $fc00, 1, ZigbeeTypeUInt16, ..
		ZigbeeAttributeRead | ZigbeeAttributeWrite | ZigbeeAttributeReport, 42) Then
	RuntimeError "Custom attribute failed"
End If
Local result:Int = ZigbeeStart(ZigbeeRoleCoordinator, ZigbeeDeviceOnOffLight, 10)
If result Then RuntimeError "Zigbee start failed: " + result
Print "Zigbee coordinator/light starting"

Local event:SZigbeeEvent
While True
	While ZigbeePollEvent(event)
		Print "Zigbee event " + event.kind + " status=" + event.status + ..
			" pan=" + event.panId + " channel=" + event.channel + " value=" + event.value + ..
			" endpoint=" + event.endpoint + " cluster=" + event.cluster + " attribute=" + event.attribute
		If event.kind = ZigbeeEventStartup And event.status = 0 Then
			result = ZigbeeSetLocalOnOff(10, 0)
			Print "Initial light state result=" + result
			result = ZigbeeSetLocalOnOff(11, 0)
			Print "Second light state result=" + result
		End If
		If event.kind = ZigbeeEventDeviceAnnounced Then
			result = ZigbeeBindCluster(10, event.shortAddress, 1, 6)
			Print "Light report binding result=" + result
			result = ZigbeeBindCluster(11, event.shortAddress, 1, 6)
			Print "Second report binding result=" + result
		End If
		If event.kind = ZigbeeEventFormed Or ..
			(event.kind = ZigbeeEventStartup And Not event.factoryNew) Then
			result = ZigbeePermitJoining(180)
			Print "Permit joining result=" + result
		End If
	Wend
	Delay 20
Wend
