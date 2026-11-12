SuperStrict

Framework BRL.StandardIO
Import BRL.Event
Import BRL.System
Import BRL.SystemDefault
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Fonts.Spleen5x8
Import Embedded.Graphics2D
Import ESP32.Drivers.Displays.RGB
Import ESP32.Drivers.Touch.GT1151

Global Display:TESP32RGBDisplay
Global Frame:TDisplayBuffer
Global Touch:TGT1151Touch

Function OnTouch:Object(hookId:Int, data:Object, context:Object)
	Local event:TEvent = TEvent(data)
	If Not event Or event.source <> Touch Then Return data
	If event.id <> EVENT_TOUCHDOWN And event.id <> EVENT_TOUCHMOVE And ..
		event.id <> EVENT_TOUCHUP Then Return data

	Local x:Int = event.x * (Frame.Width() - 1) / 10000
	Local y:Int = event.y * (Frame.Height() - 1) / 10000
	If event.id = EVENT_TOUCHUP Then
		Print "Touch up " + event.data + " at " + x + "," + y
		Return data
	End If

	If event.id = EVENT_TOUCHDOWN Then
		Print "Touch down " + event.data + " at " + x + "," + y
	End If
	SetColor 255, 225, 40
	FillRect x - 5, y - 5, 11, 11
	SetColor 255, 255, 255
	DrawRect x - 8, y - 8, 17, 17
	Present
	Return data
End Function

Display = TESP32RGBDisplay.Create(ESP32S31Korvo1ST7262Config())
If Not Display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()
Frame = Display.CreateBuffer()

SetRenderTarget Frame, Display
SetFont Spleen5x8Font()
SetClsColor 10, 18, 30
Cls
SetColor 92, 204, 255
DrawText "ESP32-S31 touch test", 48, 48, 5
SetColor 235, 240, 255
DrawText "Touch or drag anywhere on the panel", 52, 124, 3
DrawRect 24, 190, 752, 260

' A regular 16x16 checkerboard of exact 6x6 squares makes panel pixel geometry
' and nearest-neighbour row expansion easy to inspect independently of artwork.
For Local row:Int = 0 Until 16
	For Local column:Int = 0 Until 16
		If (row + column) & 1 Then SetColor 255, 225, 40 Else SetColor 32, 48, 72
		FillRect 680 + column * 6, 30 + row * 6, 6, 6
	Next
Next
Present
SetViewport 25, 191, 750, 258

Touch = TGT1151Touch.Create(ESP32S31Korvo1GT1151Config())
If Not Touch Then RuntimeError "Could not initialize GT1151 touch: " + GT1151LastError()
AddHook EmitEventHook, OnTouch

Print "GT1151 active; touch events use normalized 0..10000 coordinates"
While True
	PollSystem
	Delay 10
Wend
