SuperStrict

Framework BRL.StandardIO
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Fonts.Spleen.Size5x8
Import Embedded.Drivers.Display.Image
Import Embedded.Graphics2D
Import ESP32.Drivers.Displays.RGB

Incbin "assets/rgb_lcd_image.ebi"

Local display:TESP32RGBDisplay = TESP32RGBDisplay.Create(ESP32S31Korvo1ST7262Config())
If Not display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()

Local frame:TDisplayBuffer = display.CreateBuffer()
If Not frame Then RuntimeError "Could not access RGB display framebuffer"
Local logo:TEmbeddedImage = TEmbeddedImage.Create(IncbinPtr("assets/rgb_lcd_image.ebi"), ..
	IncbinLen("assets/rgb_lcd_image.ebi"))
If Not logo Then RuntimeError "Could not load image asset"

SetRenderTarget frame, display
SetFont Spleen5x8Font()
SetClsColor 12, 18, 32
Cls

Local barColors:UInt[] = [$ff3030, $ff9d20, $f4e640, $35d06f, $32a8ff, $655cff]
Local barWidth:Int = frame.Width() / barColors.length
For Local index:Int = 0 Until barColors.length
	SetColorValue barColors[index]
	FillRect index * barWidth, 0, barWidth + 1, 92
Next

SetColor 235, 240, 255
DrawRect 20, 112, 760, 348
DrawLine 20, 170, 779, 170
DrawText "BlitzMax", 48, 205, 8
SetColor 92, 204, 255
DrawText "ESP32-S31  RGB565", 52, 292, 4
SetColor 255, 215, 70
DrawImageRect logo, 646, 244, 96, 96

SetColor 235, 240, 255
DrawText "800 x 480 native PSRAM framebuffer", 52, 378, 3
If Present() <= 0 Then RuntimeError "Could not present RGB framebuffer"

Print "RGB LCD active: " + frame.Width() + "x" + frame.Height() + ..
	", framebuffer bytes=" + frame.Size()
While True
	Delay 1000
Wend
