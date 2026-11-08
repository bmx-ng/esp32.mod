' Resolve a Thread service and verify a 900-byte UDP exchange.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread
Import Embedded.Network.ThreadDiscovery

If ThreadStartSaved() <> 0 Then RuntimeError "No saved Thread mesh"
If ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread attach failed"
If ThreadServiceClientEnable() <> 0 Then RuntimeError "SRP client failed"
Local started:UInt = MilliSecs()
While Not ThreadServiceClientRunning() And MilliSecs() - started < 30000
	Delay 50
Wend
If Not ThreadServiceClientRunning() Then RuntimeError "SRP server not found"
If ThreadServiceResolve("bmx-large", "_bmxlarge._udp") <> 0 Then RuntimeError "Resolve start failed"
Local address:String, port:Int
started = MilliSecs()
While ThreadServiceResolveState(address, port) = 1 And MilliSecs() - started < 10000
	Delay 50
Wend
If ThreadServiceResolveState(address, port) <> 0 Then RuntimeError "Service not found"
Print "THREAD_LARGE_ENDPOINT=" + address + ":" + port

Local socket:TThreadUDPSocket = TThreadUDPSocket.Open(12356, ThreadUDPNetifThread, 4, 1024)
If Not socket Then RuntimeError "UDP bind failed"
Local payload:Byte[900]
For Local i:Int = 0 Until payload.length
	payload[i] = Byte(i & $ff)
Next
Local reply:Byte[1024]
If socket.Send(address, port, payload) <> payload.length Then RuntimeError "Large UDP send failed"
started = MilliSecs()
While MilliSecs() - started < 15000
	Local source:String, sourcePort:Int
	Local length:Int = socket.Receive(reply, source, sourcePort)
	If length >= 0 Then
		If length <> payload.length Then RuntimeError "Large UDP reply length: " + length
		For Local i:Int = 0 Until length
			If reply[i] <> payload[i] Then RuntimeError "Large UDP reply mismatch at " + i
		Next
		Print "THREAD_LARGE_UDP=pass"
		While True
			Delay 1000
		Wend
	End If
	Delay 20
Wend
RuntimeError "Large UDP reply timeout"
