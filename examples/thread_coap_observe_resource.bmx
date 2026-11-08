' Publish a changing one-byte CoAP Observe resource on the saved Thread mesh.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread
Import Embedded.Network.ThreadCoAPObserve

Local result:Int = ThreadStartSaved()
If result = ThreadErrorNoSavedNetwork Then result = ThreadForm("BMXThread", 15)
If result <> 0 Or ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread start failed"
If ThreadCoAPStart() <> 0 Or ThreadCoAPObserveResource("counter") <> 0 Then RuntimeError "Observe setup failed"
For Local index:Int = 0 To 7
	Local address:String = ThreadIPv6Address(index)
	If address = "" Then Exit
	Print "THREAD_ADDRESS=" + address
Next
Print "THREAD_OBSERVE=ready"
Local counter:Int
While True
	If ThreadCoAPObserveHasSubscriber() Then
		Print "THREAD_OBSERVE_PUBLISH=" + ThreadCoAPObservePublish([Byte(counter & $ff)])
		counter :+ 1
	End If
	Delay 2000
Wend
