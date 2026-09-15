' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Device identity, chip information, and reset services for ESP32 targets.
End Rem
Module ESP32.System.Device
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const ChipFeatureEmbeddedFlash:UInt = 1 Shl 0
Const ChipFeatureWiFi:UInt = 1 Shl 1
Const ChipFeatureBLE:UInt = 1 Shl 4
Const ChipFeatureBluetoothClassic:UInt = 1 Shl 5
Const ChipFeatureIEEE802154:UInt = 1 Shl 6
Const ChipFeatureEmbeddedPSRAM:UInt = 1 Shl 7

Const DeviceResetReasonUnknown:Int = 0
Const DeviceResetReasonPowerOn:Int = 1
Const DeviceResetReasonExternal:Int = 2
Const DeviceResetReasonSoftware:Int = 3
Const DeviceResetReasonWatchdog:Int = 4
Const DeviceResetReasonPanic:Int = 5
Const DeviceResetReasonDeepSleep:Int = 6
Const DeviceResetReasonBrownout:Int = 7
Const DeviceResetReasonPowerGlitch:Int = 8
Const DeviceResetReasonCPULockup:Int = 9

Extern "C"
	Function UniqueDeviceID:String() = "bmx_embedded_unique_device_id"
	Function UniqueDeviceIDBytes:Byte[]() = "bmx_embedded_unique_device_id_bytes"
	Function DeviceResetReason:Int() = "bmx_embedded_device_reset_reason"
	Function DeviceResetReasonNative:Int() = "bmx_esp32_device_reset_reason_native"
	Function Reboot:Int(delayMilliseconds:UInt = 0) = "bmx_embedded_device_reboot"

	Function ChipModel:Int() = "bmx_esp32_device_chip_model"
	Function ChipModelName:String() = "bmx_esp32_device_chip_model_name"
	Function ChipRevision:UInt() = "bmx_esp32_device_chip_revision"
	Function ChipCoreCount:UInt() = "bmx_esp32_device_chip_cores"
	Function ChipFeatures:UInt() = "bmx_esp32_device_chip_features"
End Extern

Function DeviceResetReasonName:String(reason:Int)
	Select reason
		Case DeviceResetReasonPowerOn Return "Power on"
		Case DeviceResetReasonExternal Return "External"
		Case DeviceResetReasonSoftware Return "Software"
		Case DeviceResetReasonWatchdog Return "Watchdog"
		Case DeviceResetReasonPanic Return "Panic"
		Case DeviceResetReasonDeepSleep Return "Deep sleep"
		Case DeviceResetReasonBrownout Return "Brownout"
		Case DeviceResetReasonPowerGlitch Return "Power glitch"
		Case DeviceResetReasonCPULockup Return "CPU lockup"
	End Select
	Return "Unknown"
End Function
?
