SuperStrict

Framework BRL.StandardIO
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Fonts.Terminus.Bold12x24
Import Embedded.Graphics2D
Import ESP32.Drivers.Displays.RGB

Local display:TESP32RGBDisplay = TESP32RGBDisplay.Create(ESP32S31Korvo1ST7262Config())
If Not display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()

Local frame:TDisplayBuffer = display.CreateBuffer()
If Not frame Then RuntimeError "Could not access RGB display framebuffer"

Const Background:UInt = $081225
Const Lane:UInt = $122947
Const Accent:UInt = $ffd248

SetRenderTarget frame, display
SetFont Terminus12x24BoldFont()
SetClsColorValue Background
Cls
SetColor 230, 238, 250
DrawText "DIRTY REGION ANIMATION", 24, 24
SetColorValue Lane
FillRoundRect 24, 132, 752, 156, 28
SetColor 104, 132, 168
DrawText "Only the moving union is presented", 126, 350
Local initialPixels:Byte Ptr = frame.Pixels()
Local preservedPixel:UInt = frame.Pixel(400, 150)
If Present() <= 0 Then RuntimeError "Could not present initial RGB framebuffer"
If display.HasCapability(DisplayCapabilityBufferSwap) Then
	If frame.Pixels() = initialPixels Then RuntimeError "RGB framebuffer did not swap"
	If frame.Pixel(400, 150) <> preservedPixel Then RuntimeError "RGB back buffer lost frame contents"
End If

SetDirtyTracking True
Local x:Int = 48
Local previousX:Int = x
Local direction:Int = 4
While True
	' Single-buffer panels expose live scanout storage and must begin mutations at
	' a frame boundary. Double-buffer panels draw invisibly and swap on PresentDirty.
	If display.HasCapability(DisplayCapabilityLiveFramebuffer) And ..
			Not display.WaitVSync() Then RuntimeError "Timed out waiting for RGB VSYNC"
	SetColorValue Lane
	FillRect previousX, 170, 72, 72

	x :+ direction
	If x >= 680 Then
		x = 680
		direction = -4
	Else If x <= 48 Then
		x = 48
		direction = 4
	End If

	SetColorValue Accent
	FillRoundRect x, 170, 72, 72, 16
	SetColor 78, 55, 6
	DrawLine x + 18, 206, x + 54, 206
	DrawLine x + 44, 194, x + 54, 206
	DrawLine x + 44, 218, x + 54, 206

	Local renderPixels:Byte Ptr = frame.Pixels()
	Local renderedPixel:UInt = frame.Pixel(x + 36, 206)
	If PresentDirty() <= 0 Then RuntimeError "Could not present dirty RGB region"
	If display.HasCapability(DisplayCapabilityBufferSwap) Then
		If frame.Pixels() = renderPixels Then RuntimeError "RGB framebuffer did not swap"
		If frame.Pixel(x + 36, 206) <> renderedPixel Then ..
			RuntimeError "RGB dirty region was not preserved"
	End If
	previousX = x
Wend
