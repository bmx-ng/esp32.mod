' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Low-power sleep and wake services for ESP32 targets.
about: Returning operations use ESP-IDF light sleep. Deep sleep restarts the
application and is exposed separately because it cannot implement the portable
returning dormant-sleep contract.
End Rem
Module ESP32.System.Power
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const PowerCapabilityIdle:UInt = $01
Const PowerCapabilitySleepInterrupt:UInt = $02
Const PowerCapabilitySleepTimer:UInt = $04
Const PowerCapabilityDormantGPIO:UInt = $08
Const PowerCapabilityDormantTimer:UInt = $10
Const PowerCapabilityLowLeakagePins:UInt = $20
Const PowerCapabilitySleepGPIORetention:UInt = $40
Const PowerCapabilitySleepGPIOWake:UInt = $80

Enum EPowerResult:Int
	Success = 0
	Error = -1
	Timeout = -2
	NotPermitted = -4
	InvalidArgument = -5
	InsufficientResources = -9
	InvalidState = -12
	PreconditionNotMet = -14
	InvalidData = -16
	Unavailable = -17
	ResourceInUse = -21
End Enum

Extern "C"
	Function PowerCapabilities:UInt() = "bmx_embedded_power_capabilities"
	Function PowerIdle() = "bmx_embedded_power_idle"
	Function LowPowerSleepUntilInterrupt:EPowerResult() = "bmx_embedded_power_sleep_until_interrupt"
	Function LowPowerSleep:EPowerResult(milliseconds:UInt, exclusive:Int = True) = "bmx_embedded_power_sleep_for_ms"
	Function DormantSleep:EPowerResult(milliseconds:UInt) = "bmx_embedded_power_dormant_for_ms"
	Function DormantSleepUntilGPIO:EPowerResult(pin:UInt, edge:Int, high:Int) = "bmx_embedded_power_dormant_until_gpio"
	Function SetUnusedPinsLowLeakage:EPowerResult(excludeMask:ULong = 0) = "bmx_embedded_power_set_unused_pins_low_leakage"

	Rem
	bbdoc: Enables or disables retention of a GPIO's current output state during returning light sleep.
	param: Output-capable GPIO number whose configured state is to be retained.
	param: True to retain the GPIO during subsequent sleeps; False to stop retaining it.
	returns: Success or an error describing an invalid GPIO or state.
	about: The GPIO remains usable while awake. It must be configured as an output
	when retention is enabled and whenever light sleep begins. ESP-IDF pad hold is
	applied only while #LowPowerSleep or #LowPowerSleepUntilInterrupt is active.
	This policy does not apply to deep sleep.
	End Rem
	Function SetSleepGPIORetention:EPowerResult(pin:UInt, enabled:Int = True) = "bmx_embedded_power_set_sleep_gpio_retention"

	Rem
	bbdoc: Enables or disables a GPIO level as a wake source for subsequent returning light sleeps.
	param: GPIO number to configure as a wake source.
	param: True to wake while the pin is high; False to wake while it is low.
	param: True to enable this wake source; False to remove it.
	returns: Success or an error describing an invalid GPIO or conflicting wake source.
	about: This persistent policy applies to #LowPowerSleep and
	#LowPowerSleepUntilInterrupt. Multiple GPIO wake sources may be enabled. The
	requested level must not already be active when sleep begins.
	End Rem
	Function SetSleepGPIOWake:EPowerResult(pin:UInt, high:Int, enabled:Int = True) = "bmx_embedded_power_set_sleep_gpio_wake"

	Rem
	bbdoc: Enters deep sleep and restarts after approximately milliseconds.
	about: A successful call does not return. DeviceResetReason reports deep sleep
	on the next boot. Zero is rejected to prevent an accidental indefinite sleep.
	End Rem
	Function DeepSleepForMilliseconds:EPowerResult(milliseconds:ULong) = "bmx_esp32_power_deep_sleep_for_ms"

	Rem
	bbdoc: Enters deep sleep and restarts when an RTC-capable GPIO reaches the requested level.
	param: RTC-capable GPIO number to use as the wake source.
	param: True to wake while the pin is high; False to wake while it is low.
	returns: An error if the pin or current state is unsuitable; a successful call does not return.
	about: The pin is configured as an EXT1 deep-sleep GPIO wake source.
	The requested level must not already be active, which prevents an immediate
	reboot loop. #PowerWakeGPIOs identifies the waking pin after restart.
	End Rem
	Function DeepSleepUntilGPIO:EPowerResult(pin:UInt, high:Int) = "bmx_esp32_power_deep_sleep_until_gpio"
	Rem
	bbdoc: Returns ESP-IDF's bitmap of wake sources from the most recent light or deep sleep.
	End Rem
	Function PowerWakeCauses:UInt() = "bmx_esp32_power_wake_causes"

	Rem
	bbdoc: Returns a GPIO-number bit mask identifying pins responsible for the most recent GPIO wake.
	returns: A mask with bit n set for GPIOn, or zero when the last wake was not GPIO-driven.
	about: Returning light sleep records all configured wake pins whose requested
	level was active on resume. Following an EXT1 deep-sleep restart, ESP-IDF's
	latched EXT1 status is returned.
	End Rem
	Function PowerWakeGPIOs:ULong() = "bmx_esp32_power_wake_gpios"
End Extern

Function PowerSupports:Int(capabilities:UInt)
	Return (PowerCapabilities() & capabilities) = capabilities
End Function
?
