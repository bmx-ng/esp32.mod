SuperStrict

Framework BRL.StandardIO
Import Embedded.Runtime.Memory
Import ESP32.Runtime

Type TBox
	Field value:Int
	Field text:String
	Field values:Int[]
End Type

Function MakeGarbage(seed:Int)
	Local discarded:TBox = New TBox
	discarded.value = seed
	discarded.text = "temporary-" + seed
	discarded.values = [seed, seed + 1]
End Function

Local box:TBox = New TBox
box.value = 42
box.text = "ESP" + "32"
box.values = [6, 7]

For Local index:Int = 0 Until 32
	MakeGarbage(index)
Next

Local reclaimed:UInt = CollectObjects()
If box.value <> 42 Or box.text <> "ESP32" Or box.values.Length <> 2 Or ..
	box.values[0] * box.values[1] <> 42 Or reclaimed = 0 Or ..
	ArenaCapacity() = 0 Or ManagedArenaReserved() <> ArenaCapacity() Or Not ManagedArenaValid() Or RuntimeCore() <> 0 Or ..
	ObjectFailureCount() <> 0 Or ArrayFailureCount() <> 0 Or StringFailureCount() <> 0 Then
	RuntimeError "ESP32 managed runtime smoke test failed"
End If

Print "ESP32 managed runtime smoke test passed"
