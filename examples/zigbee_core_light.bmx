SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

Local result:Int = ZigbeeStart(ZigbeeRoleCoordinator, ZigbeeDeviceOnOffLight, 10)
If result Then RuntimeError "Zigbee start failed: " + result
Print "Zigbee coordinator/light starting"

Local event:SZigbeeEvent
While True
	While ZigbeePollEvent(event)
		Print "Zigbee event " + event.kind + " status=" + event.status + ..
			" pan=" + event.panId + " channel=" + event.channel + " value=" + event.value
		If event.kind = ZigbeeEventStartup And event.status = 0 Then
			result = ZigbeeSetLocalOnOff(10, 0)
			Print "Initial light state result=" + result
		End If
		If event.kind = ZigbeeEventFormed Or ..
			(event.kind = ZigbeeEventStartup And Not event.factoryNew) Then
			result = ZigbeePermitJoining(180)
			Print "Permit joining result=" + result
		End If
	Wend
	Delay 20
Wend
