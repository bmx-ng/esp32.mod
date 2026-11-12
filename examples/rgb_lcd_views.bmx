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

' Every panel uses the same local drawing code. Its position, clip, and
' quarter-turn are supplied by the enclosing view.
DrawPanel 16, 16, 376, 210, 0, "VIEW 0", $17344a, $70d7ff
DrawPanel 408, 16, 376, 210, 90, "VIEW 90", $163b2b, $8df0ad
DrawPanel 16, 246, 376, 210, 270, "VIEW 270", $43243a, $ff91c1
DrawPanel 408, 246, 376, 210, 180, "VIEW 180", $4a3918, $ffda72

If Present() <= 0 Then RuntimeError "Could not present RGB framebuffer"

Print "RGB nested-view specimen active"
While True
	Delay 1000
Wend

Function DrawPanel(x:Int, y:Int, width:Int, height:Int, rotation:Int, ..
		label:String, background:UInt, foreground:UInt)
	If Not BeginView(x, y, width, height, rotation) Then RuntimeError "Could not begin panel view"
	SetClsColorValue background
	Cls
	SetColorValue foreground
	DrawRect 0, 0, GraphicsWidth(), GraphicsHeight()
	DrawText label, 18, 16
	DrawLine 18, 52, GraphicsWidth() - 19, 52
	DrawLine 18, 62, GraphicsWidth() - 19, GraphicsHeight() - 18
	DrawLine GraphicsWidth() - 19, 62, 18, GraphicsHeight() - 18

	' A child view has its own origin, clip, background, and rotation. It is
	' intentionally partly outside the parent's right edge to prove clipping.
	Local childX:Int = GraphicsWidth() - 116
	Local childY:Int = GraphicsHeight() - 88
	If Not BeginView(childX, childY, 132, 72, 90) Then RuntimeError "Could not begin child view"
	SetClsColor 12, 17, 29
	Cls
	SetColor 245, 248, 255
	DrawRect 0, 0, GraphicsWidth(), GraphicsHeight()
	DrawText "NEST", 8, 8
	EndView

	' Ordinary state changes are scoped without introducing another view.
	If Not PushGraphicsState() Then RuntimeError "Could not save graphics state"
	SetOrigin 18, GraphicsHeight() - 38
	SetColor 245, 248, 255
	DrawText "LOCAL", 0, 0
	PopGraphicsState
	EndView
End Function
