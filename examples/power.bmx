SuperStrict

Import BRL.StandardIO
Import BRL.FileSystem
Import BRL.TextStream
Import ESP32.System.Power
Import ESP32.System.Calendar
Import ESP32.System.Device
Import ESP32.System.Time
Import ESP32.Storage.LittleFS

Const WakeMarker:String = "littlefs::power-wake-v2.marker"

' USB Serial/JTAG cannot remain connected through light sleep on ESP32-S3.
' A marker plus a software reboot makes the post-wake result observable on it.
If FileType(WakeMarker) = FILETYPE_FILE
	Local resultText:String = LoadText(WakeMarker)
	DeleteFile WakeMarker
	Local separator:Int = resultText.Find(",")
	If separator < 1 Then RuntimeError "ESP32 light sleep did not return before reset"
	Local result:Int = resultText[..separator].ToInt()
	Local elapsed:ULong = ULong(resultText[separator + 1..].ToLong())
	If result <> EPowerResult.Success Then RuntimeError "ESP32 timed light sleep failed"
	If elapsed < 90 Or elapsed > 1000 Then
		RuntimeError "ESP32 timed light sleep duration was outside tolerance"
	End If
	While True
		Print "ESP32 light-sleep checks passed (timed/calendar=" + resultText[separator + 1..] + " ms)"
		Delay 1000
	Wend
End If

If Not PowerSupports(PowerCapabilityIdle | PowerCapabilitySleepTimer) Then
	RuntimeError "ESP32 power capabilities are incomplete"
End If

If LowPowerSleep(0) <> EPowerResult.InvalidArgument Then
	RuntimeError "ESP32 zero-duration sleep was not rejected"
End If

SaveText "starting", WakeMarker
Local started:ULong = MonotonicMilliseconds()
Local sleepResult:EPowerResult = LowPowerSleep(100)
Local elapsed:ULong = MonotonicMilliseconds() - started
If sleepResult <> EPowerResult.Success Or elapsed < 90 Or elapsed > 1000 Then
	RuntimeError "ESP32 timed light sleep failed"
End If

Local initial:SDateTime = New SDateTime(2026, 9, 13, 12, 34, 56, 0, True)
If Not CalendarStart(initial) Then RuntimeError "ESP32 calendar setup failed"
Local current:SDateTime
If Not CalendarGet(current) Then RuntimeError "ESP32 calendar read failed"
Local alarmTime:SDateTime = SDateTime.FromEpoch(current.ToEpochSecs() + 1)
If Not CalendarSetAlarm(alarmTime, True) Then RuntimeError "ESP32 calendar wake alarm setup failed"
started = MonotonicMilliseconds()
If LowPowerSleepUntilInterrupt() <> EPowerResult.Success Then
	RuntimeError "ESP32 calendar-driven light sleep failed"
End If
Local calendarElapsed:ULong = MonotonicMilliseconds() - started
Local deadline:ULong = MonotonicMilliseconds() + 100
While PendingCalendarAlarmEvents() = 0 And MonotonicMilliseconds() < deadline
	Delay 1
Wend
If calendarElapsed < 900 Or calendarElapsed > 1500 Or TakeCalendarAlarmEvents() <> 1 Then
	RuntimeError "ESP32 calendar wake alarm did not wake light sleep"
End If

SaveText Int(sleepResult) + "," + elapsed + "," + calendarElapsed, WakeMarker

If DormantSleep(1) <> EPowerResult.Unavailable Then
	RuntimeError "ESP32 portable dormant sleep should report unavailable"
End If

Reboot 100
RuntimeError "ESP32 software reboot unexpectedly returned"
