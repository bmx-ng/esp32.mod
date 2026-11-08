' Build only: bmk makeapp -a -r -board esp32_h2_supermini -heap 64k thread_udp_peer.bmx
' Paste the active dataset and mesh IPv6 address printed by nrf52_thread_echo.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread

Const ActiveDatasetHex:String = "<paste-active-dataset-hex>"
Const EchoIPv6:String = "<paste-mesh-ipv6-address>"

If ActiveDatasetHex[0] = Asc("<") Then
	RuntimeError "Set ActiveDatasetHex and EchoIPv6 before flashing"
End If
Local result:Int = ThreadJoin(ActiveDatasetHex)
If result <> 0 Then RuntimeError "ThreadJoin failed: " + result
Local role:Int = ThreadWaitForAttached()
If role < ThreadRoleChild Then RuntimeError "Thread attach failed: " + role
If ThreadUDPOpen(12346) <> 0 Then RuntimeError "Thread UDP bind failed"
Print "THREAD_ROLE=" + role
For Local index:Int = 0 To 7
	Local address:String = ThreadIPv6Address(index)
	If address = "" Then Exit
	Print "THREAD_IPV6=" + address
Next

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
