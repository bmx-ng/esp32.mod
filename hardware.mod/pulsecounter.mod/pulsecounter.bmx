' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Hardware pulse and quadrature counting using the ESP32 PCNT peripheral.
about: Pulse counting continues independently of the application task. Creating
a unit or channel may allocate native driver state, but starting, stopping,
clearing, and reading a configured counter do not allocate managed memory.
End Rem
Module ESP32.Hardware.PulseCounter
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const PulseCounterOK:Int = 0
Const PulseCounterErrorNoMemory:Int = $101
Const PulseCounterErrorInvalidArgument:Int = $102
Const PulseCounterErrorInvalidState:Int = $103
Const PulseCounterErrorNotFound:Int = $105
Const PulseCounterErrorNotSupported:Int = $106

Enum EPulseCounterEdgeAction:Int
	Hold = 0
	Increase = 1
	Decrease = 2
End Enum

Enum EPulseCounterLevelAction:Int
	Keep = 0
	Inverse = 1
	Hold = 2
End Enum

Extern "C"
	Function PulseCounterSupported:Int() = "bmx_esp32_pcnt_supported"
	Function PulseCounterLastCreateError:Int() = "bmx_esp32_pcnt_last_create_error"
	Function _PulseCounterNew:Byte Ptr(lowLimit:Int, highLimit:Int, accumulateOverflow:Int, groupId:Int) = "bmx_esp32_pcnt_new"
	Function _PulseCounterDelete(handle:Byte Ptr) = "bmx_esp32_pcnt_delete"
	Function _PulseCounterAddChannel:Int(handle:Byte Ptr, edgePin:Int, levelPin:Int, positiveEdge:Int, negativeEdge:Int, highLevel:Int, lowLevel:Int, invertEdge:Int, invertLevel:Int) = "bmx_esp32_pcnt_add_channel"
	Function _PulseCounterSetGlitchFilter:Int(handle:Byte Ptr, maximumNanoseconds:UInt) = "bmx_esp32_pcnt_set_glitch_filter"
	Function _PulseCounterStart:Int(handle:Byte Ptr) = "bmx_esp32_pcnt_start"
	Function _PulseCounterStop:Int(handle:Byte Ptr) = "bmx_esp32_pcnt_stop"
	Function _PulseCounterClear:Int(handle:Byte Ptr) = "bmx_esp32_pcnt_clear"
	Function _PulseCounterRead:Int(handle:Byte Ptr, value:Int Var) = "bmx_esp32_pcnt_read"
End Extern

Type TPulseCounter
	Field handle:Byte Ptr

	Method IsOpen:Int()
		Return handle <> Null
	End Method

	Rem
	bbdoc: Adds an input channel and defines how each edge affects the count.
	param: GPIO carrying the pulse or encoder edge signal.
	param: Optional GPIO controlling count direction; use -1 for a fixed virtual low level.
	param: Action applied on a rising edge.
	param: Action applied on a falling edge.
	param: Modifier applied while the level input is high.
	param: Modifier applied while the level input is low.
	param: True to invert the edge input before processing it.
	param: True to invert the level input before processing it.
	returns: PulseCounterOK or an ESP-IDF error code.
	about: A simple rising-edge counter uses Increase, Hold, Keep, Keep. A
	quadrature decoder normally adds two channels, with each signal serving as
	the other channel's level input and using Inverse to select direction.
	End Rem
	Method AddChannel:Int(edgePin:Int, levelPin:Int = -1, ..
			positiveEdge:EPulseCounterEdgeAction = EPulseCounterEdgeAction.Increase, ..
			negativeEdge:EPulseCounterEdgeAction = EPulseCounterEdgeAction.Hold, ..
			highLevel:EPulseCounterLevelAction = EPulseCounterLevelAction.Keep, ..
			lowLevel:EPulseCounterLevelAction = EPulseCounterLevelAction.Keep, ..
			invertEdge:Int = False, invertLevel:Int = False)
		Return _PulseCounterAddChannel(handle, edgePin, levelPin, Int(positiveEdge), ..
			Int(negativeEdge), Int(highLevel), Int(lowLevel), invertEdge, invertLevel)
	End Method

	Rem
	bbdoc: Sets the maximum pulse width rejected as an input glitch.
	param: Pulses shorter than this width are ignored; zero disables filtering.
	returns: PulseCounterOK or an ESP-IDF error code.
	about: Configure the filter before the counter is started.
	End Rem
	Method SetGlitchFilter:Int(maximumNanoseconds:UInt)
		Return _PulseCounterSetGlitchFilter(handle, maximumNanoseconds)
	End Method

	Rem
	bbdoc: Enables the unit when necessary and starts hardware counting.
	returns: PulseCounterOK or an ESP-IDF error code.
	End Rem
	Method Start:Int()
		Return _PulseCounterStart(handle)
	End Method

	Rem
	bbdoc: Stops counting without clearing the accumulated value.
	returns: PulseCounterOK or an ESP-IDF error code.
	End Rem
	Method Stop:Int()
		Return _PulseCounterStop(handle)
	End Method

	Rem
	bbdoc: Resets the hardware and overflow-accumulated count to zero.
	returns: PulseCounterOK or an ESP-IDF error code.
	End Rem
	Method Clear:Int()
		Return _PulseCounterClear(handle)
	End Method

	Rem
	bbdoc: Reads the current signed pulse count.
	param: Variable receiving the count when the operation succeeds.
	returns: PulseCounterOK or an ESP-IDF error code.
	End Rem
	Method Read:Int(value:Int Var)
		Return _PulseCounterRead(handle, value)
	End Method

	Method Close()
		If handle Then
			_PulseCounterDelete(handle)
			handle = Null
		End If
	End Method

	Method Delete()
		Close()
	End Method
End Type

Rem
bbdoc: Creates a pulse-counter unit with signed hardware limits.
param: Negative lower hardware limit.
param: Positive upper hardware limit.
param: True to maintain a software-extended count when hardware wraps at a limit.
param: Peripheral group to allocate from; current single-group chips use zero.
returns: A counter, or Null when unsupported or resources cannot be allocated.
about: Add channels and an optional glitch filter before calling Start. Inspect
#PulseCounterLastCreateError when creation fails.
End Rem
Function PulseCounterCreate:TPulseCounter(lowLimit:Int = -32768, highLimit:Int = 32767, ..
		accumulateOverflow:Int = True, groupId:Int = 0)
	Local nativeHandle:Byte Ptr = _PulseCounterNew(lowLimit, highLimit, accumulateOverflow, groupId)
	If Not nativeHandle Then Return Null
	Local counter:TPulseCounter = New TPulseCounter
	counter.handle = nativeHandle
	Return counter
End Function
?
