' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Analogue-to-digital conversion for ESP32 targets.
about: The pin-oriented operations mirror Embedded.Hardware.ADC. ESP32-specific
code can additionally select the input attenuation and inspect ADC routing.
End Rem
Module ESP32.Hardware.ADC
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const ADCAttenuation0dB:Int = 0
Const ADCAttenuation2_5dB:Int = 1
Const ADCAttenuation6dB:Int = 2
Const ADCAttenuation12dB:Int = 3

Extern "C"
	Function ADCIsValidPin:Int(pin:UInt) = "bmx_embedded_adc_is_valid_pin"
	Function ADCInitPin:Int(pin:UInt) = "bmx_embedded_adc_init_pin"
	Function ADCDeinitPin:Int(pin:UInt) = "bmx_embedded_adc_deinit_pin"
	Function ADCReadRaw:Int(pin:UInt, value:UInt Var) = "bmx_embedded_adc_read_raw"
	Function ADCResolutionBitsForPin:UInt(pin:UInt) = "bmx_embedded_adc_resolution_bits"
	Function ADCMaximumValueForPin:UInt(pin:UInt) = "bmx_embedded_adc_maximum_value"

	Function ADCUnitForPin:Int(pin:UInt) = "bmx_esp32_adc_unit_for_pin"
	Function ADCChannelForPin:Int(pin:UInt) = "bmx_esp32_adc_channel_for_pin"
	Function ADCSetAttenuation:Int(pin:UInt, attenuation:Int) = "bmx_esp32_adc_set_attenuation"
	Function ADCGetAttenuation:Int(pin:UInt) = "bmx_esp32_adc_get_attenuation"

	Rem
	bbdoc: Reads a calibrated voltage in millivolts when the SoC supports an ESP-IDF calibration scheme.
	End Rem
	Function ADCReadMilliVolts:Int(pin:UInt, millivolts:Int Var) = "bmx_esp32_adc_read_millivolts"
End Extern
?
