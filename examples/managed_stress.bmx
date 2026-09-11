SuperStrict

Framework BRL.StandardIO
Import Embedded.Runtime.Memory
Import ESP32.Runtime

Type TPayload
	Field identifier:Int
	Field label:String
End Type

Type TNode Extends TPayload
	Field nextNode:TNode
	Field children:TNode[]
	Field samples:Int[]
End Type

Function NewNode:TNode(identifier:Int)
	Local node:TNode = New TNode
	node.identifier = identifier
	node.label = "node-" + identifier
	node.samples = New Int[8 + identifier Mod 17]
	node.samples[0] = identifier
	node.samples[node.samples.Length - 1] = identifier * 3
	Return node
End Function

Function AllocateCycle(seed:Int)
	Local first:TNode = NewNode(seed)
	Local second:TNode = NewNode(seed + 1)
	Local third:TNode = NewNode(seed + 2)
	first.nextNode = second
	second.nextNode = third
	third.nextNode = first
	first.children = [second, third]
	second.children = [first]
End Function

Local capacity:UInt = ArenaCapacity()
If capacity = 0 Or ManagedArenaReserved() <> capacity Or RuntimeCore() <> 0 Or ..
	Not ManagedArenaValid() Then
	RuntimeError "ESP32 managed arena reservation failed"
End If

Local retained:TNode = NewNode(42)
retained.nextNode = NewNode(84)
retained.children = [retained.nextNode]

Local automaticBefore:UInt = AutomaticCollectionCount()
For Local index:Int = 0 Until 1200
	AllocateCycle(index * 3)
Next

If AutomaticCollectionCount() <= automaticBefore Then
	RuntimeError "ESP32 automatic collection did not run"
End If

Local reclaimedTotal:UInt
For Local cycle:Int = 0 Until 80
	For Local index:Int = 0 Until 12
		AllocateCycle(10000 + cycle * 36 + index * 3)
	Next
	reclaimedTotal :+ CollectObjects()
	If retained.identifier <> 42 Or retained.label <> "node-42" Or ..
		retained.samples[0] <> 42 Or retained.samples[retained.samples.Length - 1] <> 126 Or ..
		retained.nextNode.identifier <> 84 Or retained.children[0] <> retained.nextNode Or ..
		InvalidReferenceCount() <> 0 Then
		RuntimeError "ESP32 retained graph was corrupted"
	End If
Next

ReachabilityAudit()
If reclaimedTotal = 0 Or CollectionCount() < 80 Or HeapReusableBytes() = 0 Or ..
	InvalidReferenceCount() <> 0 Or ObjectFailureCount() <> 0 Or ..
	ArrayFailureCount() <> 0 Or StringFailureCount() <> 0 Or ArenaFailureCount() <> 0 Then
	RuntimeError "ESP32 managed runtime stress test failed"
End If

Print "ESP32 managed runtime stress test passed"
