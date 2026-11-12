' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Goodix GT1151 capacitive touch controller for ESP32 displays.
End Rem
Module ESP32.Drivers.Touch.GT1151
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Drivers.Touch
Import "native/esp32_gt1151.c"

Type TGT1151Config
	Field width:Int
	Field height:Int
	Field i2cPort:Int
	Field sdaPin:Int = -1
	Field sclPin:Int = -1
	Field frequencyHz:UInt = 400000
	Field resetPin:Int = -1
	Field interruptPin:Int = -1
	Field swapXY:Int
	Field mirrorX:Int
	Field mirrorY:Int
End Type

Extern "C"
	Function _GT1151Create:Byte Ptr(width:Int, height:Int, i2cPort:Int, sdaPin:Int, ..
		sclPin:Int, frequencyHz:UInt, resetPin:Int, interruptPin:Int, swapXY:Int, ..
		mirrorX:Int, mirrorY:Int) = "bmx_esp32_gt1151_create"
	Function _GT1151Delete(handle:Byte Ptr) = "bmx_esp32_gt1151_delete"
	Function _GT1151Poll:Int(handle:Byte Ptr) = "bmx_esp32_gt1151_poll"
	Function _GT1151ContactId:Int(handle:Byte Ptr, index:Int) = "bmx_esp32_gt1151_contact_id"
	Function _GT1151ContactX:Int(handle:Byte Ptr, index:Int) = "bmx_esp32_gt1151_contact_x"
	Function _GT1151ContactY:Int(handle:Byte Ptr, index:Int) = "bmx_esp32_gt1151_contact_y"
	Function _GT1151LastError:Int() = "bmx_esp32_gt1151_last_error"
End Extern

Rem
bbdoc: Official ESP32-S31-Korvo-1 LCD-EV-Board-SUB3 touch configuration.
End Rem
Function ESP32S31Korvo1GT1151Config:TGT1151Config()
	Local config:TGT1151Config = New TGT1151Config
	config.width = 800
	config.height = 480
	config.i2cPort = 0
	config.sdaPin = 0
	config.sclPin = 1
	Return config
End Function

Function GT1151LastError:Int()
	Return _GT1151LastError()
End Function

Type TGT1151Touch Extends TTouchDevice
	Private
	Field handle:Byte Ptr
	Field touchWidth:Int
	Field touchHeight:Int

	Public
	Function Create:TGT1151Touch(config:TGT1151Config, generateEvents:Int = True)
		If Not config Or config.width <= 0 Or config.height <= 0 Or ..
			config.sdaPin < 0 Or config.sclPin < 0 Then Return Null
		Local nativeHandle:Byte Ptr = _GT1151Create(config.width, config.height, ..
			config.i2cPort, config.sdaPin, config.sclPin, config.frequencyHz, ..
			config.resetPin, config.interruptPin, config.swapXY, config.mirrorX, ..
			config.mirrorY)
		If Not nativeHandle Then Return Null
		Local touch:TGT1151Touch = New TGT1151Touch
		touch.handle = nativeHandle
		touch.touchWidth = config.width
		touch.touchHeight = config.height
		If generateEvents And Not touch.StartEvents() Then
			touch.Close()
			Return Null
		End If
		Return touch
	End Function

	Method Width:Int() Override
		Return touchWidth
	End Method

	Method Height:Int() Override
		Return touchHeight
	End Method

	Method PollContacts:Int() Override
		If Not handle Then Return -1
		Return _GT1151Poll(handle)
	End Method

	Method ContactId:Int(index:Int) Override
		Return _GT1151ContactId(handle, index)
	End Method

	Method ContactX:Int(index:Int) Override
		Return _GT1151ContactX(handle, index)
	End Method

	Method ContactY:Int(index:Int) Override
		Return _GT1151ContactY(handle, index)
	End Method

	Method Close()
		StopEvents()
		If handle Then
			_GT1151Delete(handle)
			handle = Null
		End If
	End Method

	Method Delete()
		Close()
	End Method
End Type
?
