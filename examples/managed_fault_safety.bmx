SuperStrict

Framework BRL.StandardIO
Import Embedded.Runtime.Memory
Import ESP32.Runtime
Import "managed_fault_safety_probe.c"

Extern "C"
	Function ManagedFaultSafetyProbe:Int(value:Object, rootToken:UInt) = ..
		"bmx_esp32_managed_fault_safety_probe"
End Extern

Type TFaultAnchor
	Field identifier:Int
	Field label:String
	Field samples:Int[]
End Type

Local anchor:TFaultAnchor = New TFaultAnchor
anchor.identifier = 73
anchor.label = "native-root-anchor"
anchor.samples = [3, 5, 8, 13, 21]

Local rootsBefore:UInt = ObjectRootCount()
Local token:UInt = ObjectRootRetain(anchor)
If token = 0 Or ObjectRootCount() <> rootsBefore + 1 Then
	RuntimeError "ESP32 owner-task root retention failed"
End If

If Not ManagedFaultSafetyProbe(anchor, token) Then
	RuntimeError "ESP32 managed fault-safety boundary failed"
End If

anchor = Null
CollectObjects()
If ObjectRootCount() <> rootsBefore + 1 Then
	' The rejected foreign-task release must leave the native root intact.
	RuntimeError "ESP32 rejected native root was modified"
End If

ObjectRootRelease(token)
If ObjectRootCount() <> rootsBefore Or CollectObjects() = 0 Or ..
	Not HeapIntegrityValid() Or InvalidReferenceCount() <> 0 Then
	RuntimeError "ESP32 native root release or collection failed"
End If

Print "ESP32 managed fault-safety matrix passed"
