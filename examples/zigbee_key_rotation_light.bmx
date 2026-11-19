SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee
Import ESP32.Random
Import ESP32.Storage.NVS

' One-shot coordinator test. The NVS marker prevents a second rotation after
' reboot; use a fresh network/test namespace for another run.
Local store:TNVSNamespace = OpenNVS("zb_key_test")
If Not store Then RuntimeError "Rotation state open failed"
Local attempted:Int
Local result:Int = store.GetInt("attempted", attempted)
If result <> 0 And result <> NVSErrorNotFound Then RuntimeError "Rotation state read failed: " + result
result = ZigbeeStart(ZigbeeRoleCoordinator, ZigbeeDeviceOnOffLight, 10)
If result Then RuntimeError "Zigbee start failed: " + result
Print "ROTATE coordinator starting attempted=" + attempted

Local event:SZigbeeEvent
Local changes:Int
Local phase:Int
Local due:UInt
Local key:Byte[]
Const sequence:Int = 1 ' Fresh test network starts at sequence zero.
While True
	While ZigbeePollEvent(event)
		Print "Zigbee event " + event.kind + " status=" + event.status + ..
			" endpoint=" + event.endpoint + " cluster=" + event.cluster + ..
			" attribute=" + event.attribute + " value=" + event.value
		If event.kind = ZigbeeEventStartup And event.status = 0 Then
			result = ZigbeeSetLocalOnOff(10, 0)
			Print "ROTATE initial light result=" + result
		End If
		If (event.kind = ZigbeeEventFormed And event.status = 0) Or ..
			(event.kind = ZigbeeEventStartup And event.status = 0 And Not event.factoryNew) Then
			result = ZigbeePermitJoining(180)
			Print "ROTATE permit joining result=" + result
		End If
		If event.kind = ZigbeeEventAttribute And event.status = 0 And ..
			event.endpoint = 10 And event.cluster = 6 And event.attribute = 0 Then
			changes :+ 1
			Print "ROTATE light change=" + changes + " phase=" + phase + " value=" + event.value
			If changes >= 3 And phase = 0 And Not attempted Then
				' Persist before changing the network: a reboot must not rotate again.
				result = store.SetInt("attempted", 1)
				If result = 0 Then result = store.Commit()
				If result Then RuntimeError "Rotation state commit failed: " + result
				attempted = 1
				key = New Byte[16]
				If Not ESP32FillRandom(key, key.length) Then RuntimeError "Key generation failed"
				phase = 1
				due = MilliSecs() + 3000
				Print "ROTATE baseline traffic verified"
			End If
		End If
	Wend
	If phase = 1 And MilliSecs() >= due Then
		result = ZigbeeBroadcastNetworkKey(key, sequence)
		Print "ROTATE distribute result=" + result + " sequence=" + sequence
		If result Then RuntimeError "Network key distribution failed"
		phase = 2
		due = MilliSecs() + 30000
	End If
	If phase = 2 And MilliSecs() >= due Then
		result = ZigbeeBroadcastNetworkKeySwitch(sequence)
		Print "ROTATE announce switch result=" + result + " sequence=" + sequence
		If result Then RuntimeError "Network key switch broadcast failed"
		phase = 3
		due = MilliSecs() + 3000
	End If
	If phase = 3 And MilliSecs() >= due Then
		result = ZigbeeSwitchNetworkKey(key, sequence)
		Print "ROTATE local switch result=" + result + " sequence=" + sequence
		If result Then RuntimeError "Local network key switch failed"
		phase = 4
		key = Null
	End If
	Delay 20
Wend
