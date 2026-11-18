SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

Incbin "assets/zigbee_ota_update.ota"

If ZigbeeAddOtaServer(10) Then RuntimeError "OTA server cluster failed"
Local result:Int = ZigbeeStart(ZigbeeRoleCoordinator, ZigbeeDeviceOnOffLight, 10)
If result Then RuntimeError "Zigbee start failed: " + result
Print "OTA server starting"

Local event:SZigbeeEvent
Local imageReady:Int
While True
	While ZigbeePollEvent(event)
		Print "OTA event kind=" + event.kind + " signal=" + event.signal + ..
			" status=" + event.status + " value=" + event.value + ..
			" total=" + event.attribute + " endpoint=" + event.endpoint
		If event.kind = ZigbeeEventStarted And event.status = 0 Then
			result = ZigbeeOtaServeImage(10, IncbinPtr("assets/zigbee_ota_update.ota"), ..
				UInt(IncbinLen("assets/zigbee_ota_update.ota")))
			Print "OTA serve image result=" + result
			imageReady = result = 0
		End If
		If event.kind = ZigbeeEventFormed Or ..
			(event.kind = ZigbeeEventStartup And event.status = 0 And Not event.factoryNew) Then
			result = ZigbeePermitJoining(180)
			Print "Permit joining result=" + result
		End If
		If event.kind = ZigbeeEventDeviceAnnounced And imageReady Then
			result = ZigbeeOtaNotify(10, event.shortAddress, 1, $1234, 1, 2)
			Print "OTA image notify result=" + result
		End If
	Wend
	Delay 20
Wend
