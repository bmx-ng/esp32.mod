' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: LED-controller PWM for ESP32 targets.
about: The pin-oriented operations mirror Embedded.Hardware.PWM. Channels are
allocated automatically and pins requesting the same frequency share a timer.
ESP32-specific operations add hardware-timed fades and light-sleep behaviour.
End Rem
Module ESP32.Hardware.PWM
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const PWMDutyMaximum:UInt = 65535

Rem
bbdoc: Selects how an ESP32 PWM channel behaves during light sleep.
End Rem
Enum EPWMSleepMode:Int
	Invalid = -1
	StopOutput = 0
	StopOutputAndPowerDown = 1
	KeepOutputAlive = 2
End Enum

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

	Rem
	bbdoc: Selects how the PWM output behaves while the ESP32 is in light sleep.
	param: GPIO pin belonging to an initialized PWM channel.
	param: Required light-sleep behaviour.
	returns: True if the channel was reconfigured successfully.
	about: #EPWMSleepMode.KeepOutputAlive keeps the waveform and its timer clock
	running during light sleep, increasing sleep power consumption. The output is
	not retained during deep sleep. #EPWMSleepMode.StopOutputAndPowerDown also
	allows the LEDC power domain to be switched off, but returns False on chips
	without the required register-retention support. Reconfiguration is rejected
	while a hardware fade is active.
	End Rem
	Function PWMSetPinSleepMode:Int(pin:UInt, sleepMode:EPWMSleepMode) = "bmx_esp32_pwm_set_sleep_mode"

	Rem
	bbdoc: Returns the configured light-sleep behaviour for a PWM pin.
	returns: The configured behaviour, or #EPWMSleepMode.Invalid if the pin has no PWM channel.
	End Rem
	Function PWMGetPinSleepMode:EPWMSleepMode(pin:UInt) = "bmx_esp32_pwm_get_sleep_mode"

	Rem
	bbdoc: Starts a hardware-timed transition to a new PWM duty.
	param: GPIO pin belonging to an initialized and enabled PWM channel.
	param: Target duty on the zero through #PWMDutyMaximum scale.
	param: Requested transition duration in milliseconds; must be greater than zero.
	param: True to wait for completion, or False to return after starting the fade.
	returns: True if the fade was started and, when requested, completed.
	about: ESP-IDF may round the requested duration according to the PWM frequency,
	duty resolution, and distance to the target. Other changes to the channel are
	rejected while a non-blocking fade remains active.
	End Rem
	Function PWMFadePinDuty:Int(pin:UInt, duty:UInt, milliseconds:UInt, wait:Int = False) = "bmx_esp32_pwm_fade_pin_duty"

	Rem
	bbdoc: Reports whether a non-blocking hardware fade is active on a PWM pin.
	End Rem
	Function PWMIsPinFading:Int(pin:UInt) = "bmx_esp32_pwm_is_pin_fading"
End Extern
?
