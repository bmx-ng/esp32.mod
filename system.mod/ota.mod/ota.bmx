' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Validated, transport-neutral ESP32 application updates and rollback control.
about: Bytes may come from a file, SD card, socket, or another stream. Finishing
an update validates its image but does not select it for boot; call #Activate
explicitly after all application-level checks have succeeded.
End Rem
Module ESP32.System.OTA
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import BRL.Stream

Const OTAImageStateNew:UInt = 0
Const OTAImageStatePendingVerify:UInt = 1
Const OTAImageStateValid:UInt = 2
Const OTAImageStateInvalid:UInt = 3
Const OTAImageStateAborted:UInt = 4
Const OTAImageStateUndefined:UInt = $ffffffff

Const OTAErrorNoMemory:Int = $101
Const OTAErrorInvalidArgument:Int = $102
Const OTAErrorInvalidState:Int = $103
Const OTAErrorInvalidSize:Int = $104
Const OTAErrorNotFound:Int = $105
Const OTAErrorNotSupported:Int = $106
Const OTAErrorBase:Int = $1500
Const OTAErrorPartitionConflict:Int = OTAErrorBase + $01
Const OTAErrorSelectInfoInvalid:Int = OTAErrorBase + $02
Const OTAErrorValidateFailed:Int = OTAErrorBase + $03
Const OTAErrorSmallSecureVersion:Int = OTAErrorBase + $04
Const OTAErrorRollbackFailed:Int = OTAErrorBase + $05
Const OTAErrorRollbackInvalidState:Int = OTAErrorBase + $06
Const OTAErrorAlreadyInProgress:Int = OTAErrorBase + $07
Const OTAErrorSPIModeMismatch:Int = OTAErrorBase + $08

Extern "C"
	Function OTAApplicationSlotCount:UInt() = "bmx_esp32_ota_slot_count"
	Function OTAIsSupported:Int() = "bmx_esp32_ota_supported"
	Function OTALastBeginError:Int() = "bmx_esp32_ota_last_begin_error"
	Function OTAResultName:String(result:Int) = "bmx_esp32_ota_result_name"
	Function _BeginOTAUpdate:Byte Ptr(expectedSize:UInt) = "bmx_esp32_ota_begin"
	Function _OTAIsOpen:Int(handle:Byte Ptr) = "bmx_esp32_ota_is_open"
	Function _OTAIsFinished:Int(handle:Byte Ptr) = "bmx_esp32_ota_is_finished"
	Function _OTALastError:Int(handle:Byte Ptr) = "bmx_esp32_ota_last_error"
	Function _OTABytesWritten:UInt(handle:Byte Ptr) = "bmx_esp32_ota_written"
	Function _OTAExpectedSize:UInt(handle:Byte Ptr) = "bmx_esp32_ota_expected_size"
	Function _OTACapacity:UInt(handle:Byte Ptr) = "bmx_esp32_ota_capacity"
	Function _OTATargetLabel:String(handle:Byte Ptr) = "bmx_esp32_ota_target_label"
	Function _OTAWrite:Int(handle:Byte Ptr, data:Byte Ptr, size:UInt) = "bmx_esp32_ota_write"
	Function _OTAFinish:Int(handle:Byte Ptr) = "bmx_esp32_ota_finish"
	Function _OTAActivate:Int(handle:Byte Ptr) = "bmx_esp32_ota_activate"
	Function _OTAAbort:Int(handle:Byte Ptr) = "bmx_esp32_ota_abort"
	Function _OTADestroy(handle:Byte Ptr) = "bmx_esp32_ota_destroy"
	Function OTARunningPartition:String() = "bmx_esp32_ota_running_label"
	Function OTABootPartition:String() = "bmx_esp32_ota_boot_label"
	Function OTARunningImageState:Int(state:UInt Var) = "bmx_esp32_ota_running_state"
	Function OTAMarkRunningImageValid:Int() = "bmx_esp32_ota_mark_valid"
	Function OTARollbackIsPossible:Int() = "bmx_esp32_ota_can_rollback"
	Function OTAMarkRunningImageInvalid:Int() = "bmx_esp32_ota_mark_invalid"
End Extern

Type TESP32OTAUpdate
	Field handle:Byte Ptr

	Method IsOpen:Int()
		Return _OTAIsOpen(handle)
	End Method

	Method IsFinished:Int()
		Return _OTAIsFinished(handle)
	End Method

	Method LastError:Int()
		Return _OTALastError(handle)
	End Method

	Method BytesWritten:UInt()
		Return _OTABytesWritten(handle)
	End Method

	Method ExpectedSize:UInt()
		Return _OTAExpectedSize(handle)
	End Method

	Method Capacity:UInt()
		Return _OTACapacity(handle)
	End Method

	Method TargetPartition:String()
		Return _OTATargetLabel(handle)
	End Method

	Rem
	bbdoc: Appends raw firmware bytes to the update image.
	End Rem
	Method Write:Int(data:Byte Ptr, size:UInt)
		Return _OTAWrite(handle, data, size)
	End Method

	Rem
	bbdoc: Appends all or part of a byte array to the update image.
	End Rem
	Method Write:Int(data:Byte[], offset:Int = 0, count:Int = -1)
		If offset < 0 Or offset > data.length Then Return OTAErrorInvalidArgument
		If count < 0 Then count = data.length - offset
		If count < 0 Or count > data.length - offset Then Return OTAErrorInvalidArgument
		If count = 0 Then Return _OTAWrite(handle, Null, 0)
		Return _OTAWrite(handle, Varptr data[offset], UInt(count))
	End Method

	Rem
	bbdoc: Copies firmware bytes from any stream without assuming its transport.
	about: Pass a non-negative byteCount when the source length is known. The
	default reads until the stream reports EOF. The stream remains open.
	End Rem
	Method WriteStream:Int(stream:TStream, byteCount:Long = -1, bufferSize:Int = 4096)
		If Not stream Or byteCount < -1 Or bufferSize <= 0 Then Return OTAErrorInvalidArgument
		Local buffer:Byte[] = New Byte[bufferSize]
		Local remaining:Long = byteCount
		While remaining <> 0
			Local wanted:Int = buffer.length
			If remaining > 0 And remaining < wanted Then wanted = Int(remaining)
			Local count:Long = stream.Read(buffer, wanted)
			If count <= 0
				If remaining > 0 Then Return OTAErrorInvalidSize
				Exit
			End If
			Local result:Int = Write(buffer, 0, Int(count))
			If result <> 0 Then Return result
			If remaining > 0 Then remaining :- count
		Wend
		Return 0
	End Method

	Rem
	bbdoc: Finalises and validates the image without changing the boot selection.
	End Rem
	Method Finish:Int()
		Return _OTAFinish(handle)
	End Method

	Rem
	bbdoc: Selects a successfully finished image for the next reboot.
	End Rem
	Method Activate:Int()
		Return _OTAActivate(handle)
	End Method

	Method FinishAndActivate:Int()
		Local result:Int = Finish()
		If result = 0 Then result = Activate()
		Return result
	End Method

	Method Abort:Int()
		Return _OTAAbort(handle)
	End Method

	Method Close()
		If handle Then
			_OTADestroy(handle)
			handle = Null
		End If
	End Method

	Method Delete()
		Close()
	End Method
End Type

Function BeginOTAUpdate:TESP32OTAUpdate(expectedSize:UInt = 0)
	Local nativeHandle:Byte Ptr = _BeginOTAUpdate(expectedSize)
	If Not nativeHandle Then Return Null
	Local update:TESP32OTAUpdate = New TESP32OTAUpdate
	update.handle = nativeHandle
	Return update
End Function

Function OTAImageStateName:String(state:UInt)
	Select state
		Case OTAImageStateNew Return "new"
		Case OTAImageStatePendingVerify Return "pending verification"
		Case OTAImageStateValid Return "valid"
		Case OTAImageStateInvalid Return "invalid"
		Case OTAImageStateAborted Return "aborted"
		Case OTAImageStateUndefined Return "undefined"
	End Select
	Return "unknown"
End Function
?
