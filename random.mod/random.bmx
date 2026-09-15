' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP32 hardware random-number generator.
End Rem
Module ESP32.Random
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Random

Rem
bbdoc: Compatibility name for the shared device-backed random generator.
about: ESP-IDF obtains values from the SoC hardware RNG. Continuous true-random
output requires an active RF subsystem or internal entropy source. Instances
cannot be seeded or serialized.
End Rem
Type TESP32Random Extends TEmbeddedRandom
	Method GetName:String() Override
		Return "ESP32"
	End Method
End Type

Private
Type TESP32RandomFactory Extends TRandomFactory
	Method New()
		Super.New()
		Init()
	End Method

	Method GetName:String() Override
		Return "ESP32"
	End Method

	Method Create:TRandom(seed:Int) Override
		Return New TESP32Random
	End Method

	Method Create:TRandom() Override
		Return New TESP32Random
	End Method

	Method DeserializeState:TRandom(data:String) Override
		Return Null
	End Method
End Type
Public

Function ESP32RandomUInt:UInt()
	Return EmbeddedRandomUInt()
End Function

Function ESP32RandomULong:ULong()
	Return EmbeddedRandomULong()
End Function

Function ESP32FillRandom:Int(buffer:Byte Ptr, length:Int)
	Return EmbeddedFillRandom(buffer, length)
End Function

Extern "C"
	Rem
	bbdoc: Enables the ESP32 internal hardware entropy source.
	about: This source uses the SAR ADC. Disable it before using ADC, I2S on the
	original ESP32, Wi-Fi, or Bluetooth, and do not enable it while those
	subsystems are active.
	End Rem
	Function ESP32RandomEnableInternalEntropy() = "bmx_esp32_random_enable_internal_entropy"

	Rem
	bbdoc: Disables the internal entropy source enabled by ESP32RandomEnableInternalEntropy.
	End Rem
	Function ESP32RandomDisableInternalEntropy() = "bmx_esp32_random_disable_internal_entropy"

	Rem
	bbdoc: Reports whether this module explicitly enabled the internal entropy source.
	about: This does not report entropy supplied independently by Wi-Fi or Bluetooth.
	End Rem
	Function ESP32RandomInternalEntropyEnabled:Int() = "bmx_esp32_random_internal_entropy_enabled"
End Extern

New TESP32RandomFactory
?
