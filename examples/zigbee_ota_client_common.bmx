Function RunZigbeeOtaClient(version:UInt)
	If ZigbeeAddOtaClient(1, $1234, 1, version) Then RuntimeError "OTA client cluster failed"
	Local result:Int = ZigbeeStart(ZigbeeRoleEndDevice, ZigbeeDeviceOnOffSwitch, 1)
	If result Then RuntimeError "Zigbee start failed: " + result
	Print "OTA client version=" + version
	Local event:SZigbeeEvent
	Local joined:Int
	Local downloading:Int
	Local nextFind:UInt = MilliSecs() + 2000
	While True
		While ZigbeePollEvent(event)
			Print "OTA event kind=" + event.kind + " signal=" + event.signal + ..
				" status=" + event.status + " value=" + event.value + ..
				" total=" + event.attribute + " endpoint=" + event.endpoint + ..
				" cluster=" + event.cluster
			If event.kind = ZigbeeEventJoined And event.status = 0 Then joined = True
			If event.kind = ZigbeeEventStartup And event.status = 0 And Not event.factoryNew Then joined = True
			If event.kind = ZigbeeEventStartup And event.status = 0 Then
				Local imageState:UInt
				If OTARunningImageState(imageState) = 0 And imageState = OTAImageStatePendingVerify Then
					result = OTAMarkRunningImageValid()
					Print "OTA mark running valid result=" + result
				End If
			End If
			If event.kind = ZigbeeEventMatch And event.cluster = ZigbeeClusterOtaUpgrade Then
				result = ZigbeeOtaQuery(1, event.shortAddress, event.endpoint)
				Print "OTA query submission=" + result
			End If
			If event.kind = ZigbeeEventOta And event.signal = ZigbeeOtaStart And event.status = 0 Then downloading = True
			If event.kind = ZigbeeEventOta And event.signal = ZigbeeOtaAbort Then downloading = False
		Wend
		If joined And Not downloading And MilliSecs() >= nextFind Then
			result = ZigbeeFindServers(ZigbeeClusterOtaUpgrade)
			Print "Find OTA server submission=" + result
			nextFind = MilliSecs() + 15000
		End If
		Delay 20
	Wend
End Function
