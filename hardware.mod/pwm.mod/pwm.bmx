' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: LED-controller PWM for ESP32 targets.
about: The pin-oriented operations mirror Embedded.Hardware.PWM. Channels are
allocated automatically and pins requesting the same frequency share a timer.
End Rem
Module ESP32.Hardware.PWM
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const PWMDutyMaximum:UInt = 65535

Extern "C"
	Function PWMIsValidPin:Int(pin:UInt) = "bmx_embedded_pwm_is_valid_pin"
	Function PWMInitPin:UInt(pin:UInt, frequency:UInt, duty:UInt = 0, inverted:Int = False) = "bmx_embedded_pwm_init_pin"
	Function PWMDeinitPin:Int(pin:UInt) = "bmx_embedded_pwm_deinit_pin"
	Function PWMSetPinFrequency:UInt(pin:UInt, frequency:UInt) = "bmx_embedded_pwm_set_pin_frequency"
	Function PWMGetPinFrequency:UInt(pin:UInt) = "bmx_embedded_pwm_get_pin_frequency"
	Function PWMSetPinDuty:Int(pin:UInt, duty:UInt) = "bmx_embedded_pwm_set_pin_duty"
	Function PWMGetPinDuty:UInt(pin:UInt) = "bmx_embedded_pwm_get_pin_duty"
	Function PWMSetPinPolarity:Int(pin:UInt, inverted:Int) = "bmx_embedded_pwm_set_pin_polarity"
	Function PWMGetPinPolarity:Int(pin:UInt) = "bmx_embedded_pwm_get_pin_polarity"
	Function PWMSetPinEnabled:Int(pin:UInt, enabled:Int) = "bmx_embedded_pwm_set_pin_enabled"
	Function PWMGetPinEnabled:Int(pin:UInt) = "bmx_embedded_pwm_get_pin_enabled"

	Function PWMChannelForPin:Int(pin:UInt) = "bmx_esp32_pwm_channel_for_pin"
	Function PWMTimerForPin:Int(pin:UInt) = "bmx_esp32_pwm_timer_for_pin"
	Function PWMResolutionBitsForPin:UInt(pin:UInt) = "bmx_esp32_pwm_resolution_bits_for_pin"
End Extern
?
