SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

If ZigbeeAddOnOffEndpoint(ZigbeeDeviceOnOffLight, 11) Then RuntimeError "Second light endpoint failed"
If ZigbeeAddEndpoint(ZigbeeDeviceTemperatureSensor, 12) Then RuntimeError "Temperature endpoint failed"
If ZigbeeAddHumidityCluster(12, ZigbeeClusterServer) Then RuntimeError "Humidity cluster failed"
If ZigbeeAddIlluminanceCluster(12, ZigbeeClusterServer) Then RuntimeError "Illuminance cluster failed"
If ZigbeeAddPressureCluster(12, ZigbeeClusterServer) Then RuntimeError "Pressure cluster failed"
If ZigbeeAddOccupancyCluster(12, ZigbeeClusterServer) Then RuntimeError "Occupancy cluster failed"
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
Local initialOctets:Byte[] = [Byte($00), Byte($80), Byte($ff)]
If ZigbeeAddCustomOctets(11, $fc00, 3, ..
		ZigbeeAttributeRead | ZigbeeAttributeWrite, initialOctets) Then
	RuntimeError "Custom octets attribute failed"
End If
Local result:Int = ZigbeeStart(ZigbeeRoleCoordinator, ZigbeeDeviceOnOffLight, 10)
If result Then RuntimeError "Zigbee start failed: " + result
Print "Zigbee coordinator/light starting"

Local event:SZigbeeEvent
Local nextCustomUpdate:UInt = MilliSecs() + 20000
Local nextTemperatureUpdate:UInt = MilliSecs() + 10000
Local temperature:Int = 2345
Local humidity:Int = 4550
Local illuminance:Int = 20001
Local pressure:Int = 101
Local occupancy:Int = 0
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
			result = ZigbeeSetLocalIlluminance(12, 20001) ' Simulated encoded illuminance.
			Print "Illuminance update result=" + result
			result = ZigbeeSetLocalPressure(12, pressure)
			Print "Pressure update result=" + result + " value=" + pressure
			result = ZigbeeSetLocalOccupancy(12, occupancy)
			Print "Occupancy update result=" + result + " value=" + occupancy
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
			result = ZigbeeBindCluster(12, event.shortAddress, 1, ZigbeeClusterIlluminanceMeasurement)
			Print "Illuminance report binding result=" + result
			result = ZigbeeBindCluster(12, event.shortAddress, 1, ZigbeeClusterPressureMeasurement)
			Print "Pressure report binding result=" + result
			result = ZigbeeBindCluster(12, event.shortAddress, 1, ZigbeeClusterOccupancySensing)
			Print "Occupancy report binding result=" + result
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
		result = ZigbeeSetCustomOctets(11, $fc00, 3, initialOctets)
		Print "Custom local octets update result=" + result
		nextCustomUpdate = MilliSecs() + 20000
	End If
	If MilliSecs() >= nextTemperatureUpdate Then
		If temperature = 2345 Then temperature = 2355 Else temperature = 2345
		result = ZigbeeSetLocalTemperature(12, temperature)
		Print "Temperature update result=" + result + " value=" + temperature
		If humidity = 4550 Then humidity = 4560 Else humidity = 4550
		result = ZigbeeSetLocalHumidity(12, humidity)
		Print "Humidity update result=" + result + " value=" + humidity
		If illuminance = 20001 Then illuminance = 20101 Else illuminance = 20001
		result = ZigbeeSetLocalIlluminance(12, illuminance)
		Print "Illuminance update result=" + result + " value=" + illuminance
		If pressure = 101 Then pressure = 102 Else pressure = 101
		result = ZigbeeSetLocalPressure(12, pressure)
		Print "Pressure update result=" + result + " value=" + pressure
		occupancy = 1 - occupancy
		result = ZigbeeSetLocalOccupancy(12, occupancy)
		Print "Occupancy update result=" + result + " value=" + occupancy
		nextTemperatureUpdate = MilliSecs() + 10000
	End If
	Delay 20
Wend
