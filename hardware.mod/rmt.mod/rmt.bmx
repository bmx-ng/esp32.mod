' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Timed pulse transmission and reception using the ESP32 RMT peripheral.
about: A symbol is two level/duration pairs. Durations are clock ticks at the
channel resolution, from 1 to 32767. Transmit calls finish before returning;
receive completion is polled in application context. Native RMT interrupts do
not enter BlitzMax or retain managed arrays.
End Rem
Module ESP32.Hardware.RMT
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const RMTOK:Int = 0
Const RMTErrorNoMemory:Int = $101
Const RMTErrorInvalidArgument:Int = $102
Const RMTErrorInvalidState:Int = $103
Const RMTErrorInvalidSize:Int = $104
Const RMTErrorNotFound:Int = $105
Const RMTErrorNotSupported:Int = $106
Const RMTMaximumSymbols:UInt = 16384

Extern "C"
	Function RMTSupported:Int() = "bmx_esp32_rmt_supported"
	Function RMTLastCreateError:Int() = "bmx_esp32_rmt_last_create_error"
	Function _RMTNewTX:Byte Ptr(pin:UInt, resolutionHz:UInt, invertOutput:Int) = "bmx_esp32_rmt_new_tx"
	Function _RMTDeleteTX(handle:Byte Ptr) = "bmx_esp32_rmt_delete_tx"
	Function _RMTTXSetCarrier:Int(handle:Byte Ptr, frequencyHz:UInt, dutyPerMille:UInt, activeLow:Int) = "bmx_esp32_rmt_tx_set_carrier"
	Function _RMTTransmitSymbols:Int(handle:Byte Ptr, words:Byte Ptr, count:UInt, endLevel:Int) = "bmx_esp32_rmt_transmit_symbols"
	Function _RMTTransmitBytes:Int(handle:Byte Ptr, bytes:Byte Ptr, count:UInt, zeroSymbol:UInt, oneSymbol:UInt, msbFirst:Int, endLevel:Int) = "bmx_esp32_rmt_transmit_bytes"
	Function _RMTNewRX:Byte Ptr(pin:UInt, resolutionHz:UInt, capacity:UInt, invertInput:Int) = "bmx_esp32_rmt_new_rx"
	Function _RMTDeleteRX(handle:Byte Ptr) = "bmx_esp32_rmt_delete_rx"
	Function _RMTRXSetCarrier:Int(handle:Byte Ptr, frequencyHz:UInt, dutyPerMille:UInt, activeLow:Int) = "bmx_esp32_rmt_rx_set_carrier"
	Function _RMTStartReceive:Int(handle:Byte Ptr, minimumNanoseconds:UInt, maximumNanoseconds:UInt) = "bmx_esp32_rmt_start_receive"
	Function _RMTReceiveReady:Int(handle:Byte Ptr) = "bmx_esp32_rmt_receive_ready"
	Function _RMTReceivedCount:UInt(handle:Byte Ptr) = "bmx_esp32_rmt_received_count"
	Function _RMTReadReceived:Int(handle:Byte Ptr, words:Byte Ptr, capacity:UInt) = "bmx_esp32_rmt_read_received"
End Extern

Rem
bbdoc: Packs two level/duration pairs into an RMT symbol.
about: Returns zero for an invalid duration. Zero is not a transmittable symbol.
End Rem
Function RMTMakeSymbol:UInt(duration0:UInt, level0:Int, duration1:UInt, level1:Int)
	If duration0 = 0 Or duration0 > 32767 Or duration1 = 0 Or duration1 > 32767 Then Return 0
	Return duration0 | (UInt(level0 <> 0) Shl 15) | (duration1 Shl 16) | (UInt(level1 <> 0) Shl 31)
End Function

Function RMTSymbolDuration0:UInt(symbol:UInt)
	Return symbol & $7fff
End Function

Function RMTSymbolLevel0:Int(symbol:UInt)
	Return Int((symbol Shr 15) & 1)
End Function

Function RMTSymbolDuration1:UInt(symbol:UInt)
	Return (symbol Shr 16) & $7fff
End Function

Function RMTSymbolLevel1:Int(symbol:UInt)
	Return Int(symbol Shr 31)
End Function

Type TRMTTransmitter
	Field handle:Byte Ptr

	Method IsOpen:Int()
		Return handle <> Null
	End Method

	Rem
	bbdoc: Sets an optional modulated carrier, useful for infrared transmitters.
	about: frequencyHz=0 disables the carrier. Otherwise dutyPerMille must be
	between 1 and 999. Configure the carrier between transmissions.
	End Rem
	Method SetCarrier:Int(frequencyHz:UInt, dutyPerMille:UInt = 333, activeLow:Int = False)
		Return _RMTTXSetCarrier(handle, frequencyHz, dutyPerMille, activeLow)
	End Method

	Rem
	bbdoc: Synchronously transmits packed symbols, then drives the selected end level.
	End Rem
	Method WriteSymbols:Int(words:Byte Ptr, count:UInt, endLevel:Int = False)
		Return _RMTTransmitSymbols(handle, words, count, endLevel)
	End Method

	Method WriteSymbols:Int(words:UInt[], endLevel:Int = False)
		If Not words Or words.length = 0 Then Return RMTErrorInvalidArgument
		Return _RMTTransmitSymbols(handle, Varptr words[0], UInt(words.length), endLevel)
	End Method

	Rem
	bbdoc: Synchronously encodes each source bit as zeroSymbol or oneSymbol.
	about: This is useful for WS2812, one-wire signalling, and other bit-timed
	protocols. The call accepts caller-owned raw memory as well as Byte arrays.
	End Rem
	Method WriteBytes:Int(data:Byte Ptr, count:UInt, zeroSymbol:UInt, oneSymbol:UInt, msbFirst:Int = True, endLevel:Int = False)
		Return _RMTTransmitBytes(handle, data, count, zeroSymbol, oneSymbol, msbFirst, endLevel)
	End Method

	Method WriteBytes:Int(data:Byte[], zeroSymbol:UInt, oneSymbol:UInt, msbFirst:Int = True, endLevel:Int = False)
		If Not data Or data.length = 0 Then Return RMTErrorInvalidArgument
		Return _RMTTransmitBytes(handle, Varptr data[0], UInt(data.length), zeroSymbol, oneSymbol, msbFirst, endLevel)
	End Method

	Method Close()
		If handle Then
			_RMTDeleteTX(handle)
			handle = Null
		End If
	End Method

	Method Delete()
		Close()
	End Method
End Type

Rem
bbdoc: Creates and enables an RMT transmitter on a GPIO output.
about: The default 10 MHz clock makes each duration tick 100 ns. Returns Null
when the pin, resolution, or hardware resources are unavailable; inspect
RMTLastCreateError for the ESP-IDF result.
End Rem
Function RMTCreateTransmitter:TRMTTransmitter(pin:UInt, resolutionHz:UInt = 10000000, invertOutput:Int = False)
	Local nativeHandle:Byte Ptr = _RMTNewTX(pin, resolutionHz, invertOutput)
	If Not nativeHandle Then Return Null
	Local transmitter:TRMTTransmitter = New TRMTTransmitter
	transmitter.handle = nativeHandle
	Return transmitter
End Function

Type TRMTReceiver
	Field handle:Byte Ptr
	Field capacity:UInt

	Method IsOpen:Int()
		Return handle <> Null
	End Method

	Rem
	bbdoc: Sets optional carrier demodulation before starting a receive job.
	End Rem
	Method SetCarrier:Int(frequencyHz:UInt, dutyPerMille:UInt = 333, activeLow:Int = False)
		Return _RMTRXSetCarrier(handle, frequencyHz, dutyPerMille, activeLow)
	End Method

	Rem
	bbdoc: Starts one bounded receive job without blocking.
	about: Pulses shorter than minimumNanoseconds are filtered; a level longer
	than maximumNanoseconds ends the frame. Read the result before starting another.
	End Rem
	Method Start:Int(minimumNanoseconds:UInt, maximumNanoseconds:UInt)
		Return _RMTStartReceive(handle, minimumNanoseconds, maximumNanoseconds)
	End Method

	Method Ready:Int()
		Return _RMTReceiveReady(handle)
	End Method

	Method ReceivedCount:UInt()
		Return _RMTReceivedCount(handle)
	End Method

	Rem
	bbdoc: Returns completed symbols or Null when reception is not complete.
	about: A completed empty frame returns an empty array. Reading releases the
	native receive slot so another Start call can be made. The final half-symbol
	may have zero duration when it continues into the idle gap without an edge.
	End Rem
	Method ReadSymbols:UInt[]()
		If Not Ready() Then Return Null
		Local count:UInt = ReceivedCount()
		Local words:UInt[] = New UInt[Int(count)]
		Local pointer:Byte Ptr = Null
		If count Then pointer = Varptr words[0]
		If _RMTReadReceived(handle, pointer, count) <> RMTOK Then Return Null
		Return words
	End Method

	Method Close()
		If handle Then
			_RMTDeleteRX(handle)
			handle = Null
		End If
	End Method

	Method Delete()
		Close()
	End Method
End Type

Rem
bbdoc: Creates an RMT receiver with a native, fixed-size symbol buffer.
about: No managed pointer is retained by an interrupt. Capacity is 1..16384
symbols; hardware may stop before filling it. Returns Null on failure.
End Rem
Function RMTCreateReceiver:TRMTReceiver(pin:UInt, capacity:UInt = 256, resolutionHz:UInt = 1000000, invertInput:Int = False)
	Local nativeHandle:Byte Ptr = _RMTNewRX(pin, resolutionHz, capacity, invertInput)
	If Not nativeHandle Then Return Null
	Local receiver:TRMTReceiver = New TRMTReceiver
	receiver.handle = nativeHandle
	receiver.capacity = capacity
	Return receiver
End Function
?
