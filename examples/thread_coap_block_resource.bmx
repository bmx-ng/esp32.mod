' Serve a 2048-byte CoAP response using 256-byte Block2 segments.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread
Import Embedded.Network.ThreadCoAPBlock

Local result:Int = ThreadStartSaved()
If result = ThreadErrorNoSavedNetwork Then result = ThreadForm("BMXThread", 15)
If result <> 0 Or ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread start failed"
If ThreadCoAPStart() <> 0 Or ThreadCoAPBlockResource("large") <> 0 Then RuntimeError "CoAP setup failed"
For Local index:Int = 0 To 7
	Local address:String = ThreadIPv6Address(index)
	If address = "" Then Exit
	Print "THREAD_ADDRESS=" + address
Next
Print "THREAD_COAP_BLOCK=ready"
Local request:Byte[4096]
Local response:Byte[2048]
For Local i:Int = 0 Until response.length
	response[i] = Byte(i & $ff)
Next
While True
	Local method:Int, address:String
	Local length:Int = ThreadCoAPBlockReceive(request, method, address)
	If length >= 0 Then
		Print "THREAD_COAP_BLOCK_RX=" + length
		If method = ThreadCoAPGet Then
			Print "THREAD_COAP_BLOCK_RESPOND=" + ThreadCoAPBlockRespond(ThreadCoAPContent, response)
		Else If method = ThreadCoAPPost Then
			If length <> 2048 Then RuntimeError "Block1 request length: " + length
			For Local i:Int = 0 Until length
				If request[i] <> Byte(i & $ff) Then RuntimeError "Block1 mismatch at " + i
			Next
			Print "THREAD_COAP_BLOCK_POST=pass"
			Print "THREAD_COAP_BLOCK_RESPOND=" + ThreadCoAPBlockRespond(ThreadCoAPChanged, [Byte(Asc("O")), Byte(Asc("K"))])
		Else
			Print "THREAD_COAP_BLOCK_RESPOND=" + ThreadCoAPBlockRespond(ThreadCoAPMethodNotAllowed)
		End If
	End If
	Delay 20
Wend
