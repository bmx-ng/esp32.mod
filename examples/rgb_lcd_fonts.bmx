SuperStrict

Framework BRL.StandardIO
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Fonts.Spleen.Size5x8
Import Embedded.Drivers.Display.Fonts.Spleen.Size6x12
Import Embedded.Drivers.Display.Fonts.Spleen.Size8x16
Import Embedded.Drivers.Display.Fonts.Spleen.Size12x24
Import Embedded.Drivers.Display.Fonts.Spleen.Size16x32
Import Embedded.Drivers.Display.Fonts.Spleen.Size32x64
Import Embedded.Graphics2D
Import ESP32.Drivers.Displays.RGB

Local display:TESP32RGBDisplay = TESP32RGBDisplay.Create(ESP32S31Korvo1ST7262Config())
If Not display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()

Local frame:TDisplayBuffer = display.CreateBuffer()
If Not frame Then RuntimeError "Could not access RGB display framebuffer"

SetRenderTarget frame, display
SetClsColor 9, 14, 24
Cls

SetColor 105, 210, 255
SetFont Spleen5x8Font()
DrawText "Spleen 5x8  ABCDEFGHIJKLMNOPQRSTUVWXYZ  0123456789", 24, 20

SetColor 150, 235, 170
SetFont Spleen6x12Font()
DrawText "Spleen 6x12  The quick brown fox jumps over the lazy dog.", 24, 46

SetColor 255, 220, 100
SetFont Spleen8x16Font()
DrawText "Spleen 8x16  BlitzMax on embedded displays", 24, 78

SetColor 255, 165, 100
SetFont Spleen12x24Font()
DrawText "Spleen 12x24  Bitmap text", 24, 118

SetColor 225, 130, 255
SetFont Spleen16x32Font()
DrawText "Spleen 16x32", 24, 170

SetColor 245, 248, 255
SetFont Spleen32x64Font()
DrawText "BlitzMax", 24, 232

SetColor 76, 102, 136
DrawRect 16, 12, 768, 304
If Present() <= 0 Then RuntimeError "Could not present RGB framebuffer"

Print "Spleen font-family specimen active"
While True
	Delay 1000
Wend
