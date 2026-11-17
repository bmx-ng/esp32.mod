SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

Local result:Int = ZigbeeStart(ZigbeeRoleEndDevice, ZigbeeDeviceOnOffSwitch, 1)
If result Then RuntimeError "Zigbee start failed: " + result
Print "Zigbee end-device/switch starting"

Local joined:Int
Local bound:Int
Local lightAddress:Int
Local lightEndpoint:Int
Local nextFind:UInt = MilliSecs() + 1000
Local nextToggle:UInt = MilliSecs() + 10000
Local nextRead:UInt
Local event:SZigbeeEvent
While True
	While ZigbeePollEvent(event)
		Print "Zigbee event " + event.kind + " status=" + event.status + ..
			" pan=" + event.panId + " channel=" + event.channel + " value=" + event.value
		If event.kind = ZigbeeEventJoined And event.status = 0 Then joined = True
		If event.kind = ZigbeeEventStartup And Not event.factoryNew Then joined = True
		If event.kind = ZigbeeEventOnOffMatch Then
			lightAddress = event.shortAddress
			lightEndpoint = event.endpoint
			result = ZigbeeBindOnOff(1, lightAddress, lightEndpoint)
			Print "Bind submission=" + result
		End If
		If event.kind = ZigbeeEventOnOffBound And event.status = 0 Then bound = True
	Wend
	If joined And Not bound And MilliSecs() >= nextFind Then
		result = ZigbeeFindOnOffLights()
		Print "Find light submission=" + result
		nextFind = MilliSecs() + 10000
	End If
	If joined And bound And MilliSecs() >= nextToggle Then
		result = ZigbeeSendBoundOnOff(1, 2)
		Print "Toggle submission=" + result
		nextToggle = MilliSecs() + 10000
		nextRead = MilliSecs() + 500
	End If
	If nextRead And MilliSecs() >= nextRead Then
		result = ZigbeeReadOnOff(1, lightAddress, lightEndpoint)
		Print "Read submission=" + result
		nextRead = 0
	End If
	Delay 20
Wend
