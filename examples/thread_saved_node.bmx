' Resume a network saved by Thread Joiner commissioning or dataset provisioning.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread

Local result:Int = ThreadStartSaved()
If result <> 0 Then RuntimeError "ThreadStartSaved failed: " + result
Print "THREAD_NETWORK=resumed"
Local role:Int = ThreadWaitForAttached()
If role < ThreadRoleChild Then RuntimeError "Thread attach failed: " + role
Print "THREAD_ROLE=" + role
For Local index:Int = 0 To 7
	Local address:String = ThreadIPv6Address(index)
	If address = "" Then Exit
	Print "THREAD_IPV6=" + address
Next
While True
	Delay 1000
Wend
