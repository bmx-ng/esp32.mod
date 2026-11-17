' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP32 Zigbee v2 SDK bring-up module for ESP32-H2 and ESP32-C6.
about: This initial module only verifies the Espressif Zigbee v2 component in the
BlitzMax build. The public stack and data-model API will follow hardware bring-up.
End Rem
Module ESP32.Network.Zigbee
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Private
Extern "C"
	Function _ZigbeeSdkVersion:Int() = "bmx_esp32_zigbee_sdk_version"
	Function _ZigbeeBringupStart:Int() = "bmx_esp32_zigbee_bringup_start"
	Function _ZigbeeBringupToggle:Int() = "bmx_esp32_zigbee_bringup_toggle"
	Function _ZigbeeBringupRead:Int() = "bmx_esp32_zigbee_bringup_read"
End Extern
Public

Rem
bbdoc: Returns the linked Zigbee SDK major version for bring-up diagnostics.
End Rem
Function ZigbeeSdkMajorVersion:Int()
	Return _ZigbeeSdkVersion()
End Function

Rem
bbdoc: Starts the initial H2 coordinator/light or C6 end-device/switch radio test.
about: The board determines the role. This diagnostic function is temporary while
the configurable public stack API is developed. Returns an ESP-IDF error code.
End Rem
Function ZigbeeBringupStart:Int()
	Return _ZigbeeBringupStart()
End Function

Rem
bbdoc: Sends a bound On/Off Toggle command from the C6 switch test.
about: Returns an ESP-IDF error code; ESP_ERR_INVALID_STATE means that the
switch has not yet joined and bound to the light.
End Rem
Function ZigbeeBringupToggle:Int()
	Return _ZigbeeBringupToggle()
End Function

Rem
bbdoc: Reads the remote light's On/Off attribute from the C6 switch test.
about: The response is reported to the serial log by the Zigbee callback.
Returns an ESP-IDF error code for request submission.
End Rem
Function ZigbeeBringupRead:Int()
	Return _ZigbeeBringupRead()
End Function
?
