' Use an ephemeral UDP port and a connected peer on the saved Thread mesh.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread

Const EchoIPv6:String = "<paste-mesh-ipv6-address>"
If EchoIPv6[0] = Asc("<") Then RuntimeError "Set EchoIPv6 before flashing"
If ThreadStartSaved() <> 0 Or ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread start failed"

Local socket:TThreadUDPSocket = TThreadUDPSocket.Open(0)
If Not socket Or socket.LocalPort() < 1 Then RuntimeError "Ephemeral UDP bind failed"
Print "THREAD_UDP_LOCAL_PORT=" + socket.LocalPort()
If socket.Connect(EchoIPv6, 12345) <> 0 Then RuntimeError "UDP connect failed"
Local payload:Byte[] = [Byte(Asc("P")), Byte(Asc("I")), Byte(Asc("N")), Byte(Asc("G"))]
If socket.SendConnected(payload) <> payload.length Then RuntimeError "Connected UDP send failed"
Local reply:Byte[1280], source:String, port:Int
Local started:UInt = MilliSecs()
Local received:Int
While MilliSecs() - started < 10000
	Local length:Int = socket.Receive(reply, source, port)
	If length >= 0 Then
		If length <> payload.length Or source <> EchoIPv6 Or port <> 12345 Then RuntimeError "Connected UDP peer mismatch"
		For Local i:Int = 0 Until length
			If reply[i] <> payload[i] Then RuntimeError "Connected UDP payload mismatch"
		Next
		Print "THREAD_UDP_CONNECTED=pass"
		received = True
		Exit
	End If
	Delay 20
Wend
If Not received Then RuntimeError "Connected UDP timeout"
If socket.Disconnect() <> 0 Then RuntimeError "UDP disconnect failed"
socket.Close()
While True
	Delay 1000
Wend
