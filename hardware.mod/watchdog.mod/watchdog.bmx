' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Task watchdog services for ESP32 targets.
about: These operations mirror Embedded.Hardware.Watchdog and subscribe the
calling BlitzMax task to ESP-IDF's task watchdog. Disabling restores the
project's configured task-watchdog timeout.
End Rem
Module ESP32.Hardware.Watchdog
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Extern "C"
	Function WatchdogMaximumDelayMilliseconds:UInt() = "bmx_embedded_watchdog_maximum_delay_ms"
	Function WatchdogEnable:Int(delayMilliseconds:UInt) = "bmx_embedded_watchdog_enable"
	Function WatchdogDisable:Int() = "bmx_embedded_watchdog_disable"
	Function WatchdogFeed:Int() = "bmx_embedded_watchdog_feed"
	Function WatchdogIsEnabled:Int() = "bmx_embedded_watchdog_is_enabled"
	Function WatchdogCausedReboot:Int() = "bmx_embedded_watchdog_caused_reboot"
End Extern
?
