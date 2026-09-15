' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Import BRL.StandardIO
Import BRL.System
Import ESP32.Hardware.Watchdog

If WatchdogCausedReboot() Then
	Delay 1000
	Print "ESP32 watchdog reset check passed"
Else
	Print "Waiting for the watchdog to reset the device"
	If Not WatchdogEnable(250) Then RuntimeError "Unable to enable the ESP32 watchdog"
	Delay 2000
	RuntimeError "ESP32 watchdog did not reset the device"
End If
