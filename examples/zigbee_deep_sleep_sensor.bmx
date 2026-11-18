SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee
Import ESP32.System.Power

' Run zigbee_core_light.bmx on a coordinator first. This end device keeps its
' saved network state across each deep-sleep restart.
Print "Deep-sleep sensor boot wakeCauses=" + PowerWakeCauses()
Local result:Int = ZigbeeStart(ZigbeeRoleEndDevice, ZigbeeDeviceTemperatureSensor, 12)
If result Then RuntimeError "Zigbee start failed: " + result
Print "Deep-sleep Zigbee start submitted"

Local joined:Int
Local stopping:Int
Local sleepAt:UInt
Local nextJoin:UInt
Local event:SZigbeeEvent
While True
	While ZigbeePollEvent(event)
		Print "Deep-sleep event " + event.kind + " status=" + event.status + ..
			" factoryNew=" + event.factoryNew + " endpoint=" + event.endpoint
		If event.kind = ZigbeeEventJoined And event.status = 0 Then
			joined = True
			nextJoin = 0
			Print "Deep-sleep network joined"
		End If
		If event.kind = ZigbeeEventStartup And event.status = 0 And Not event.factoryNew Then
			joined = True
			nextJoin = 0
			Print "Deep-sleep saved network restored"
		End If
		If event.kind = ZigbeeEventStartup And event.status <> 0 Then nextJoin = MilliSecs() + 1000
		If event.kind = ZigbeeEventJoined And event.status <> 0 Then nextJoin = MilliSecs() + 15000
		If event.kind = ZigbeeEventStopped Then
			If event.status Then RuntimeError "Zigbee stop failed: " + event.status
			Print "Deep-sleep Zigbee stopped"
			Delay 250
			result = Int(DeepSleepForMilliseconds(10000))
			RuntimeError "Deep sleep failed: " + result
		End If
	Wend
	If nextJoin And Not joined And MilliSecs() >= nextJoin Then
		result = ZigbeeJoinNetwork()
		Print "Deep-sleep retry join submission=" + result
		nextJoin = MilliSecs() + 15000
	End If
	If joined And Not sleepAt Then
		result = ZigbeeSetLocalTemperature(12, 2300)
		If result Then RuntimeError "Temperature update failed: " + result
		Print "Deep-sleep temperature updated"
		sleepAt = MilliSecs() + 5000
	End If
	If sleepAt And Not stopping And MilliSecs() >= sleepAt Then
		result = ZigbeeStop()
		If result Then RuntimeError "Zigbee stop request failed: " + result
		Print "Deep-sleep stop requested"
		stopping = True
	End If
	Delay 100
Wend
