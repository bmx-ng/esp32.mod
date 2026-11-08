' Standalone mesh service publisher and UDP echo node (S31 server build).
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread
Import Embedded.Network.ThreadServer

Local result:Int = ThreadStartSaved()
If result = ThreadErrorNoSavedNetwork Then result = ThreadForm("BMXThread", 15)
If result <> 0 Then RuntimeError "Thread network start failed: " + result
If ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread attach failed"

result = ThreadServiceServerEnable()
If result <> 0 Then RuntimeError "SRP server enable failed: " + result
Local started:UInt = MilliSecs()
While ThreadServiceServerState() <> 1 And MilliSecs() - started < 30000
	Delay 50
Wend
If ThreadServiceServerState() <> 1 Then RuntimeError "SRP server did not start"
Print "THREAD_SRP_SERVER=running"

Local socket:TThreadUDPSocket = TThreadUDPSocket.Open(12345)
If Not socket Then RuntimeError "Thread UDP bind failed"
result = ThreadServicePublish("bmx-echo-host", "bmx-echo", "_bmxecho._udp", 12345)
If result <> 0 Then RuntimeError "Service publish failed: " + result

Local packet:Byte[512]
Local reported:Int
While True
	If Not reported And ThreadServicePublishState() = 0 Then
		Print "THREAD_SERVICE=registered"
		reported = True
	End If
	Local source:String, port:Int
	Local length:Int = socket.Receive(packet, source, port)
	If length >= 0 Then
		Print "THREAD_RX=" + length
		Print "THREAD_ECHO=" + socket.Send(source, port, packet, length)
	End If
	Delay 20
Wend
