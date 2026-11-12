' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP32 parallel RGB display panels using an ESP-IDF framebuffer.
about: The panel framebuffer is allocated by ESP-IDF, normally in PSRAM, and
wrapped by TDisplayBuffer without a second screen-sized managed allocation.
End Rem
Module ESP32.Drivers.Displays.RGB
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Drivers.Display
Import "native/esp32_rgb_display.c"
Import "native/esp32_display_acceleration.c"

Type TESP32RGBDisplayConfig
	Field width:Int
	Field height:Int
	Field orientation:Int = DisplayOrientation0
	Field pixelClockHz:UInt
	Field hsyncPulseWidth:Int
	Field hsyncBackPorch:Int
	Field hsyncFrontPorch:Int
	Field vsyncPulseWidth:Int
	Field vsyncBackPorch:Int
	Field vsyncFrontPorch:Int
	Field hsyncPin:Int = -1
	Field vsyncPin:Int = -1
	Field dataEnablePin:Int = -1
	Field pixelClockPin:Int = -1
	Field displayEnablePin:Int = -1
	Field dataPins:Int[16]
	Field pixelClockActiveNegative:Int
	Field displayEnableActiveLow:Int
	Rem
	bbdoc: Number of RGB framebuffers allocated by ESP-IDF, from one through three.
	about: One buffer minimizes memory use, two prevent visible drawing and tearing,
	and three allow rendering to overlap a queued display swap. Each 800x480 RGB565
	buffer occupies 768,000 bytes.
	End Rem
	Field framebufferCount:Int = 1
End Type

Extern "C"
	Function _RGBDisplaySupported:Int() = "bmx_esp32_rgb_display_supported"
	Function _RGBDisplayAccelerationInitialize:Int() = ..
		"bmx_esp32_display_acceleration_initialize"
	Function _RGBDisplayCreate:Byte Ptr(width:Int, height:Int, pixelClockHz:UInt, ..
		hsyncPulseWidth:Int, hsyncBackPorch:Int, hsyncFrontPorch:Int, ..
		vsyncPulseWidth:Int, vsyncBackPorch:Int, vsyncFrontPorch:Int, ..
		hsyncPin:Int, vsyncPin:Int, dataEnablePin:Int, pixelClockPin:Int, ..
		displayEnablePin:Int, dataPins:Int Ptr, pixelClockActiveNegative:Int, ..
		displayEnableActiveLow:Int, framebufferCount:Int) = "bmx_esp32_rgb_display_create"
	Function _RGBDisplayDelete(handle:Byte Ptr) = "bmx_esp32_rgb_display_delete"
	Function _RGBDisplayFramebuffer:Byte Ptr(handle:Byte Ptr) = "bmx_esp32_rgb_display_framebuffer"
	Function _RGBDisplayFramebufferCount:Int(handle:Byte Ptr) = "bmx_esp32_rgb_display_framebuffer_count"
	Function _RGBDisplayPresent:Int(handle:Byte Ptr, pixels:Byte Ptr, stride:Int, ..
		x:Int, y:Int, width:Int, height:Int, preserveContents:Int) = "bmx_esp32_rgb_display_present"
	Function _RGBDisplaySetEnabled:Int(handle:Byte Ptr, enabled:Int) = "bmx_esp32_rgb_display_set_enabled"
	Function _RGBDisplayWaitVSync:Int(handle:Byte Ptr, timeoutMs:Int) = "bmx_esp32_rgb_display_wait_vsync"
	Function _RGBDisplayLastError:Int() = "bmx_esp32_rgb_display_last_error"
End Extern

Rem
bbdoc: Returns whether the selected ESP32 target has a parallel RGB LCD peripheral.
End Rem
Function RGBDisplaySupported:Int()
	Return _RGBDisplaySupported()
End Function

Rem
bbdoc: Returns the ESP-IDF error from the most recent failed panel creation.
End Rem
Function RGBDisplayLastError:Int()
	Return _RGBDisplayLastError()
End Function

Rem
bbdoc: Official ESP32-S31-Korvo-1 and LCD-EV-Board-SUB3 panel configuration.
End Rem
Function ESP32S31Korvo1ST7262Config:TESP32RGBDisplayConfig()
	Local config:TESP32RGBDisplayConfig = New TESP32RGBDisplayConfig
	config.width = 800
	config.height = 480
	config.pixelClockHz = 18000000
	config.hsyncPulseWidth = 40
	config.hsyncBackPorch = 40
	config.hsyncFrontPorch = 48
	config.vsyncPulseWidth = 23
	config.vsyncBackPorch = 32
	config.vsyncFrontPorch = 13
	config.hsyncPin = 44
	config.vsyncPin = 45
	config.dataEnablePin = 43
	config.pixelClockPin = 40
	config.displayEnablePin = 38
	config.dataPins = [8, 9, 10, 11, 12, 13, 14, 15, ..
		16, 17, 18, 19, 33, 34, 35, 36]
	config.pixelClockActiveNegative = True
	config.framebufferCount = 2
	Return config
End Function

Type TESP32RGBDisplay Extends TDisplay
	Private
	Field handle:Byte Ptr
	Field displayWidth:Int
	Field displayHeight:Int
	Field framebuffer:Byte Ptr
	Field framebufferCount:Int
	Field acceleratedFill:Int
	Field buffer:TDisplayBuffer

	Public
	Function Create:TESP32RGBDisplay(config:TESP32RGBDisplayConfig)
		If Not config Or config.width <= 0 Or config.height <= 0 Or ..
			config.pixelClockHz = 0 Or Not config.dataPins Or ..
			config.framebufferCount < 1 Or config.framebufferCount > 3 Then Return Null
		Local acceleratedFill:Int = _RGBDisplayAccelerationInitialize()
		Local nativeHandle:Byte Ptr = _RGBDisplayCreate(config.width, config.height, ..
			config.pixelClockHz, config.hsyncPulseWidth, config.hsyncBackPorch, ..
			config.hsyncFrontPorch, config.vsyncPulseWidth, config.vsyncBackPorch, ..
			config.vsyncFrontPorch, config.hsyncPin, config.vsyncPin, ..
			config.dataEnablePin, config.pixelClockPin, config.displayEnablePin, ..
			Varptr config.dataPins[0], config.pixelClockActiveNegative, ..
			config.displayEnableActiveLow, config.framebufferCount)
		If Not nativeHandle Then Return Null
		Local display:TESP32RGBDisplay = New TESP32RGBDisplay
		display.handle = nativeHandle
		display.displayWidth = config.width
		display.displayHeight = config.height
		display.framebufferCount = _RGBDisplayFramebufferCount(nativeHandle)
		display.acceleratedFill = acceleratedFill
		If Not display.SetOrientation(config.orientation) Then
			display.Close()
			Return Null
		End If
		display.framebuffer = _RGBDisplayFramebuffer(nativeHandle)
		display.buffer = TDisplayBuffer.CreateExternal(config.width, config.height, ..
			DisplayPixelFormatRGB565, display.framebuffer, config.width * 2)
		If Not display.buffer Then
			display.Close()
			Return Null
		End If
		Return display
	End Function

	Method Width:Int() Override
		Return displayWidth
	End Method

	Method Height:Int() Override
		Return displayHeight
	End Method

	Method PreferredPixelFormat:Int() Override
		Return DisplayPixelFormatRGB565
	End Method

	Method Capabilities:Int() Override
		Local result:Int = DisplayCapabilityPartialPresent | DisplayCapabilityVSync
		If framebufferCount > 1 Then
			result :| DisplayCapabilityBufferSwap | DisplayCapabilityDiscardPresent
		Else
			result :| DisplayCapabilityLiveFramebuffer
		End If
		If acceleratedFill Then result :| DisplayCapabilityAcceleratedFill
		Return result
	End Method

	Method FramebufferCount:Int()
		Return framebufferCount
	End Method

	Method CreateBuffer:TDisplayBuffer() Override
		Return buffer
	End Method

	Method PresentPixels:Int(pixels:Byte Ptr, stride:Int, format:Int, ..
			x:Int, y:Int, width:Int, height:Int) Override
		If Not handle Or Not pixels Then Return DisplayErrorInvalidArgument
		If format <> DisplayPixelFormatRGB565 Then Return DisplayErrorUnsupportedFormat
		If stride <> displayWidth * 2 Then Return DisplayErrorInvalidArgument
		If _RGBDisplayPresent(handle, pixels, stride, x, y, width, height, True) <> 0 Then ..
			Return DisplayErrorGeneric
		If framebufferCount > 1 Then
			framebuffer = _RGBDisplayFramebuffer(handle)
			If Not framebuffer Or Not buffer.RetargetExternal(framebuffer) Then ..
				Return DisplayErrorGeneric
		End If
		Return True
	End Method

	Method PresentPixelsDiscard:Int(pixels:Byte Ptr, stride:Int, format:Int) Override
		If Not handle Or Not pixels Then Return DisplayErrorInvalidArgument
		If format <> DisplayPixelFormatRGB565 Then Return DisplayErrorUnsupportedFormat
		If stride <> displayWidth * 2 Then Return DisplayErrorInvalidArgument
		If _RGBDisplayPresent(handle, pixels, stride, 0, 0, displayWidth, displayHeight, False) <> 0 Then ..
			Return DisplayErrorGeneric
		If framebufferCount > 1 Then
			framebuffer = _RGBDisplayFramebuffer(handle)
			If Not framebuffer Or Not buffer.RetargetExternal(framebuffer) Then ..
				Return DisplayErrorGeneric
		End If
		Return True
	End Method

	Method SetEnabled:Int(enabled:Int) Override
		Return handle And _RGBDisplaySetEnabled(handle, enabled) = 0
	End Method

	Rem
	bbdoc: Waits for the next vertical synchronization boundary.
	about: This is useful before changing the live framebuffer of a continuously
	scanned RGB panel. A timeout of zero performs a non-blocking poll.
	returns: True when a frame boundary was observed, or False on timeout/error.
	End Rem
	Method WaitVSync:Int(timeoutMs:Int = 1000)
		Return handle And timeoutMs >= 0 And _RGBDisplayWaitVSync(handle, timeoutMs) = 0
	End Method

	Method Close()
		buffer = Null
		framebuffer = Null
		If handle Then
			_RGBDisplayDelete(handle)
			handle = Null
		End If
	End Method

	Method Delete()
		Close()
	End Method
End Type
?
