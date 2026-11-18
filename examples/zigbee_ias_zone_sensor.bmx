SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

' Pair with zigbee_extended_smoke.bmx on the C6 coordinator. Replace the
' coordinator IEEE address when using a different board.
Const CoordinatorIeee:Long = 1206300595088024144:Long

If ZigbeeAddIasZoneCluster(12, ZigbeeClusterServer, ZigbeeIasZoneContactSwitch) Then
	RuntimeError "IAS Zone cluster failed"
End If
Local result:Int = ZigbeeStart(ZigbeeRoleEndDevice, ZigbeeDeviceTemperatureSensor, 12)
If result Then RuntimeError "Zigbee start failed: " + result
Print "IAS Zone sensor starting"

Local joined:Int
Local enrolled:Int
Local nextJoin:UInt
Local nextEnroll:UInt
Local event:SZigbeeEvent
While True
	While ZigbeePollEvent(event)
		Print "IAS sensor event=" + event.kind + " status=" + event.status + ..
			" value=" + event.value + " endpoint=" + event.endpoint
		If event.kind = ZigbeeEventJoined And event.status = 0 Then
			joined = True
			nextJoin = 0
			nextEnroll = 0
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
	Delay 50
Wend
