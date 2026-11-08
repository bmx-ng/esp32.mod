' A UDP peer for a board already provisioned by Joiner commissioning.
' Set EchoIPv6 to the mesh address printed by thread_form_echo.bmx.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread

Const EchoIPv6:String = "<paste-mesh-ipv6-address>"

If EchoIPv6[0] = Asc("<") Then RuntimeError "Set EchoIPv6 before flashing"
Local result:Int = ThreadStartSaved()
If result <> 0 Then RuntimeError "ThreadStartSaved failed: " + result
Print "THREAD_NETWORK=resumed"
Local role:Int = ThreadWaitForAttached()
If role < ThreadRoleChild Then RuntimeError "Thread attach failed: " + role
If ThreadUDPOpen(12346) <> 0 Then RuntimeError "Thread UDP bind failed"
Print "THREAD_ROLE=" + role

Local payload:Byte[] = [Byte(Asc("P")), Byte(Asc("I")), Byte(Asc("N")), Byte(Asc("G"))]
Local reply:Byte[512]
While True
	Local sent:Int = ThreadUDPSend(EchoIPv6, 12345, payload)
	Print "THREAD_TX=" + sent
	Local started:UInt = MilliSecs()
	While MilliSecs() - started < 3000
		Local source:String, port:Int
		Local length:Int = ThreadUDPReceive(reply, source, port)
		If length = 4 And reply[0] = Asc("P") And reply[1] = Asc("I") And ..
				reply[2] = Asc("N") And reply[3] = Asc("G") Then
			Print "THREAD_ECHO=pass from " + source
			Exit
		End If
		Delay 20
	Wend
	Delay 1000
Wend
