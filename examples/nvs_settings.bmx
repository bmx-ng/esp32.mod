' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Import BRL.StandardIO
Import ESP32.Storage.NVS

Function Check:Int(result:Int, operation:String)
	If result <> 0 Then
		RuntimeError operation + " failed: " + NVSResultName(result) + " (" + result + ")"
	End If
	Return result
End Function

Local settings:TNVSNamespace = OpenNVS("app_settings")
If Not settings Then
	RuntimeError "Open failed: " + NVSResultName(NVSLastOpenError())
End If

Local brightness:Int
Local deviceName:String
Local changed:Int

Local result:Int = settings.GetInt("brightness", brightness)
If result = NVSErrorNotFound Then
	brightness = 80
	Check settings.SetInt("brightness", brightness), "Store default brightness"
	changed = True
Else
	Check result, "Read brightness"
End If

If Not settings.TryGetString("device_name", deviceName) Then
	If settings.LastError() <> NVSErrorNotFound Then
		RuntimeError "Read device name failed: " + NVSResultName(settings.LastError())
	End If
	deviceName = "Küche sensor"
	Check settings.SetString("device_name", deviceName), "Store default device name"
	changed = True
End If

If changed Then Check settings.Commit(), "Commit defaults"

Print "Device: " + deviceName
Print "Brightness: " + brightness
Print "Defaults written: " + changed

settings.Close()
