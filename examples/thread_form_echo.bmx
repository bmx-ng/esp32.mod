' Build and flash on an ESP32-H2, C6, or S31 board with an IEEE 802.15.4 radio.
' Copy THREAD_DATASET and THREAD_IPV6 into thread_udp_peer.bmx on a second board.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread

Local result:Int = ThreadForm("BMXThread", 15)
If result <> 0 Then RuntimeError "ThreadForm failed: " + result
Print "THREAD_DATASET=" + ThreadActiveDataset()
Local role:Int = ThreadWaitForAttached()
If role < ThreadRoleChild Then RuntimeError "Thread attach failed: " + role
Print "THREAD_ROLE=" + role
For Local index:Int = 0 To 7
	Local address:String = ThreadIPv6Address(index)
	If address = "" Then Exit
	Print "THREAD_IPV6=" + address
Next
If ThreadUDPOpen(12345) <> 0 Then RuntimeError "Thread UDP bind failed"

Local packet:Byte[512]
While True
	Local source:String, port:Int
	Local length:Int = ThreadUDPReceive(packet, source, port)
	If length >= 0 Then
		Print "THREAD_RX=" + length + " from " + source + ":" + port
		Local sent:Int = ThreadUDPSend(source, port, packet, length)
		Print "THREAD_ECHO=" + sent
	End If
	Delay 20
Wend
