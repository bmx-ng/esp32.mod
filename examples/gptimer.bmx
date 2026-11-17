SuperStrict

Framework BRL.StandardIO
Import ESP32.Hardware.GPTimer
Import ESP32.System.Time

If Not GPTimerSupported() Then RuntimeError "GPTimer is unavailable on this ESP32 target"

' Native USB targets briefly disconnect while starting newly flashed firmware.
SleepMilliseconds 5000

' One timer tick is one microsecond; generate five alarms 100 ms apart.
Local timer:TGPTimer = GPTimerCreate(1000000, EGPTimerDirection.Up, False, 8)
If Not timer Then RuntimeError "GPTimer creation failed: " + GPTimerLastCreateError()
If timer.ResolutionHz() <> 1000000 Then RuntimeError "unexpected GPTimer resolution"
If timer.SetCount(0) <> GPTimerOK Then RuntimeError "GPTimer count setup failed"
If timer.SetAlarm(100000, 0, True) <> GPTimerOK Then RuntimeError "GPTimer alarm setup failed"
If timer.Start() <> GPTimerOK Then RuntimeError "GPTimer start failed"

For Local index:Int = 1 To 5
	Local countValue:ULong
	Local alarmValue:ULong
	If timer.WaitAlarm(500, countValue, alarmValue) <> GPTimerOK Then
		RuntimeError "timed out waiting for GPTimer alarm"
	End If
	Print "GPTimer alarm " + index + ": count=" + countValue + ..
		", target=" + alarmValue
Next

If timer.Stop() <> GPTimerOK Then RuntimeError "GPTimer stop failed"
If timer.DroppedAlarms() <> 0 Then RuntimeError "GPTimer alarm queue overflowed"
Local finalCount:ULong
If timer.ReadCount(finalCount) <> GPTimerOK Then RuntimeError "GPTimer read failed"
Print "GPTimer final count: " + finalCount
timer.Close()
Print "ESP32 GPTimer test passed"
