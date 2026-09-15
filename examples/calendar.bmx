SuperStrict

Import BRL.StandardIO
Import ESP32.System.Calendar
Import ESP32.System.Time

Local initial:SDateTime = New SDateTime(2026, 9, 13, 12, 34, 56, 125, True)
If Not CalendarStart(initial) Then RuntimeError "ESP32 calendar start failed"

Local current:SDateTime
If Not CalendarGet(current) Then RuntimeError "ESP32 calendar read failed"
If current.year <> 2026 Or current.month <> 9 Or current.day <> 13 Then RuntimeError "ESP32 calendar date mismatch"
If CurrentUnixTime() = 0 Then RuntimeError "Pub.Time did not observe ESP32 system time"

Local alarmTime:SDateTime = SDateTime.FromEpoch(current.ToEpochSecs() + 1)
If Not CalendarSetAlarm(alarmTime) Then RuntimeError "ESP32 calendar alarm setup failed"
Local deadline:ULong = MonotonicMilliseconds() + 2000
While PendingCalendarAlarmEvents() = 0 And MonotonicMilliseconds() < deadline
	Delay 10
Wend
If TakeCalendarAlarmEvents() <> 1 Then RuntimeError "ESP32 calendar alarm did not fire"

Print "ESP32 calendar/system-clock checks passed"
