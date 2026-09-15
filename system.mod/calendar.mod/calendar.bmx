' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: UTC system clock and absolute alarm services for ESP32 targets.
about: The clock is backed by ESP-IDF system time. Calendar alarms publish a
coalesced native event for ordinary BlitzMax code to consume; no managed code
runs in the ESP timer task.
End Rem
Module ESP32.System.Calendar
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Pub.Time

Extern "C"
	Function CalendarStart:Int(dateTime:SDateTime Var) = "bmx_esp32_calendar_start"
	Function CalendarStop() = "bmx_esp32_calendar_stop"
	Function CalendarSet:Int(dateTime:SDateTime Var) = "bmx_esp32_calendar_set"
	Function CalendarGet:Int(dateTime:SDateTime Var) = "bmx_esp32_calendar_get"
	Function CalendarIsRunning:Int() = "bmx_esp32_calendar_is_running"
	Function CalendarResolutionNanoseconds:ULong() = "bmx_esp32_calendar_resolution_nanoseconds"
	Function CalendarSetAlarm:Int(dateTime:SDateTime Var, wakeFromLowPower:Int = False) = "bmx_esp32_calendar_set_alarm"
	Function CalendarDisableAlarm() = "bmx_esp32_calendar_disable_alarm"
	Function PendingCalendarAlarmEvents:UInt() = "bmx_esp32_calendar_pending_alarm_events"
	Function TakeCalendarAlarmEvents:UInt() = "bmx_esp32_calendar_take_alarm_events"
End Extern
?
