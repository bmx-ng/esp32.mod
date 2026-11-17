' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP32 general-purpose hardware timers with task-safe alarm delivery.
about: GPTimer counts independently of the application task at a selectable
resolution. Native interrupt code places fixed-size alarm records into a queue;
it never invokes BlitzMax or retains managed objects.
End Rem
Module ESP32.Hardware.GPTimer
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const GPTimerOK:Int = 0
Const GPTimerErrorNoMemory:Int = $101
Const GPTimerErrorInvalidArgument:Int = $102
Const GPTimerErrorInvalidState:Int = $103
Const GPTimerErrorNotFound:Int = $105
Const GPTimerErrorNotSupported:Int = $106
Const GPTimerErrorTimeout:Int = $107

Enum EGPTimerDirection:Int
	Up = 0
	Down = 1
End Enum

Extern "C"
	Function GPTimerSupported:Int() = "bmx_esp32_gptimer_supported"
	Function GPTimerLastCreateError:Int() = "bmx_esp32_gptimer_last_create_error"
	Function _GPTimerNew:Byte Ptr(resolutionHz:UInt, countDown:Int, allowPowerDown:Int, alarmQueueCapacity:UInt) = "bmx_esp32_gptimer_new"
	Function _GPTimerDelete(handle:Byte Ptr) = "bmx_esp32_gptimer_delete"
	Function _GPTimerResolution:UInt(handle:Byte Ptr) = "bmx_esp32_gptimer_resolution"
	Function _GPTimerSetCount:Int(handle:Byte Ptr, value:ULong) = "bmx_esp32_gptimer_set_count"
	Function _GPTimerGetCount:Int(handle:Byte Ptr, value:ULong Var) = "bmx_esp32_gptimer_get_count"
	Function _GPTimerSetAlarm:Int(handle:Byte Ptr, alarmCount:ULong, reloadCount:ULong, autoReload:Int) = "bmx_esp32_gptimer_set_alarm"
	Function _GPTimerDisableAlarm:Int(handle:Byte Ptr) = "bmx_esp32_gptimer_disable_alarm"
	Function _GPTimerStart:Int(handle:Byte Ptr) = "bmx_esp32_gptimer_start"
	Function _GPTimerStop:Int(handle:Byte Ptr) = "bmx_esp32_gptimer_stop"
	Function _GPTimerDisable:Int(handle:Byte Ptr) = "bmx_esp32_gptimer_disable"
	Function _GPTimerWaitAlarm:Int(handle:Byte Ptr, timeoutMilliseconds:Int, countValue:ULong Var, alarmValue:ULong Var) = "bmx_esp32_gptimer_wait_alarm"
	Function _GPTimerDroppedAlarms:UInt(handle:Byte Ptr, clear:Int) = "bmx_esp32_gptimer_dropped_alarms"
	Function _GPTimerClearAlarmQueue(handle:Byte Ptr) = "bmx_esp32_gptimer_clear_alarm_queue"
End Extern

Type TGPTimer
	Field handle:Byte Ptr

	Method IsOpen:Int()
		Return handle <> Null
	End Method

	Rem
	bbdoc: Returns the actual timer resolution in counter ticks per second.
	returns: Timer resolution in hertz, or zero after the timer is closed.
	End Rem
	Method ResolutionHz:UInt()
		Return _GPTimerResolution(handle)
	End Method

	Rem
	bbdoc: Replaces the timer's current raw counter value.
	param: New unsigned counter value.
	returns: GPTimerOK or an ESP-IDF error code.
	End Rem
	Method SetCount:Int(value:ULong)
		Return _GPTimerSetCount(handle, value)
	End Method

	Rem
	bbdoc: Reads the timer's current raw counter value.
	param: Variable receiving the counter value.
	returns: GPTimerOK or an ESP-IDF error code.
	End Rem
	Method ReadCount:Int(value:ULong Var)
		Return _GPTimerGetCount(handle, value)
	End Method

	Rem
	bbdoc: Configures a one-shot or automatically reloading alarm.
	param: Counter value which triggers the alarm.
	param: Counter value restored after an automatically reloading alarm.
	param: True to reload and continue generating alarms.
	returns: GPTimerOK or an ESP-IDF error code.
	about: Configure the counter direction and initial count consistently with
	the alarm and reload values. Alarm records are queued by native interrupt code.
	End Rem
	Method SetAlarm:Int(alarmCount:ULong, reloadCount:ULong = 0, autoReload:Int = False)
		Return _GPTimerSetAlarm(handle, alarmCount, reloadCount, autoReload)
	End Method

	Rem
	bbdoc: Disables generation of timer alarm events.
	returns: GPTimerOK or an ESP-IDF error code.
	End Rem
	Method DisableAlarm:Int()
		Return _GPTimerDisableAlarm(handle)
	End Method

	Rem
	bbdoc: Enables the timer when necessary and starts hardware counting.
	returns: GPTimerOK or an ESP-IDF error code.
	End Rem
	Method Start:Int()
		Return _GPTimerStart(handle)
	End Method

	Rem
	bbdoc: Stops hardware counting while preserving the counter value.
	returns: GPTimerOK or an ESP-IDF error code.
	End Rem
	Method Stop:Int()
		Return _GPTimerStop(handle)
	End Method

	Rem
	bbdoc: Releases the enabled timer state after it has been stopped.
	returns: GPTimerOK or an ESP-IDF error code.
	End Rem
	Method Disable:Int()
		Return _GPTimerDisable(handle)
	End Method

	Rem
	bbdoc: Waits for and removes the oldest queued alarm record.
	param: Maximum wait in milliseconds; zero polls and -1 waits indefinitely.
	param: Variable receiving the captured counter value.
	param: Variable receiving the configured alarm value.
	returns: GPTimerOK, GPTimerErrorTimeout, or another ESP-IDF error code.
	about: Waiting blocks only the calling task. The timer and interrupt continue
	independently. No managed code runs in interrupt context.
	End Rem
	Method WaitAlarm:Int(timeoutMilliseconds:Int, countValue:ULong Var, alarmValue:ULong Var)
		Return _GPTimerWaitAlarm(handle, timeoutMilliseconds, countValue, alarmValue)
	End Method

	Rem
	bbdoc: Polls for and removes the oldest queued alarm record.
	param: Variable receiving the captured counter value.
	param: Variable receiving the configured alarm value.
	returns: GPTimerOK when an alarm was available, or GPTimerErrorTimeout otherwise.
	End Rem
	Method PollAlarm:Int(countValue:ULong Var, alarmValue:ULong Var)
		Return _GPTimerWaitAlarm(handle, 0, countValue, alarmValue)
	End Method

	Rem
	bbdoc: Reports alarm records lost because the native queue was full.
	param: True to atomically reset the dropped count after reading it.
	returns: Number of alarm records dropped since creation or the last clearing read.
	End Rem
	Method DroppedAlarms:UInt(clear:Int = False)
		Return _GPTimerDroppedAlarms(handle, clear)
	End Method

	Rem
	bbdoc: Removes every currently queued alarm record.
	End Rem
	Method ClearAlarmQueue()
		_GPTimerClearAlarmQueue(handle)
	End Method

	Method Close()
		If handle Then
			_GPTimerDelete(handle)
			handle = Null
		End If
	End Method

	Method Delete()
		Close()
	End Method
End Type

Rem
bbdoc: Creates a general-purpose hardware timer.
param: Requested counter resolution in ticks per second.
param: Counter direction.
param: True to permit the timer power domain to be suspended during sleep.
param: Fixed number of alarm records retained before new records are dropped.
returns: A timer, or Null when unsupported or resources cannot be allocated.
about: Native timer and queue memory is allocated during creation. Inspect
#GPTimerLastCreateError when creation fails.
End Rem
Function GPTimerCreate:TGPTimer(resolutionHz:UInt = 1000000, ..
		direction:EGPTimerDirection = EGPTimerDirection.Up, ..
		allowPowerDown:Int = False, alarmQueueCapacity:UInt = 8)
	Local nativeHandle:Byte Ptr = _GPTimerNew(resolutionHz, ..
		direction = EGPTimerDirection.Down, allowPowerDown, alarmQueueCapacity)
	If Not nativeHandle Then Return Null
	Local timer:TGPTimer = New TGPTimer
	timer.handle = nativeHandle
	Return timer
End Function
?
