SuperStrict

Framework BRL.StandardIO
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Fonts.Terminus.Bold12x24
Import Embedded.Graphics2D
Import Embedded.System.Time
Import ESP32.Drivers.Displays.RGB

Local display:TESP32RGBDisplay = TESP32RGBDisplay.Create(ESP32S31Korvo1ST7262Config())
If Not display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()
If Not display.HasCapability(DisplayCapabilityBufferSwap) Or display.FramebufferCount() <> 2 Then ..
	RuntimeError "This specimen requires two RGB framebuffers"
If Not display.HasCapability(DisplayCapabilityDiscardPresent) Then ..
	RuntimeError "This specimen requires discard presentation"

Local frame:TDisplayBuffer = display.CreateBuffer()
If Not frame Then RuntimeError "Could not access RGB render framebuffer"

SetRenderTarget frame, display
SetFont Terminus12x24BoldFont()

Local phase:Int
Local ballX:Int = 64
Local ballStep:Int = 4
Local measuredFPS:Int
Local measurementFrames:Int
Local measurementStarted:ULong = MonotonicMilliseconds()
While True
	Local oldPixels:Byte Ptr = frame.Pixels()
	If phase & 1 Then
		SetClsColor 7, 18, 42
	Else
		SetClsColor 42, 11, 24
	End If
	Cls

	For Local band:Int = 0 Until 8
		Local red:Int = (phase * 13 + band * 29) & 255
		Local green:Int = (48 + band * 23) & 255
		Local blue:Int = (210 - band * 17) & 255
		SetColor red, green, blue
		FillRect band * 100, 96, 100, 240
	Next

	SetColor 255, 255, 255
	DrawText "DOUBLE BUFFER / FULL FRAME", 192, 30
	DrawText "FPS: " + measuredFPS, 336, 352
	DrawRect 16, 16, 768, 448
	SetColor 255, 221, 72
	FillCircle ballX, 408, 34

	If PresentDiscard() <= 0 Then RuntimeError "Could not swap RGB framebuffers"
	If frame.Pixels() = oldPixels Then RuntimeError "RGB framebuffer did not swap"

	ballX :+ ballStep
	If ballX >= 736 Or ballX <= 64 Then ballStep = -ballStep
	phase :+ 1
	measurementFrames :+ 1
	Local now:ULong = MonotonicMilliseconds()
	Local elapsed:ULong = now - measurementStarted
	If elapsed >= 1000 Then
		measuredFPS = Int((Long(measurementFrames) * 1000) / Long(elapsed))
		Print "RGB full-frame presentation: " + measuredFPS + " FPS"
		measurementFrames = 0
		measurementStarted = now
	End If
Wend
