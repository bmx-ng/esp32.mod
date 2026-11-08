' A bounded 900-byte Thread UDP echo service for testing fragmented datagrams.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread
Import Embedded.Network.ThreadServer
Import Embedded.Network.ThreadDiscovery

Local result:Int = ThreadStartSaved()
If result = ThreadErrorNoSavedNetwork Then result = ThreadForm("BMXThread", 15)
If result <> 0 Then RuntimeError "Thread start failed: " + result
If ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread attach failed"
If ThreadServiceServerEnable() <> 0 Then RuntimeError "SRP server failed"
Local started:UInt = MilliSecs()
While ThreadServiceServerState() <> 1 And MilliSecs() - started < 30000
	Delay 50
Wend
If ThreadServiceServerState() <> 1 Then RuntimeError "SRP server not running"

Local socket:TThreadUDPSocket = TThreadUDPSocket.Open(12355, ThreadUDPNetifThread, 4, 1024)
If Not socket Then RuntimeError "UDP bind failed"
If ThreadServicePublish("bmx-large-host", "bmx-large", "_bmxlarge._udp", 12355) <> 0 Then RuntimeError "SRP publish failed"
started = MilliSecs()
While ThreadServicePublishState() = 1 And MilliSecs() - started < 30000
	Delay 50
Wend
If ThreadServicePublishState() <> 0 Then RuntimeError "SRP registration failed"
Print "THREAD_LARGE_UDP=ready"

Local packet:Byte[1024]
While True
	Local source:String, port:Int
	Local length:Int = socket.Receive(packet, source, port)
	If length >= 0 Then
		Print "THREAD_LARGE_RX=" + length
		Print "THREAD_LARGE_ECHO=" + socket.Send(source, port, packet, length)
	End If
	Delay 20
Wend
