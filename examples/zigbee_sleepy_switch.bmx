SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

' The application yields between Zigbee actions so ESP-IDF can enter light sleep.
' Use a coordinator running zigbee_core_light.bmx on another board.
Local result:Int = ZigbeeStart(ZigbeeRoleSleepyEndDevice, ZigbeeDeviceOnOffSwitch, 1)
If result Then RuntimeError "Sleepy Zigbee start failed: " + result
Print "Sleepy Zigbee switch starting"

Local joined:Int
Local bound:Int
Local binding:Int
Local nextFind:UInt = MilliSecs() + 1000
Local nextToggle:UInt = MilliSecs() + 15000
Local event:SZigbeeEvent

While True
	While ZigbeePollEvent(event)
		Print "Sleepy Zigbee event " + event.kind + " status=" + event.status + ..
			" pan=" + event.panId + " address=" + event.shortAddress + ..
			" endpoint=" + event.endpoint + " cluster=" + event.cluster
		If event.kind = ZigbeeEventJoined And event.status = 0 Then joined = True
		If event.kind = ZigbeeEventStartup And event.status = 0 And Not event.factoryNew Then joined = True
		If event.kind = ZigbeeEventLeft Then
			joined = False
			bound = False
			binding = False
		End If
		If event.kind = ZigbeeEventMatch And event.cluster = 6 And Not bound And Not binding Then
			result = ZigbeeBindOnOff(1, event.shortAddress, event.endpoint)
			Print "Sleepy bind submission=" + result
			If result = 0 Then binding = True
		End If
		If event.kind = ZigbeeEventBound And event.cluster = 6 Then
			binding = False
			If event.status = 0 Then bound = True
		End If
	Wend
	If joined And Not bound And MilliSecs() >= nextFind Then
		result = ZigbeeFindServers(6)
		Print "Sleepy find submission=" + result
		nextFind = MilliSecs() + 15000
	End If
	If bound And MilliSecs() >= nextToggle Then
		result = ZigbeeSendBoundOnOff(1, 2)
		Print "Sleepy toggle submission=" + result
		nextToggle = MilliSecs() + 15000
	End If
	Delay 1000
Wend
