' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

' Destructive regression test: this deliberately reboots once, then erases its
' bmx_nvs_test namespace after verifying that every value persisted correctly.

Import BRL.StandardIO
Import ESP32.Runtime
Import ESP32.Storage.NVS
Import ESP32.System.Device

Const TestNamespace:String = "bmx_nvs_test"

Function RequireSuccess(result:Int, operation:String)
	If result <> 0 Then RuntimeError operation + " failed: " + NVSResultName(result) + " (" + result + ")"
End Function

Local store:TNVSNamespace = OpenNVS(TestNamespace)
If Not store Then RuntimeError "Could not open NVS: " + NVSResultName(NVSLastOpenError())

Local phase:Int
Local phaseResult:Int = store.GetInt("phase", phase)
If phaseResult = NVSErrorNotFound
	RequireSuccess store.SetInt8("int8", -128), "SetInt8"
	RequireSuccess store.SetUInt8("uint8", 255), "SetUInt8"
	RequireSuccess store.SetInt16("int16", -32768), "SetInt16"
	RequireSuccess store.SetUInt16("uint16", 65535), "SetUInt16"
	RequireSuccess store.SetInt("signed", -1234567), "SetInt"
	RequireSuccess store.SetUInt("unsigned", $f1234567:UInt), "SetUInt"
	RequireSuccess store.SetLong("long", -5000000000:Long), "SetLong"
	RequireSuccess store.SetULong("ulong", 9000000000000000000:ULong), "SetULong"
	RequireSuccess store.SetFloat("float", 1.25), "SetFloat"
	RequireSuccess store.SetDouble("double", -9876.125), "SetDouble"
	RequireSuccess store.SetString("string", "BlitzMax ESP32"), "SetString"
	RequireSuccess store.SetBytes("bytes", [Byte(1), Byte(2), Byte(3), Byte(255)]), "SetBytes"
	Local rawSource:Byte Ptr = MemAlloc(5)
	If Not rawSource Then RuntimeError "Could not allocate raw NVS source memory"
	For Local index:Int = 0 Until 5
		rawSource[index] = Byte(index * 17 + 3)
	Next
	RequireSuccess store.SetBytes("raw", rawSource, 5), "Set raw bytes"
	MemFree rawSource
	RequireSuccess store.SetInt("phase", 1), "Set phase"
	RequireSuccess store.Commit(), "Commit"
	store.Close()
	Print "NVS values committed; rebooting to verify persistence"
	Delay 100
	Reboot(25)
	RuntimeError "NVS persistence reboot returned unexpectedly"
Else If phaseResult <> 0
	RuntimeError "Get phase failed: " + NVSResultName(phaseResult)
Else If phase <> 1
	RuntimeError "Unexpected NVS test phase"
End If

Local signedValue:Int
Local int8Value:Int
Local uint8Value:Int
Local int16Value:Int
Local uint16Value:Int
Local unsignedValue:UInt
Local longValue:Long
Local ulongValue:ULong
Local floatValue:Float
Local doubleValue:Double
Local stringValue:String
Local bytesValue:Byte[]

RequireSuccess store.GetInt8("int8", int8Value), "GetInt8"
RequireSuccess store.GetUInt8("uint8", uint8Value), "GetUInt8"
RequireSuccess store.GetInt16("int16", int16Value), "GetInt16"
RequireSuccess store.GetUInt16("uint16", uint16Value), "GetUInt16"
RequireSuccess store.GetInt("signed", signedValue), "GetInt"
RequireSuccess store.GetUInt("unsigned", unsignedValue), "GetUInt"
RequireSuccess store.GetLong("long", longValue), "GetLong"
RequireSuccess store.GetULong("ulong", ulongValue), "GetULong"
RequireSuccess store.GetFloat("float", floatValue), "GetFloat"
RequireSuccess store.GetDouble("double", doubleValue), "GetDouble"
If Not store.TryGetString("string", stringValue) Then RuntimeError "GetString failed: " + NVSResultName(store.LastError())
If Not store.TryGetBytes("bytes", bytesValue) Then RuntimeError "GetBytes failed: " + NVSResultName(store.LastError())

If int8Value <> -128 Then RuntimeError "Int8 NVS value differs"
If uint8Value <> 255 Then RuntimeError "UInt8 NVS value differs"
If int16Value <> -32768 Then RuntimeError "Int16 NVS value differs"
If uint16Value <> 65535 Then RuntimeError "UInt16 NVS value differs"
If signedValue <> -1234567 Then RuntimeError "Signed NVS value differs"
If unsignedValue <> $f1234567:UInt Then RuntimeError "Unsigned NVS value differs"
If longValue <> -5000000000:Long Then RuntimeError "Long NVS value differs"
If ulongValue <> 9000000000000000000:ULong Then RuntimeError "ULong NVS value differs"
If floatValue < 1.249 Or floatValue > 1.251 Then RuntimeError "Float NVS value differs"
If doubleValue < -9876.126 Or doubleValue > -9876.124 Then RuntimeError "Double NVS value differs"
If stringValue <> "BlitzMax ESP32" Then RuntimeError "String NVS value differs"
If bytesValue.length <> 4 Or bytesValue[0] <> 1 Or bytesValue[1] <> 2 Or bytesValue[2] <> 3 Or bytesValue[3] <> 255 Then
	RuntimeError "Blob NVS value differs"
End If
Local rawDestination:Byte Ptr = MemAlloc(5)
If Not rawDestination Then RuntimeError "Could not allocate raw NVS destination memory"
Local rawSize:UInt
If store.GetBytes("raw", rawDestination, 4, rawSize) <> NVSErrorInvalidLength Or rawSize <> 5 Then
	RuntimeError "Raw NVS capacity guard differs"
End If
RequireSuccess store.GetBytes("raw", rawDestination, 5, rawSize), "Get raw bytes"
If rawSize <> 5 Then RuntimeError "Raw NVS value size differs"
For Local index:Int = 0 Until 5
	If rawDestination[index] <> Byte(index * 17 + 3) Then RuntimeError "Raw NVS value differs"
Next
MemFree rawDestination
If store.ValueType("signed") <> NVSTypeInt32 Then RuntimeError "NVS type discovery differs"

Local usedEntries:UInt
RequireSuccess store.UsedEntryCount(usedEntries), "Read namespace entry count"
If usedEntries < 15 Then RuntimeError "NVS namespace entry count is unexpectedly small"

Local iterator:TNVSEntryIterator = store.OpenIterator()
If Not iterator Then RuntimeError "Could not enumerate NVS entries: " + NVSResultName(store.LastError())
Local iterated:Int
Local foundInt8:Int
Local iteratorResult:Int
While True
	iterated :+ 1
	If iterator.Key() = "int8" And iterator.ValueType() = NVSTypeInt8 Then foundInt8 = True
	iteratorResult = iterator.Next()
	If iteratorResult = NVSErrorNotFound Then Exit
	RequireSuccess iteratorResult, "Advance NVS iterator"
Wend
iterator.Close()
If iterated < 14 Or Not foundInt8 Then RuntimeError "NVS entry enumeration differs"

store.Close()

Local reader:TNVSNamespace = OpenNVS(TestNamespace, True)
If Not reader Then RuntimeError "Could not reopen NVS read-only: " + NVSResultName(NVSLastOpenError())
If reader.SetInt("forbidden", 1) <> NVSErrorReadOnly Then RuntimeError "Read-only NVS accepted a write"
reader.Close()

store = OpenNVS(TestNamespace)
If Not store Then RuntimeError "Could not reopen NVS for cleanup: " + NVSResultName(NVSLastOpenError())
RequireSuccess store.EraseAll(), "Erase test namespace"
RequireSuccess store.Commit(), "Commit test cleanup"
store.Close()

Local statistics:SNVSStatistics
RequireSuccess NVSStatistics(statistics), "Read NVS statistics"
If statistics.availableEntries > statistics.freeEntries Then RuntimeError "NVS available entry count differs"
Print "NVS persistence test passed; " + statistics.usedEntries + " / " + statistics.totalEntries + " entries used"

Local missing:TNVSNamespace = OpenNVSPartition("missing_nvs", TestNamespace)
If missing Or NVSLastOpenError() <> NVSErrorPartitionNotFound Then
	RuntimeError "Missing NVS partition handling differs"
End If
Local missingStatistics:SNVSStatistics
If NVSStatistics(missingStatistics, "missing_nvs") <> NVSErrorPartitionNotFound Then
	RuntimeError "Missing NVS partition statistics handling differs"
End If

RequireSuccess NVSDeinitialize(), "Deinitialize default NVS partition"
RequireSuccess NVSInitialize(), "Reinitialize default NVS partition"

While True
	Delay 2000
	Print "NVS persistence test alive"
Wend
