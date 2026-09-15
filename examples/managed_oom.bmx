SuperStrict

Framework BRL.StandardIO
Import Embedded.Runtime.Memory
Import ESP32.Runtime
Import "managed_oom_probe.c"

Extern "C"
	Function ManagedOOMProbe:Int() = "bmx_esp32_managed_oom_probe"
	Function LanguageOOMBegin:Int() = "bmx_esp32_language_oom_begin"
	Function LanguageOOMEnd() = "bmx_esp32_language_oom_end"
End Extern

Type TOOMGuard
	Field identifier:Int
	Field label:String
	Field nextGuard:TOOMGuard
	Field samples:Int[]
End Type

Global finalizerStarted:Int
Global finalizerCompleted:Int
Global finalizerFirst:TOOMFinalizer
Global finalizerSecond:TOOMFinalizer
Global localCatchFinalizer:TLocalCatchFinalizer
Global localCatchCompleted:Int

Type TOOMFinalizer
	Field identifier:Int

	Method Delete()
		finalizerStarted :+ 1
		Local values:Int[] = New Int[64]
		values[0] = identifier
		finalizerCompleted :+ 1
	End Method
End Type

Type TLocalCatchFinalizer
	Method Delete()
		Try
			Throw "finalizer-local-catch"
		Catch message:String
			If message = "finalizer-local-catch" Then localCatchCompleted :+ 1
		End Try
	End Method
End Type

Function CatchObjectAllocationFailure:Int()
	If Not LanguageOOMBegin() Then Return False
	Local caught:String
	Try
		Local value:TOOMGuard = New TOOMGuard
	Catch message:String
		caught = message
	End Try
	LanguageOOMEnd()
	Return caught = "BlitzMax Object allocation failed"
End Function

Function CatchArrayAllocationFailure:Int()
	If Not LanguageOOMBegin() Then Return False
	Local caught:String
	Try
		Local values:Int[] = New Int[64]
	Catch message:String
		caught = message
	End Try
	LanguageOOMEnd()
	Return caught = "BlitzMax Array allocation failed"
End Function

Function CatchStringAllocationFailure:Int()
	If Not LanguageOOMBegin() Then Return False
	Local caught:String
	Local prefix:String = "managed"
	Try
		Local value:String = prefix + "-allocation"
	Catch message:String
		caught = message
	End Try
	LanguageOOMEnd()
	Return caught = "BlitzMax String allocation failed"
End Function

Function CatchFinalizerAllocationFailure:Int()
	finalizerStarted = 0
	finalizerCompleted = 0
	finalizerFirst = New TOOMFinalizer
	finalizerFirst.identifier = 11
	finalizerSecond = New TOOMFinalizer
	finalizerSecond.identifier = 22
	Local invocationsBefore:UInt = FinalizerInvocationCount()

	If Not LanguageOOMBegin() Then
		finalizerFirst = Null
		finalizerSecond = Null
		Return False
	End If
	finalizerFirst = Null
	finalizerSecond = Null

	Local caught:String
	Try
		CollectObjects()
	Catch message:String
		caught = message
	End Try
	Local pendingAfterThrow:UInt = FinalizerPendingCount()
	LanguageOOMEnd()

	Local resumedCollection:UInt = CollectObjects()
	Local resumedFinalizers:UInt = LastFinalizedObjectCount()
	Local reclaimed:UInt = CollectObjects()
	Return caught = "BlitzMax Array allocation failed" And pendingAfterThrow = 0 And ..
		resumedCollection = 0 And resumedFinalizers = 1 And reclaimed = 2 And ..
		finalizerStarted = 2 And finalizerCompleted = 1 And ..
		FinalizerInvocationCount() = invocationsBefore + 2 And ..
		FinalizerPendingCount() = 0 And HeapIntegrityValid()
End Function

Function CheckLocalFinalizerCatch:Int()
	localCatchCompleted = 0
	localCatchFinalizer = New TLocalCatchFinalizer
	Local invocationsBefore:UInt = FinalizerInvocationCount()
	localCatchFinalizer = Null
	Local finalized:UInt = CollectObjects()
	Local finalizedThisCycle:UInt = LastFinalizedObjectCount()
	Local reclaimed:UInt = CollectObjects()
	Return finalized = 0 And finalizedThisCycle = 1 And reclaimed = 1 And ..
		localCatchCompleted = 1 And FinalizerInvocationCount() = invocationsBefore + 1 And ..
		FinalizerPendingCount() = 0 And ExceptionDepth() = 0 And HeapIntegrityValid()
End Function

Local retained:TOOMGuard = New TOOMGuard
retained.identifier = 73
retained.label = "retained-through-oom"
retained.samples = [3, 5, 8, 13, 21]
retained.nextGuard = New TOOMGuard
retained.nextGuard.identifier = 144

Local failuresBefore:UInt = ArenaFailureCount()
Local automaticBefore:UInt = AutomaticCollectionCount()

If Not ManagedOOMProbe() Then
	RuntimeError "ESP32 managed allocation failure probe failed"
End If

If ArenaFailureCount() <> failuresBefore + 2 Or ..
	AutomaticCollectionCount() <= automaticBefore Then
	RuntimeError "ESP32 managed allocator failure accounting failed"
End If

If Not CatchObjectAllocationFailure() Or Not CatchArrayAllocationFailure() Or ..
	Not CatchStringAllocationFailure() Or Not CatchFinalizerAllocationFailure() Then
	RuntimeError "ESP32 language allocation failure semantics failed"
End If

If Not CheckLocalFinalizerCatch() Then
	RuntimeError "ESP32 local finalizer exception handling failed"
End If

If retained.identifier <> 73 Or retained.label <> "retained-through-oom" Or ..
	retained.samples.Length <> 5 Or retained.samples[4] <> 21 Or ..
	retained.nextGuard.identifier <> 144 Then
	RuntimeError "ESP32 managed roots were not retained during allocation failure"
End If

Local recovered:TOOMGuard = New TOOMGuard
recovered.identifier = 233
recovered.label = retained.label + "-recovered"
recovered.samples = New Int[32]
recovered.samples[31] = recovered.identifier

If recovered.identifier <> 233 Or recovered.label <> "retained-through-oom-recovered" Or ..
	recovered.samples[31] <> 233 Or Not HeapIntegrityValid() Or ..
	InvalidReferenceCount() <> 0 Or ManagedContextViolationCount() <> 0 Then
	RuntimeError "ESP32 managed allocator did not recover after exhaustion"
End If

Print "ESP32 managed allocation failure test passed"
