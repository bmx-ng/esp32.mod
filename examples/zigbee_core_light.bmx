SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

If ZigbeeAddOnOffEndpoint(ZigbeeDeviceOnOffLight, 11) Then RuntimeError "Second light endpoint failed"
If ZigbeeAddEndpoint(ZigbeeDeviceTemperatureSensor, 12) Then RuntimeError "Temperature endpoint failed"
If ZigbeeAddHumidityCluster(12, ZigbeeClusterServer) Then RuntimeError "Humidity cluster failed"
If ZigbeeAddCustomEndpoint(13, $c0de, $1234, 1) Then RuntimeError "Custom endpoint failed"
If ZigbeeAddCustomCluster(13, $fc01, ZigbeeClusterServer) Then RuntimeError "Custom profile cluster failed"
If ZigbeeAddCustomScalar(13, $fc01, 1, ZigbeeTypeUInt16, ZigbeeAttributeRead, 77) Then
	RuntimeError "Custom profile attribute failed"
End If
If ZigbeeAddCustomCluster(11, $fc00, ZigbeeClusterServer) Then RuntimeError "Custom cluster failed"
If ZigbeeAddCustomScalar(11, $fc00, 1, ZigbeeTypeUInt16, ..
		ZigbeeAttributeRead | ZigbeeAttributeWrite | ZigbeeAttributeReport, 42) Then
	RuntimeError "Custom attribute failed"
End If
If ZigbeeAddCustomString(11, $fc00, 2, ..
		ZigbeeAttributeRead | ZigbeeAttributeWrite, "hello") Then
	RuntimeError "Custom string attribute failed"
End If
Local result:Int = ZigbeeStart(ZigbeeRoleCoordinator, ZigbeeDeviceOnOffLight, 10)
If result Then RuntimeError "Zigbee start failed: " + result
Print "Zigbee coordinator/light starting"

Local event:SZigbeeEvent
Local nextCustomUpdate:UInt = MilliSecs() + 20000
Local nextTemperatureUpdate:UInt = MilliSecs() + 10000
Local temperature:Int = 2345
Local humidity:Int = 4550
While True
	While ZigbeePollEvent(event)
		Print "Zigbee event " + event.kind + " status=" + event.status + ..
			" pan=" + event.panId + " channel=" + event.channel + " value=" + event.value + ..
			" endpoint=" + event.endpoint + " cluster=" + event.cluster + " attribute=" + event.attribute + ..
			" text=" + event.text
		If event.kind = ZigbeeEventStartup And event.status = 0 Then
			result = ZigbeeSetLocalOnOff(10, 0)
			Print "Initial light state result=" + result
			result = ZigbeeSetLocalOnOff(11, 0)
			Print "Second light state result=" + result
			result = ZigbeeSetCustomScalar(11, $fc00, 1, 44)
			Print "Custom local state result=" + result
			result = ZigbeeSetCustomString(11, $fc00, 2, "ready")
			Print "Custom local string result=" + result
			result = ZigbeeSetLocalTemperature(12, 2345) ' Simulated 23.45 C reading.
			Print "Temperature update result=" + result
			result = ZigbeeSetLocalHumidity(12, 4550) ' Simulated 45.50% reading.
			Print "Humidity update result=" + result
		End If
		If event.kind = ZigbeeEventDeviceAnnounced Then
			result = ZigbeeBindCluster(10, event.shortAddress, 1, 6)
			Print "Light report binding result=" + result
			result = ZigbeeBindCluster(11, event.shortAddress, 1, 6)
			Print "Second report binding result=" + result
			result = ZigbeeBindCluster(12, event.shortAddress, 1, ZigbeeClusterTemperatureMeasurement)
			Print "Temperature report binding result=" + result
			result = ZigbeeBindCluster(12, event.shortAddress, 1, ZigbeeClusterRelativeHumidityMeasurement)
			Print "Humidity report binding result=" + result
		End If
		If event.kind = ZigbeeEventFormed Or ..
			(event.kind = ZigbeeEventStartup And Not event.factoryNew) Then
			result = ZigbeePermitJoining(180)
			Print "Permit joining result=" + result
		End If
	Wend
	If MilliSecs() >= nextCustomUpdate Then
		result = ZigbeeSetCustomScalar(11, $fc00, 1, 44)
		Print "Custom local update result=" + result
		result = ZigbeeSetCustomString(11, $fc00, 2, "ready")
		Print "Custom local string update result=" + result
		nextCustomUpdate = MilliSecs() + 20000
	End If
	If MilliSecs() >= nextTemperatureUpdate Then
		If temperature = 2345 Then temperature = 2355 Else temperature = 2345
		result = ZigbeeSetLocalTemperature(12, temperature)
		Print "Temperature update result=" + result + " value=" + temperature
		If humidity = 4550 Then humidity = 4560 Else humidity = 4550
		result = ZigbeeSetLocalHumidity(12, humidity)
		Print "Humidity update result=" + result + " value=" + humidity
		nextTemperatureUpdate = MilliSecs() + 10000
	End If
	Delay 20
Wend
