' Publish two independently reachable Thread services from one SRP host.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread
Import Embedded.Network.ThreadServer

Local result:Int = ThreadStartSaved()
If result = ThreadErrorNoSavedNetwork Then result = ThreadForm("BMXThread", 15)
If result <> 0 Then RuntimeError "Thread network start failed: " + result
If ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread attach failed"
If ThreadServiceServerEnable() <> 0 Then RuntimeError "SRP server enable failed"
Local started:UInt = MilliSecs()
While ThreadServiceServerState() <> 1 And MilliSecs() - started < 30000
	Delay 50
Wend
If ThreadServiceServerState() <> 1 Then RuntimeError "SRP server did not start"
Print "THREAD_SRP_SERVER=running"

Local firstSocket:TThreadUDPSocket = TThreadUDPSocket.Open(12345)
Local secondSocket:TThreadUDPSocket = TThreadUDPSocket.Open(12347)
If Not firstSocket Or Not secondSocket Then RuntimeError "Thread UDP bind failed"
Local first:TThreadPublishedService = TThreadPublishedService.Publish("bmx-echo-host", "bmx-echo", "_bmxecho._udp", 12345, ["id=one", "ver=2"])
Local second:TThreadPublishedService = TThreadPublishedService.Publish("bmx-echo-host", "bmx-echo-alt", "_bmxecho._udp", 12347, ["id=two", "ver=2"])
If Not first Or Not second Then RuntimeError "Service publish failed"
started = MilliSecs()
While (first.State() = 1 Or second.State() = 1) And MilliSecs() - started < 30000
	Delay 50
Wend
If first.State() <> 0 Or second.State() <> 0 Then RuntimeError "Service registration failed"
Print "THREAD_CATALOG=registered-two"

Local packet:Byte[512]
Local lastChange:UInt = MilliSecs()
Local removingSecond:Int
While True
	If second And Not removingSecond And MilliSecs() - lastChange > 30000 Then
		If second.Unpublish() = 0 Then removingSecond = True
	End If
	If removingSecond And second.State() = 2 Then
		second.Close()
		second = Null
		removingSecond = False
		lastChange = MilliSecs()
		Print "THREAD_CATALOG=removed-second"
	End If
	If Not second And MilliSecs() - lastChange > 12000 Then
		second = TThreadPublishedService.Publish("bmx-echo-host", "bmx-echo-alt", "_bmxecho._udp", 12347, ["id=two", "ver=2"])
		If second Then
			lastChange = MilliSecs()
			Print "THREAD_CATALOG=readded-second"
		End If
	End If
	Local source:String, port:Int
	Local length:Int = firstSocket.Receive(packet, source, port)
	If length >= 0 Then
		Print "THREAD_RX_ONE=" + length
		firstSocket.Send(source, port, packet, length)
	End If
	length = secondSocket.Receive(packet, source, port)
	If length >= 0 Then
		Print "THREAD_RX_TWO=" + length
		secondSocket.Send(source, port, packet, length)
	End If
	Delay 20
Wend
