' One-connection TCP echo server on a saved Thread mesh.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread
Import Embedded.Network.ThreadTCP

Local result:Int = ThreadStartSaved()
If result = ThreadErrorNoSavedNetwork Then result = ThreadForm("BMXThread", 15)
If result <> 0 Or ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread start failed"
For Local index:Int = 0 To 7
	Local address:String = ThreadIPv6Address(index)
	If address = "" Then Exit
	Print "THREAD_ADDRESS=" + address
Next
If ThreadTCPOpen() <> 0 Or ThreadTCPListen(12360) <> 0 Then RuntimeError "TCP listen failed"
Print "THREAD_TCP_LISTEN=ready"
Local packet:Byte[1024]
While True
	Local length:Int = ThreadTCPReceive(packet)
	If length > 0 Then
		Print "THREAD_TCP_RX=" + length
		While Not ThreadTCPSendReady()
			Delay 20
		Wend
		Print "THREAD_TCP_ECHO=" + ThreadTCPSend(packet, length)
	End If
	Delay 20
Wend
