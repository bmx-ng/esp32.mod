SuperStrict

Framework BRL.StandardIO
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Fonts.Terminus.Bold10x20
Import Embedded.Drivers.Display.Image
Import Embedded.Graphics2D
Import ESP32.Drivers.Displays.RGB

Incbin "assets/rgb_lcd_image_options.ebi"

Local display:TESP32RGBDisplay = TESP32RGBDisplay.Create(ESP32S31Korvo1ST7262Config())
If Not display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()

Local frame:TDisplayBuffer = display.CreateBuffer()
If Not frame Then RuntimeError "Could not access RGB display framebuffer"
Local arrow:TEmbeddedImage = TEmbeddedImage.Create(IncbinPtr("assets/rgb_lcd_image_options.ebi"), ..
	IncbinLen("assets/rgb_lcd_image_options.ebi"))
If Not arrow Then RuntimeError "Could not load image-options asset"

SetRenderTarget frame, display
SetFont Terminus10x20BoldFont()
SetClsColor 8, 13, 25
Cls

SetColor 230, 238, 250
DrawText "IMAGE DRAW OPTIONS", 24, 18

' Four backgrounds make transparency and source-over blending obvious.
Local panelColors:UInt[] = [$17233c, $17233c, $254f38, $66352b]
For Local index:Int = 0 Until 4
	SetColorValue panelColors[index]
	FillRoundRect 24 + index * 194, 70, 170, 260, 18
Next

SetColor 230, 238, 250
DrawText "NORMAL", 51, 90
DrawText "FLIP X", 245, 90
DrawText "ALPHA", 439, 90
DrawText "KEY", 650, 90

SetColor 255, 255, 255
DrawImageRect arrow, 45, 146, 128, 128
DrawImageRect arrow, 239, 146, 128, 128, ImageFlipX
DrawImageRect arrow, 433, 146, 128, 128, 0, 96
DrawImageRect arrow, 627, 146, 128, 128, 0, 255, $ff00ff

' Combined flags are shown in a smaller footer specimen.
SetColor 136, 157, 190
DrawText "FLIP X | FLIP Y", 28, 375
SetColor 255, 210, 72
DrawImageRect arrow, 246, 350, 96, 96, ImageFlipX | ImageFlipY, 192, $ff00ff
SetColor 230, 238, 250
DrawText "mask/key pixels preserve the framebuffer", 370, 382

If Present() <= 0 Then RuntimeError "Could not present RGB framebuffer"

Print "RGB image-options specimen active"
While True
	Delay 1000
Wend
