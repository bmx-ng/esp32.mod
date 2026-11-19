SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

' Pair with zigbee_extended_smoke.bmx on the C6 coordinator. Replace the
' coordinator IEEE address when using a different board.
Const CoordinatorIeee:Long = 1206300595088024144:Long

If ZigbeeAddIasZoneCluster(12, ZigbeeClusterServer, ZigbeeIasZoneContactSwitch) Then
	RuntimeError "IAS Zone cluster failed"
End If
If ZigbeeAddCustomCluster(12, $fc10, ZigbeeClusterClient) Then
	RuntimeError "Custom client cluster failed"
End If
Local result:Int = ZigbeeStart(ZigbeeRoleEndDevice, ZigbeeDeviceTemperatureSensor, 12)
If result Then RuntimeError "Zigbee start failed: " + result
Print "IAS Zone sensor starting"

Local joined:Int
Local enrolled:Int
Local nextJoin:UInt
Local nextEnroll:UInt
Local nextComposite:UInt
Local nextCompositeRead:UInt
Local event:SZigbeeEvent
While True
	While ZigbeePollEvent(event)
		Print "IAS sensor event=" + event.kind + " status=" + event.status + ..
			" value=" + event.value + " endpoint=" + event.endpoint
		If event.kind = ZigbeeEventJoined And event.status = 0 Then
			joined = True
			nextJoin = 0
			nextEnroll = 0
			nextComposite = MilliSecs() + 3000
		End If
		If event.kind = ZigbeeEventStartup And event.status = 0 And Not event.factoryNew Then
			joined = True
			nextJoin = 0
		End If
		If (event.kind = ZigbeeEventJoined Or event.kind = ZigbeeEventStartup) And event.status Then
			nextJoin = MilliSecs() + 2000
		End If
		If event.kind = ZigbeeEventIasEnrollResponse Then
			enrolled = event.status = 0
			If enrolled Then
				Print "IAS notification status=" + ZigbeeSetLocalIasZoneStatus(12, ..
					ZigbeeIasZoneAlarm1 | ZigbeeIasZoneTamper)
			End If
		End If
		If event.kind = ZigbeeEventRead And event.cluster = $fc10 And ..
				event.bytes Then
			Print "H2 composite read attr=" + event.attribute + " size=" + ..
				event.bytes.length + " first=" + event.bytes[3] + ..
				" last=" + event.bytes[event.bytes.length - 1]
		End If
		If event.kind = ZigbeeEventWrite And event.cluster = $fc10 Then
			Print "H2 composite write response=" + event.status
		End If
	Wend
	If nextJoin And Not joined And MilliSecs() >= nextJoin Then
		Print "IAS retry join=" + ZigbeeJoinNetwork()
		nextJoin = MilliSecs() + 15000
	End If
	If joined And Not enrolled And MilliSecs() >= nextEnroll Then
		result = ZigbeeRequestIasEnrollment(12, 0, 13, CoordinatorIeee)
		Print "IAS enrollment request=" + result
		If result = 0 Then
			nextEnroll = MilliSecs() + 15000
		Else
			nextEnroll = MilliSecs() + 1000
		End If
	End If
	If nextComposite And MilliSecs() >= nextComposite Then
		Local arrayWrite:Byte[] = [Byte($21), Byte(2), Byte(0), Byte(11), Byte(0), Byte(13), Byte(0)]
		Local structureWrite:Byte[] = [Byte(2), Byte(0), Byte($20), Byte(9), ..
			Byte($42), Byte(2), Byte(72), Byte(50)]
		Local largeWrite:Byte[153]
		largeWrite[0] = ZigbeeTypeUInt8
		largeWrite[1] = 150
		For Local i:Int = 0 Until 150
			largeWrite[i + 3] = Byte(149 - i)
		Next
		Print "H2 array read=" + ZigbeeReadAttribute(12, 0, 10, $fc10, 3)
		Print "H2 structure read=" + ZigbeeReadAttribute(12, 0, 10, $fc10, 4)
		Print "H2 large array read=" + ZigbeeReadAttribute(12, 0, 10, $fc10, 5)
		Print "H2 array write=" + ZigbeeWriteArray(12, 0, 10, $fc10, 3, arrayWrite)
		Print "H2 structure write=" + ZigbeeWriteStructure(12, 0, 10, $fc10, 4, structureWrite)
		Print "H2 large array write=" + ZigbeeWriteArray(12, 0, 10, $fc10, 5, largeWrite)
		nextComposite = 0
		nextCompositeRead = MilliSecs() + 5000
	End If
	If nextCompositeRead And MilliSecs() >= nextCompositeRead Then
		Print "H2 after-array read=" + ZigbeeReadAttribute(12, 0, 10, $fc10, 3)
		Print "H2 after-structure read=" + ZigbeeReadAttribute(12, 0, 10, $fc10, 4)
		Print "H2 after-large-array read=" + ZigbeeReadAttribute(12, 0, 10, $fc10, 5)
		nextCompositeRead = 0
	End If
	Delay 50
Wend
