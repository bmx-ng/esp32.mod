' Serve a CoAP resource over a Thread DTLS connection.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread
Import Embedded.Network.ThreadCoAPSecure

' Replace this example key on both peers before flashing. Supply real keys at runtime.
Local psk:Byte[16]
If psk[0] = 0 Then RuntimeError "Set a private PSK before flashing"

Local result:Int = ThreadStartSaved()
If result = ThreadErrorNoSavedNetwork Then result = ThreadForm("BMXThread", 15)
If result <> 0 Or ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread start failed"
If ThreadCoAPSecureStart(psk, "example-peer") <> 0 Then RuntimeError "Secure CoAP start failed"
If ThreadCoAPSecureResource("secure") <> 0 Then RuntimeError "Secure resource failed"
For Local index:Int = 0 To 7
	Local address:String = ThreadIPv6Address(index)
	If address = "" Then Exit
	Print "THREAD_ADDRESS=" + address
Next
Print "THREAD_COAP_SECURE=ready"
Local payload:Byte[1024], requestMethod:Int, address:String
While True
	Local length:Int = ThreadCoAPSecureReceive(payload, requestMethod, address)
	If length >= 0 Then
		Print "THREAD_COAP_SECURE_RX=" + length
		Local reply:Byte[] = [Byte(Asc("O")), Byte(Asc("K"))]
		Print "THREAD_COAP_SECURE_RESPOND=" + ThreadCoAPSecureRespond(ThreadCoAPContent, reply)
	End If
	Delay 20
Wend
