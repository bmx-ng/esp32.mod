SuperStrict

Framework BRL.StandardIO
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Fonts.Terminus.Bold12x24
Import Embedded.Graphics2D
Import ESP32.Drivers.Displays.RGB

Local config:TESP32RGBDisplayConfig = ESP32S31Korvo1ST7262Config()
config.orientation = DisplayOrientation0
Local display:TESP32RGBDisplay = TESP32RGBDisplay.Create(config)
If Not display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()

Local frame:TDisplayBuffer = display.CreateBuffer()
If Not frame Then RuntimeError "Could not access RGB display framebuffer"

SetRenderTarget frame, display
SetFont Terminus12x24BoldFont()
SetRotation 0

Local orientations:Int[] = [DisplayOrientation0, DisplayOrientation90, ..
	DisplayOrientation180, DisplayOrientation270]

For Local orientation:Int = EachIn orientations
	If Not SetDisplayOrientation(orientation) Then RuntimeError "Invalid display orientation"
	SetClsColor 8, 12, 22
	Cls
	SetColor 100, 210, 255
	DrawRect 16, 16, GraphicsWidth() - 32, GraphicsHeight() - 32
	SetColor 245, 248, 255
	DrawText "Configured orientation: " + orientation, 36, 42
	DrawText "Logical size: " + GraphicsWidth() + " x " + GraphicsHeight(), 36, 82
	SetColor 255, 205, 80
	FillRect 36, 136, 160, 32
	FillRect 36, 136, 32, 120
	SetColor 145, 235, 160
	DrawText "rotation zero", 36, 286
	If Present() <= 0 Then RuntimeError "Could not present RGB framebuffer"
	Print "Orientation " + orientation + ": " + GraphicsWidth() + "x" + GraphicsHeight()
	Delay 2500
Next

' Leave the device in portrait after the demonstration.
SetDisplayOrientation DisplayOrientation90
SetClsColor 8, 12, 22
Cls
SetColor 145, 235, 160
DrawRect 16, 16, GraphicsWidth() - 32, GraphicsHeight() - 32
DrawText "Portrait is now rotation zero", 36, 42
DrawText GraphicsWidth() + " x " + GraphicsHeight(), 36, 82
Present

While True
	Delay 1000
Wend
