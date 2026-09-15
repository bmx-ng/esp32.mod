SuperStrict

Import BRL.StandardIO
Import ESP32.System.Time

Local checksPassed:Int = SystemClockFrequency() > 0
Local beforeMicroseconds:ULong = MonotonicMicroseconds()
Local beforeMilliseconds:ULong = MonotonicMilliseconds()
SleepMilliseconds(5)
checksPassed :& MonotonicMicroseconds() >= beforeMicroseconds + 5000:ULong
checksPassed :& MonotonicMilliseconds() >= beforeMilliseconds + 5:ULong

Local beforeShortSleep:ULong = MonotonicMicroseconds()
SleepMicroseconds(1000)
checksPassed :& MonotonicMicroseconds() >= beforeShortSleep + 1000:ULong

Local oneShot:Int = AlarmAfterMilliseconds(10)
checksPassed :& oneShot <> 0 And AlarmActive(oneShot)
SleepMilliseconds(15)
checksPassed :& Not AlarmActive(oneShot)
checksPassed :& PendingAlarmEvents(oneShot) = 1
checksPassed :& TakeAlarmEvents(oneShot) = 1
checksPassed :& PendingAlarmEvents(oneShot) = 0

Local microOneShot:Int = AlarmAfterMicroseconds(2000)
SleepMilliseconds(4)
checksPassed :& TakeAlarmEvents(microOneShot) = 1

Local repeating:Int = RepeatingAlarmMilliseconds(5)
checksPassed :& repeating <> 0 And AlarmActive(repeating)
SleepMilliseconds(24)
checksPassed :& TakeAlarmEvents(repeating) >= 3
checksPassed :& AlarmActive(repeating)
checksPassed :& CancelAlarm(repeating)
checksPassed :& Not AlarmActive(repeating)

Local microRepeating:Int = RepeatingAlarmMicroseconds(2000)
SleepMilliseconds(7)
checksPassed :& TakeAlarmEvents(microRepeating) >= 2
checksPassed :& CancelAlarm(microRepeating)

Local cancellable:Int = AlarmAfterMilliseconds(1000)
Local remaining:Int = RemainingAlarmMilliseconds(cancellable)
Local remainingMicroseconds:Long = RemainingAlarmMicroseconds(cancellable)
checksPassed :& cancellable <> 0 And remaining >= 0 And remaining <= 1000
checksPassed :& remainingMicroseconds >= 0 And remainingMicroseconds <= 1000000
checksPassed :& CancelAlarm(cancellable)
checksPassed :& Not AlarmActive(cancellable) And Not CancelAlarm(cancellable)
checksPassed :& AlarmAfterMilliseconds(0) = 0
checksPassed :& AlarmAfterMicroseconds(0) = 0
checksPassed :& RepeatingAlarmMilliseconds(0) = 0
checksPassed :& RepeatingAlarmMicroseconds(0) = 0

Local capacityAlarms:Int[8]
For Local index:Int = 0 Until capacityAlarms.length
	capacityAlarms[index] = AlarmAfterMilliseconds(1000)
	checksPassed :& capacityAlarms[index] <> 0
Next
checksPassed :& AlarmAfterMilliseconds(1000) = 0
For Local handle:Int = EachIn capacityAlarms
	checksPassed :& CancelAlarm(handle)
Next
Local reused:Int = AlarmAfterMilliseconds(1000)
checksPassed :& reused <> 0 And CancelAlarm(reused)

If checksPassed Then
	Print "ESP32 monotonic time and native alarm checks passed"
Else
	RuntimeError "ESP32 monotonic time or native alarm check failed"
End If
