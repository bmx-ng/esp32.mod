SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

Local result:Int = ZigbeeStart(ZigbeeRoleEndDevice, ZigbeeDeviceOnOffSwitch, 1)
If result Then RuntimeError "Zigbee start failed: " + result
Print "Zigbee end-device/switch starting"

Local joined:Int
Local nextToggle:UInt = MilliSecs() + 10000
Local nextRead:UInt
Local event:SZigbeeEvent
While True
	While ZigbeePollEvent(event)
		Print "Zigbee event " + event.kind + " status=" + event.status + ..
			" pan=" + event.panId + " channel=" + event.channel + " value=" + event.value
		If event.kind = ZigbeeEventJoined And event.status = 0 Then joined = True
		If event.kind = ZigbeeEventStartup And Not event.factoryNew Then joined = True
	Wend
	If joined And MilliSecs() >= nextToggle Then
		result = ZigbeeSendOnOff(1, 0, 10, 2)
		Print "Toggle submission=" + result
		nextToggle = MilliSecs() + 10000
		nextRead = MilliSecs() + 500
	End If
	If nextRead And MilliSecs() >= nextRead Then
		result = ZigbeeReadOnOff(1, 0, 10)
		Print "Read submission=" + result
		nextRead = 0
	End If
	Delay 20
Wend
