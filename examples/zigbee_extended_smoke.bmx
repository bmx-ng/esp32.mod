SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

If ZigbeeAddEndpoint(ZigbeeDeviceDoorLock, 14) Then RuntimeError "Door Lock endpoint failed"
If ZigbeeAddEndpoint(ZigbeeDeviceWindowCovering, 15) Then RuntimeError "Covering endpoint failed"
If ZigbeeAddEndpoint(ZigbeeDeviceDoorLockController, 13) Then RuntimeError "Client endpoint failed"
If ZigbeeAddEndpoint(ZigbeeDeviceWindowCoveringController, 16) Then RuntimeError "Covering client failed"
If ZigbeeAddCustomCluster(13, $fc10, ZigbeeClusterClient) Then RuntimeError "Custom client failed"
If ZigbeeAddCustomCluster(13, $fc11, ZigbeeClusterClient) Then RuntimeError "Composite-only client failed"
If ZigbeeAddIasZoneCluster(13, ZigbeeClusterClient) Then RuntimeError "IAS client failed"
If ZigbeeAddEndpoint(ZigbeeDeviceTemperatureSensor, 12) Then RuntimeError "Sensor endpoint failed"
If ZigbeeAddIasZoneCluster(12, ZigbeeClusterServer, ZigbeeIasZoneContactSwitch) Then
	RuntimeError "IAS Zone cluster failed"
End If
If ZigbeeAddCustomCluster(10, $fc10, ZigbeeClusterServer) Then RuntimeError "Custom cluster failed"
If ZigbeeAddCustomCluster(10, $fc11, ZigbeeClusterServer) Then RuntimeError "Composite-only cluster failed"
Local longText:String = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
If ZigbeeAddCustomLongString(10, $fc10, 1, ..
		ZigbeeAttributeRead | ZigbeeAttributeWrite, longText) Then
	RuntimeError "Long string attribute failed"
End If
Local octets:Byte[80]
For Local i:Int = 0 Until octets.length
	octets[i] = Byte(i)
Next
If ZigbeeAddCustomLongOctets(10, $fc10, 2, ..
		ZigbeeAttributeRead | ZigbeeAttributeWrite, octets) Then
	RuntimeError "Long octet attribute failed"
End If
Local arrayValue:Byte[] = [Byte($21), Byte(3), Byte(0), Byte(100), Byte(0), ..
	Byte(200), Byte(0), Byte(44), Byte(1)]
Local arrayWrite:Byte[] = [Byte($21), Byte(2), Byte(0), Byte(7), Byte(0), Byte(9), Byte(0)]
Local structureValue:Byte[] = [Byte(2), Byte(0), Byte($20), Byte(7), ..
	Byte($42), Byte(2), Byte(72), Byte(105)]
Local structureWrite:Byte[] = [Byte(2), Byte(0), Byte($20), Byte(8), ..
	Byte($42), Byte(2), Byte(79), Byte(75)]
Local largeArray:Byte[153]
largeArray[0] = ZigbeeTypeUInt8
largeArray[1] = 150
For Local i:Int = 0 Until 150
	largeArray[i + 3] = Byte(i)
Next
If ZigbeeAddCustomArray(10, $fc10, 3, ..
		ZigbeeAttributeRead | ZigbeeAttributeWrite, arrayValue) Then
	RuntimeError "Array attribute failed"
End If
If ZigbeeAddCustomStructure(10, $fc10, 4, ..
		ZigbeeAttributeRead | ZigbeeAttributeWrite, structureValue) Then
	RuntimeError "Structure attribute failed"
End If
If ZigbeeAddCustomArray(10, $fc10, 5, ..
		ZigbeeAttributeRead | ZigbeeAttributeWrite, largeArray) Then
	RuntimeError "Large array attribute failed"
End If
If ZigbeeAddCustomArray(10, $fc11, 1, ZigbeeAttributeRead, arrayValue) Then
	RuntimeError "Composite-only array failed"
End If
Local malformedArray:Byte[] = [Byte($21), Byte(2), Byte(0), Byte(1)]
If Not ZigbeeAddCustomArray(10, $fc10, 6, ZigbeeAttributeRead, malformedArray) Then
	RuntimeError "Malformed array was accepted"
End If
Local malformedStructure:Byte[] = [Byte(1), Byte(0), Byte($21), Byte(1)]
If Not ZigbeeAddCustomStructure(10, $fc10, 6, ..
		ZigbeeAttributeRead, malformedStructure) Then
	RuntimeError "Malformed structure was accepted"
End If

Local result:Int = ZigbeeStart(ZigbeeRoleCoordinator, ZigbeeDeviceOnOffLight, 10)
Print "Extended Zigbee start=" + result
If result Then RuntimeError "Zigbee start failed"

Local event:SZigbeeEvent
Local completedWrites:Int
Local nextCompositeCheck:UInt
Local nextCompositeSet:UInt
Local cieIeee:Long
While True
	While ZigbeePollEvent(event)
		Print "event=" + event.kind + " status=" + event.status + ..
			" signal=" + event.signal + " endpoint=" + event.endpoint
		If event.kind = ZigbeeEventStartup And event.status = 0 Then
			Print "ieee-result=" + ZigbeeGetIEEEAddress(cieIeee)
			Print "ieee=" + cieIeee
			Print "permit-join=" + ZigbeePermitJoining(180)
			Print "lock-state=" + ZigbeeSetLocalDoorLockState(14, ZigbeeDoorLockUnlocked)
			Print "covering-lift=" + ZigbeeSetLocalCoveringLift(15, 42)
			Print "zone-status=" + ZigbeeSetLocalIasZoneStatus(12, ZigbeeIasZoneAlarm1)
			Print "local-long-string=" + ZigbeeSetCustomLongString(10, $fc10, 1, "local-" + longText)
			Print "local-long-octets=" + ZigbeeSetCustomLongOctets(10, $fc10, 2, octets)
			Print "long-string-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 1)
			Print "long-octet-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 2)
			Print "long-string-write=" + ZigbeeWriteLongString(13, 0, 10, $fc10, 1, "remote-" + longText)
			Print "long-octet-write=" + ZigbeeWriteLongOctets(13, 0, 10, $fc10, 2, octets)
			Print "array-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 3)
			Print "structure-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 4)
			Print "large-array-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 5)
			Print "composite-only-read=" + ZigbeeReadAttribute(13, 0, 10, $fc11, 1)
			Print "array-write=" + ZigbeeWriteArray(13, 0, 10, $fc10, 3, arrayWrite)
			Print "structure-write=" + ZigbeeWriteStructure(13, 0, 10, $fc10, 4, structureWrite)
			nextCompositeCheck = MilliSecs() + 10000
			Print "lock-read=" + ZigbeeReadAttribute(13, 0, 14, ZigbeeClusterDoorLock, ZigbeeDoorLockStateAttribute)
			Print "covering-read=" + ZigbeeReadAttribute(16, 0, 15, ZigbeeClusterWindowCovering, ..
				ZigbeeWindowCoveringLiftPercentageAttribute)
			Print "zone-read=" + ZigbeeReadAttribute(13, 0, 12, ZigbeeClusterIasZone, ZigbeeIasZoneStatusAttribute)
			Print "lock-command=" + ZigbeeSendDoorLock(13, 0, 14, 1)
		End If
		If event.kind = ZigbeeEventDoorLockResponse Then
			Delay 200
			Print "covering-command=" + ZigbeeSendWindowCovering(16, 0, 15, ..
				ZigbeeCoveringGoToLiftPercentage, 55)
		End If
		If event.kind = ZigbeeEventActuatorCommand Then
			Print "actuator cluster=" + event.cluster + " command=" + event.value + ..
				" target=" + event.attribute + " source=" + event.shortAddress
			Print "actuator-ack=" + ZigbeeCompleteActuatorCommand(event.signal, ZigbeeCommandSuccess)
			If event.cluster = ZigbeeClusterDoorLock Then
				Print "observed-lock=" + ZigbeeSetLocalDoorLockState(event.endpoint, ZigbeeDoorLockLocked)
			Else If event.value = ZigbeeCoveringGoToLiftPercentage Then
				Print "observed-covering=" + ZigbeeSetLocalCoveringLift(event.endpoint, event.attribute)
				Delay 200
				Print "ias-enroll=" + ZigbeeRequestIasEnrollment(12, 0, 13, cieIeee)
			End If
		End If
		If event.kind = ZigbeeEventIasEnrollRequest Then
			Print "ias-request zone-type=" + event.value + " source=" + event.shortAddress + ..
				" source-ep=" + event.dataType
			Delay 1000
			Print "ias-response=" + ZigbeeRespondIasEnrollment(event.endpoint, ..
				event.shortAddress, event.dataType, 0, 3)
			If event.shortAddress Then
				Delay 500
				Print "remote-zone-read=" + ZigbeeReadAttribute(13, event.shortAddress, ..
					event.dataType, ZigbeeClusterIasZone, 0)
			End If
			Delay 200
			Print "zone-state-read=" + ZigbeeReadAttribute(13, 0, 12, ZigbeeClusterIasZone, 0)
			Print "zone-id-read=" + ZigbeeReadAttribute(13, 0, 12, ZigbeeClusterIasZone, $11)
		End If
		If event.kind = ZigbeeEventIasEnrollResponse Then
			Print "ias-enroll-result=" + event.status + " zone-id=" + event.value
			If event.status = 0 Then
				Print "zone-status-notification=" + ZigbeeSetLocalIasZoneStatus(12, ..
					ZigbeeIasZoneAlarm1 | ZigbeeIasZoneTamper)
			End If
		End If
		If event.kind = ZigbeeEventIasStatus Then
			Print "ias-status=" + event.value + " zone-id=" + event.attribute
		End If
		If event.kind = ZigbeeEventRead Then
			Print "read cluster=" + event.cluster + " attr=" + event.attribute + ..
				" size=" + event.valueSize + " value=" + event.value + " text=" + event.text
			If event.cluster = $fc10 And (event.attribute = 3 Or event.attribute = 4) And ..
					event.bytes Then
				Print "composite-read attr=" + event.attribute + " type=" + event.dataType + ..
					" size=" + event.bytes.length + " last=" + event.bytes[event.bytes.length - 1]
			End If
		End If
		If event.kind = ZigbeeEventWrite Then
			Print "write cluster=" + event.cluster + " attr=" + event.attribute + ..
				" status=" + event.status
			If event.status = 0 Then
				' Successful ZCL write responses omit the attribute ID.
				completedWrites :+ 1
				If completedWrites = 2 Then
					Print "after-write-string-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 1)
					Print "after-write-octet-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 2)
				End If
			End If
		End If
	Wend
	If nextCompositeCheck And MilliSecs() >= nextCompositeCheck Then
		Print "after-composite-array-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 3)
		Print "after-composite-structure-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 4)
		nextCompositeCheck = 0
		nextCompositeSet = MilliSecs() + 2000
	End If
	If nextCompositeSet And MilliSecs() >= nextCompositeSet Then
		Local localArray:Byte[] = [Byte($21), Byte(1), Byte(0), Byte(99), Byte(0)]
		Local localStructure:Byte[] = [Byte(1), Byte(0), Byte($20), Byte(42)]
		Print "local-array-update=" + ZigbeeSetCustomArray(10, $fc10, 3, localArray)
		Print "local-structure-update=" + ZigbeeSetCustomStructure(10, $fc10, 4, localStructure)
		Print "local-array-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 3)
		Print "local-structure-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 4)
		nextCompositeSet = 0
	End If
	Delay 20
Wend
