' Edit the unique PSKd before flashing this example on an unprovisioned node.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread
Import Embedded.Network.ThreadCommissioning

Const JoinerPskd:String = "<unique-pskd>"

If JoinerPskd[0] = Asc("<") Then RuntimeError "Set JoinerPskd"
Print "THREAD_FACTORY_EUI64=" + ThreadFactoryEui64()
Local result:Int = ThreadJoinerStart(JoinerPskd)
If result <> 0 Then RuntimeError "ThreadJoinerStart failed: " + result
Local started:UInt = MilliSecs()
While True
	result = ThreadJoinerStatus()
	If result = 0 Then Exit
	If result <> ThreadJoinerPending Then RuntimeError "Thread commissioning failed: " + result
	If MilliSecs() - started > 120000 Then RuntimeError "Thread commissioning timed out"
	Delay 20
Wend
Local role:Int = ThreadWaitForAttached()
If role < ThreadRoleChild Then RuntimeError "Thread attach failed: " + role
Print "THREAD_COMMISSIONED=attached"
Print "THREAD_ROLE=" + role
While True
	Delay 1000
Wend
