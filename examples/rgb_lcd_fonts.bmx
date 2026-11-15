SuperStrict

Framework BRL.StandardIO
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Fonts.Anakron.Regular8x12
Import Embedded.Drivers.Display.Fonts.Cozette.Regular6x13
Import Embedded.Drivers.Display.Fonts.Scientifica.Regular
Import Embedded.Drivers.Display.Fonts.Scientifica.Bold
Import Embedded.Drivers.Display.Fonts.Scientifica.Italic
Import Embedded.Drivers.Display.Fonts.Spleen.Size16x32
Import Embedded.Drivers.Display.Fonts.Tamzen.Regular8x16
Import Embedded.Drivers.Display.Fonts.Tamzen.Bold8x16
Import Embedded.Drivers.Display.Fonts.Terminus.Regular12x24
Import Embedded.Drivers.Display.Fonts.Terminus.Bold12x24
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
SetFont Anakron8x12Font()
DrawText "ANAKRON 8x12  RETROFUTURISTIC BITMAP", 24, 20

SetColor 150, 235, 170
SetFont Cozette6x13Font()
DrawText "Cozette 6x13  The quick brown fox jumps over the lazy dog.", 24, 52

SetColor 255, 220, 100
SetFont ScientificaFont()
DrawText "Scientifica regular  Compact Unicode text and symbols", 24, 86

SetColor 255, 180, 100
SetFont ScientificaBoldFont()
DrawText "Scientifica bold  ABCDEFGHIJKLMNOPQRSTUVWXYZ", 24, 112

SetColor 225, 130, 255
SetFont ScientificaItalicFont()
DrawText "Scientifica italic  abcdefghijklmnopqrstuvwxyz", 24, 138

SetColor 110, 220, 255
SetFont Tamzen8x16Font()
DrawText "Tamzen 8x16 regular  Clear embedded interface text", 24, 174

SetColor 140, 245, 175
SetFont Tamzen8x16BoldFont()
DrawText "Tamzen 8x16 bold     Status: READY", 24, 206

SetColor 255, 220, 100
SetFont Terminus12x24Font()
DrawText "Terminus 12x24 regular", 24, 248

SetColor 255, 145, 105
SetFont Terminus12x24BoldFont()
DrawText "Terminus 12x24 bold", 24, 288

SetColor 245, 248, 255
SetFont Spleen16x32Font()
DrawText "Spleen 16x32  BlitzMax", 24, 346

SetColor 76, 102, 136
DrawRect 16, 12, 768, 402
If Present() <= 0 Then RuntimeError "Could not present RGB framebuffer"

Print "Spleen font-family specimen active"
While True
	Delay 1000
Wend
