SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

If ZigbeeAddEndpoint(ZigbeeDeviceDoorLock, 14) Then RuntimeError "Door Lock endpoint failed"
If ZigbeeAddEndpoint(ZigbeeDeviceWindowCovering, 15) Then RuntimeError "Covering endpoint failed"
If ZigbeeAddEndpoint(ZigbeeDeviceDoorLockController, 13) Then RuntimeError "Client endpoint failed"
If ZigbeeAddEndpoint(ZigbeeDeviceWindowCoveringController, 16) Then RuntimeError "Covering client failed"
If ZigbeeAddCustomCluster(13, $fc10, ZigbeeClusterClient) Then RuntimeError "Custom client failed"
If ZigbeeAddIasZoneCluster(13, ZigbeeClusterClient) Then RuntimeError "IAS client failed"
If ZigbeeAddEndpoint(ZigbeeDeviceTemperatureSensor, 12) Then RuntimeError "Sensor endpoint failed"
If ZigbeeAddIasZoneCluster(12, ZigbeeClusterServer, ZigbeeIasZoneContactSwitch) Then
	RuntimeError "IAS Zone cluster failed"
End If
If ZigbeeAddCustomCluster(10, $fc10, ZigbeeClusterServer) Then RuntimeError "Custom cluster failed"
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

Local result:Int = ZigbeeStart(ZigbeeRoleCoordinator, ZigbeeDeviceOnOffLight, 10)
Print "Extended Zigbee start=" + result
If result Then RuntimeError "Zigbee start failed"

Local event:SZigbeeEvent
Local completedWrites:Int
While True
	While ZigbeePollEvent(event)
		Print "event=" + event.kind + " status=" + event.status + " endpoint=" + event.endpoint
		If event.kind = ZigbeeEventStartup And event.status = 0 Then
			Print "lock-state=" + ZigbeeSetLocalDoorLockState(14, ZigbeeDoorLockUnlocked)
			Print "covering-lift=" + ZigbeeSetLocalCoveringLift(15, 42)
			Print "zone-status=" + ZigbeeSetLocalIasZoneStatus(12, ZigbeeIasZoneAlarm1)
			Print "local-long-string=" + ZigbeeSetCustomLongString(10, $fc10, 1, "local-" + longText)
			Print "local-long-octets=" + ZigbeeSetCustomLongOctets(10, $fc10, 2, octets)
			Print "long-string-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 1)
			Print "long-octet-read=" + ZigbeeReadAttribute(13, 0, 10, $fc10, 2)
			Print "long-string-write=" + ZigbeeWriteLongString(13, 0, 10, $fc10, 1, "remote-" + longText)
			Print "long-octet-write=" + ZigbeeWriteLongOctets(13, 0, 10, $fc10, 2, octets)
			Print "lock-read=" + ZigbeeReadAttribute(13, 0, 14, ZigbeeClusterDoorLock, ZigbeeDoorLockStateAttribute)
			Print "covering-read=" + ZigbeeReadAttribute(16, 0, 15, ZigbeeClusterWindowCovering, ..
				ZigbeeWindowCoveringLiftPercentageAttribute)
			Print "zone-read=" + ZigbeeReadAttribute(13, 0, 12, ZigbeeClusterIasZone, ZigbeeIasZoneStatusAttribute)
		End If
		If event.kind = ZigbeeEventRead Then
			Print "read cluster=" + event.cluster + " attr=" + event.attribute + ..
				" size=" + event.valueSize + " value=" + event.value + " text=" + event.text
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
	Delay 20
Wend
