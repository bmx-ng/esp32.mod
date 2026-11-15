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
DrawText "WRAPPED TEXT LAYOUT", 24, 18

Local message:String = "Bitmap text now wraps at word boundaries without allocating " + ..
	"temporary strings. Each panel uses the same text and a different alignment."

DrawTextPanel 24, 72, 232, 300, "LEFT", message, TextAlignLeft, $17344a, $70d7ff
DrawTextPanel 284, 72, 232, 300, "CENTRE", message, TextAlignCenter, $43243a, $ff91c1
DrawTextPanel 544, 72, 232, 300, "RIGHT", message, TextAlignRight, $3d3517, $ffda72

SetColor 107, 224, 156
DrawRoundRect 24, 392, 752, 66, 16
DrawTextBox "A deliberately short final box demonstrates deterministic overflow ellipsis.", ..
	42, 403, 716, 40, TextAlignCenter, 1, True

If Present() <= 0 Then RuntimeError "Could not present RGB framebuffer"

Print "RGB text-layout specimen active"
While True
	Delay 1000
Wend

Function DrawTextPanel(x:Int, y:Int, width:Int, height:Int, label:String, text:String, ..
		alignment:Int, background:UInt, foreground:UInt)
	If Not BeginView(x, y, width, height) Then RuntimeError "Could not begin text panel"
	SetClsColorValue background
	Cls
	SetColorValue foreground
	DrawRoundRect 0, 0, GraphicsWidth(), GraphicsHeight(), 18
	DrawTextBox label, 12, 12, GraphicsWidth() - 24, 28, TextAlignCenter, 1, False
	DrawLine 12, 48, GraphicsWidth() - 13, 48
	DrawTextBox text, 14, 64, GraphicsWidth() - 28, GraphicsHeight() - 78, ..
		alignment, 1, True
	EndView
End Function
