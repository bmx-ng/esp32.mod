' Simple named CoAP resource on the current Thread mesh.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread
Import Embedded.Network.ThreadCoAP
Import Embedded.Network.ThreadServer

Local result:Int = ThreadStartSaved()
If result = ThreadErrorNoSavedNetwork Then result = ThreadForm("BMXThread", 15)
If result <> 0 Then RuntimeError "Thread start failed"
If ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread attach failed"
If ThreadServiceServerEnable() <> 0 Then RuntimeError "SRP server failed"
Local started:UInt = MilliSecs()
While ThreadServiceServerState() <> 1 And MilliSecs() - started < 30000
	Delay 50
Wend
If ThreadServiceServerState() <> 1 Then RuntimeError "SRP server not running"
If ThreadCoAPStart() <> 0 Or ThreadCoAPResource("status") <> 0 Then RuntimeError "CoAP start failed"
If ThreadCoAPAddResource("health") <> 0 Or ThreadCoAPAddResource("echo") <> 0 Then RuntimeError "CoAP multi-resource setup failed"
Local service:TThreadPublishedService = TThreadPublishedService.Publish("bmx-coap-host", "bmx-coap", "_bmxcoap._udp", 5683, ["ver=1"])
If Not service Then RuntimeError "SRP publish failed"
started = MilliSecs()
While service.State() = 1 And MilliSecs() - started < 30000
	Delay 50
Wend
If service.State() <> 0 Then RuntimeError "SRP registration failed"
Print "THREAD_COAP=ready"

Local request:Byte[512]
While True
	Local requestMethod:Int, source:String
	Local length:Int = ThreadCoAPReceive(request, requestMethod, source)
	If length >= 0 Then
		If requestMethod = ThreadCoAPGet Then
			result = ThreadCoAPRespond(ThreadCoAPContent, [Byte(Asc("O")), Byte(Asc("K"))])
			Print "THREAD_COAP_GET=" + result
		Else If requestMethod = ThreadCoAPPost Then
			Local reply:Byte[] = New Byte[length]
			For Local i:Int = 0 Until length
				reply[i] = request[i]
			Next
			result = ThreadCoAPRespond(ThreadCoAPChanged, reply)
			Print "THREAD_COAP_POST=" + result
		End If
	End If
	Local path:String, requestId:Int
	length = ThreadCoAPReceiveNext(request, requestMethod, source, path, requestId)
	If length >= 0 Then
		If path = "health" And requestMethod = ThreadCoAPGet Then
			result = ThreadCoAPRespondTo(requestId, ThreadCoAPContent, [Byte(Asc("U")), Byte(Asc("P"))])
			Print "THREAD_COAP_HEALTH=" + result
		Else If path = "echo" And requestMethod = ThreadCoAPPost Then
			Local reply:Byte[] = New Byte[length]
			For Local i:Int = 0 Until length
				reply[i] = request[i]
			Next
			result = ThreadCoAPRespondTo(requestId, ThreadCoAPChanged, reply)
			Print "THREAD_COAP_ECHO=" + result
		Else
			result = ThreadCoAPRespondTo(requestId, ThreadCoAPMethodNotAllowed)
			Print "THREAD_COAP_REJECT=" + result
		End If
	End If
	Delay 20
Wend
