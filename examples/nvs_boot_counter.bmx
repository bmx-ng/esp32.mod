' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Import BRL.StandardIO
Import ESP32.Storage.NVS

Local state:TNVSNamespace = OpenNVS("app_state")
If Not state Then
	RuntimeError "Open failed: " + NVSResultName(NVSLastOpenError())
End If

Local boots:UInt
Local result:Int = state.GetUInt("boot_count", boots)
If result = NVSErrorNotFound Then
	boots = 0
Else If result <> 0 Then
	RuntimeError "Read failed: " + NVSResultName(result)
End If

boots :+ 1
result = state.SetUInt("boot_count", boots)
If result = 0 Then result = state.Commit()
If result <> 0 Then RuntimeError "Write failed: " + NVSResultName(result)

Print "Persistent boot count: " + boots
state.Close()

