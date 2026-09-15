' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP32 Bluetooth Low Energy support.
about: The portable lifecycle and discovery API is provided by
Embedded.Network.BLE. ESP32-specific controls build on that shared contract.
End Rem
Module ESP32.Network.BLE
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Network.BLE

Private
Extern "C"
	Function _ESP32BLEResultName:String(result:Int) = "bmx_esp32_ble_result_name"
End Extern
Public

Function BLEResultName:String(result:Int)
	Return _ESP32BLEResultName(result)
End Function
?
