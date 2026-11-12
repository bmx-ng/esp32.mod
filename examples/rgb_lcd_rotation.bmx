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
SetClsColor 8, 12, 22
Cls
SetFont Terminus12x24BoldFont()

' Each label and corner marker is rendered through a different logical canvas
' orientation into the same physical framebuffer.
SetRotation 0
SetColor 100, 210, 255
DrawText "0 degrees", 24, 20
DrawRect 18, 14, TextWidth("0 degrees") + 12, FontHeight() + 12

SetRotation 90
SetColor 145, 235, 160
DrawText "90 degrees", 24, 20
DrawRect 18, 14, TextWidth("90 degrees") + 12, FontHeight() + 12

SetRotation 180
SetColor 255, 210, 90
DrawText "180 degrees", 24, 20
DrawRect 18, 14, TextWidth("180 degrees") + 12, FontHeight() + 12

SetRotation 270
SetColor 255, 125, 165
DrawText "270 degrees", 24, 20
DrawRect 18, 14, TextWidth("270 degrees") + 12, FontHeight() + 12

' A central asymmetric shape makes the physical orientation unambiguous.
SetRotation 0
SetColor 245, 248, 255
DrawRect 326, 176, 148, 128
DrawLine 326, 176, 473, 303
DrawLine 473, 176, 326, 303
FillRect 386, 224, 28, 80
FillRect 386, 224, 66, 28
DrawText "quarter-turn canvas", 274, 324

If Present() <= 0 Then RuntimeError "Could not present RGB framebuffer"

Print "RGB quarter-turn rotation specimen active"
While True
	Delay 1000
Wend
