' Edit the EUI-64 and unique PSKd before flashing this example on an attached node.
SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Thread
Import Embedded.Network.Thread

Const JoinerEui64:String = "<factory-eui64>"
Const JoinerPskd:String = "<unique-pskd>"

If JoinerEui64[0] = Asc("<") Then RuntimeError "Set JoinerEui64 and JoinerPskd"
Local result:Int = ThreadStartSaved()
If result <> 0 Then RuntimeError "ThreadStartSaved failed: " + result
If ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread attach failed"
result = ThreadCommissionerStart()
If result <> 0 Then RuntimeError "ThreadCommissionerStart failed: " + result
Local started:UInt = MilliSecs()
While ThreadCommissionerState() <> ThreadCommissionerActive
	If MilliSecs() - started > 30000 Then RuntimeError "Commissioner petition timed out"
	Delay 20
Wend
result = ThreadCommissionerAddJoiner(JoinerEui64, JoinerPskd, 120)
If result <> 0 Then RuntimeError "ThreadCommissionerAddJoiner failed: " + result
Print "THREAD_COMMISSIONER=active"
While True
	Delay 1000
Wend
