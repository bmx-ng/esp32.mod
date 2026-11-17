SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

For Local run:Int = 1 To 2
	Local result:Int = ZigbeeStart(ZigbeeRoleCoordinator, ZigbeeDeviceOnOffLight, 10, ..
		1 Shl 15, 0, False)
	If result Then RuntimeError "Zigbee start rejected: " + result
	Print "LIFECYCLE start accepted run=" + run
	Local started:Int
	Local startup:Int
	Local deadline:UInt = MilliSecs() + 20000
	Local event:SZigbeeEvent
	While MilliSecs() < deadline And Not (started And startup)
		While ZigbeePollEvent(event)
			Print "LIFECYCLE event=" + event.kind + " status=" + event.status
			If event.kind = ZigbeeEventStarted Then started = True
			If event.kind = ZigbeeEventStartup And event.status = 0 Then startup = True
			If event.kind = ZigbeeEventError Then RuntimeError "Zigbee startup error: " + event.status
		Wend
		Delay 20
	Wend
	If Not (started And startup) Then RuntimeError "Zigbee startup timed out"
	result = ZigbeeStop()
	If result Then RuntimeError "Zigbee stop rejected: " + result
	Print "LIFECYCLE stop accepted run=" + run
	Local stopped:Int
	deadline = MilliSecs() + 10000
	While MilliSecs() < deadline And Not stopped
		While ZigbeePollEvent(event)
			Print "LIFECYCLE event=" + event.kind + " status=" + event.status
			If event.kind = ZigbeeEventStopped Then stopped = True
		Wend
		Delay 20
	Wend
	If Not stopped Or ZigbeeState() <> ZigbeeStateStopped Then RuntimeError "Zigbee stop timed out"
Next
Print "LIFECYCLE PASS: two start/stop cycles"
While True
	Delay 1000
Wend
