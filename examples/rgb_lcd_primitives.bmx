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

SetRenderTarget frame, display
SetFont Terminus12x24BoldFont()
SetClsColor 7, 11, 20
Cls

SetColor 228, 236, 245
DrawText "INTEGER GRAPHICS PRIMITIVES", 24, 18

SetColor 55, 190, 255
DrawCircle 95, 130, 58
FillCircle 235, 130, 58
SetColor 7, 11, 20
FillCircle 235, 130, 28

SetColor 255, 198, 74
DrawEllipse 410, 130, 82, 50
FillEllipse 610, 130, 82, 50

SetColor 255, 105, 150
DrawTriangle 30, 300, 145, 222, 230, 350
FillTriangle 270, 350, 385, 222, 470, 350

SetColor 107, 224, 156
DrawRoundRect 510, 230, 254, 128, 30
FillRoundRect 535, 255, 204, 78, 22
SetColor 7, 40, 28
DrawText "ROUNDED", 555, 280

' The lower strip proves that primitives inherit a rotated local view.
If Not BeginView(24, 382, 752, 76, 0) Then RuntimeError "Could not begin primitive view"
SetColor 92, 112, 145
DrawRoundRect 0, 0, GraphicsWidth(), GraphicsHeight(), 18
For Local index:Int = 0 Until 7
	If Not BeginView(24 + index * 100, 12, 52, 52, index Mod 4 * 90) Then ..
		RuntimeError "Could not begin rotated primitive view"
	SetColor 155 + index * 14, 225 - index * 17, 255 - index * 20
	FillTriangle 2, 48, 26, 2, 50, 48
	EndView
Next
EndView

If Present() <= 0 Then RuntimeError "Could not present RGB framebuffer"

Print "RGB graphics primitive specimen active"
While True
	Delay 1000
Wend
