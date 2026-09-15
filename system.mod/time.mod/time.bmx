' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Monotonic time, deadlines, and sleep operations for ESP32 targets.
about: Alarm callbacks execute only native bookkeeping in the ESP timer task.
BlitzMax code observes coalesced alarm events by polling PendingAlarmEvents or
TakeAlarmEvents from the managed application task.
End Rem
Module ESP32.System.Time
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Extern "C"
	Rem
	bbdoc: Returns the current processor-clock frequency in hertz.
	End Rem
	Function SystemClockFrequency:UInt() = "bmx_esp32_system_clock_hz"

	Function MonotonicMicroseconds:ULong() = "bmx_embedded_time_microseconds"
	Function MonotonicMilliseconds:ULong() = "bmx_embedded_time_milliseconds"
	Function SleepMilliseconds(milliseconds:UInt) = "bmx_embedded_sleep_milliseconds"
	Function SleepMicroseconds(microseconds:ULong) = "bmx_embedded_sleep_microseconds"

	Rem
	bbdoc: Creates a one-shot alarm and returns its generation-checked handle.
	about: A zero result means that the duration was invalid, the call was not
	made from the managed application context, or all eight native alarm slots
	are occupied.
	End Rem
	Function AlarmAfterMilliseconds:Int(milliseconds:UInt) = "bmx_esp32_alarm_after_ms"
	Function AlarmAfterMicroseconds:Int(microseconds:ULong) = "bmx_esp32_alarm_after_us"
	Function RepeatingAlarmMilliseconds:Int(milliseconds:UInt) = "bmx_esp32_repeating_alarm_ms"
	Function RepeatingAlarmMicroseconds:Int(microseconds:ULong) = "bmx_esp32_repeating_alarm_us"
	Function CancelAlarm:Int(handle:Int) = "bmx_esp32_alarm_cancel"
	Function AlarmActive:Int(handle:Int) = "bmx_esp32_alarm_active"
	Function PendingAlarmEvents:UInt(handle:Int) = "bmx_esp32_alarm_pending_events"

	Rem
	bbdoc: Returns and clears the coalesced event count for an alarm.
	about: Consuming a fired one-shot alarm also releases its native slot.
	End Rem
	Function TakeAlarmEvents:UInt(handle:Int) = "bmx_esp32_alarm_take_events"
	Function RemainingAlarmMicroseconds:Long(handle:Int) = "bmx_esp32_alarm_remaining_us"
	Function RemainingAlarmMilliseconds:Int(handle:Int) = "bmx_esp32_alarm_remaining_ms"
End Extern
?
