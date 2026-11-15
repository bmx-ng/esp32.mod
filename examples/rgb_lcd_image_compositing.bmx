SuperStrict

Framework BRL.StandardIO
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Fonts.Terminus.Bold12x24
Import Embedded.Drivers.Display.Image
Import Embedded.Graphics2D
Import Embedded.System.Time
Import ESP32.Drivers.Displays.RGB

Function PutU16(data:Byte Ptr, offset:Int, value:Int)
	data[offset] = Byte(value)
	data[offset + 1] = Byte(value Shr 8)
End Function

Function PutU32(data:Byte Ptr, offset:Int, value:Int)
	data[offset] = Byte(value)
	data[offset + 1] = Byte(value Shr 8)
	data[offset + 2] = Byte(value Shr 16)
	data[offset + 3] = Byte(value Shr 24)
End Function

Function InitializeHeader(data:Byte Ptr, width:Int, height:Int, format:Int, stride:Int)
	MemClear data, 32
	data[0] = Asc("E"); data[1] = Asc("B")
	data[2] = Asc("I"); data[3] = Asc("1")
	PutU16 data, 4, EmbeddedImageVersion
	PutU16 data, 8, width
	PutU16 data, 10, height
	PutU16 data, 12, format
	PutU32 data, 16, stride
	PutU32 data, 20, 32
End Function

Function QuantizedRGB565:UInt(color:UInt)
	Local red:UInt = (color Shr 19) & $1f
	Local green:UInt = (color Shr 10) & $3f
	Local blue:UInt = (color Shr 3) & $1f
	Return (((red Shl 3) | (red Shr 2)) Shl 16) | ..
		(((green Shl 2) | (green Shr 4)) Shl 8) | ((blue Shl 3) | (blue Shr 2))
End Function

Const sourceWidth:Int = 64
Const sourceHeight:Int = 48
Const rgb565Stride:Int = sourceWidth * 2
Const paddedRGB565Stride:Int = rgb565Stride + 2
Const rgb888Stride:Int = sourceWidth * 3
Const rgb565Length:Int = 32 + rgb565Stride * sourceHeight
Const paddedRGB565Length:Int = 32 + paddedRGB565Stride * sourceHeight
Const rgb888Length:Int = 32 + rgb888Stride * sourceHeight

Local rgb565Data:Byte Ptr = MemAlloc(rgb565Length)
Local paddedRGB565Data:Byte Ptr = MemAlloc(paddedRGB565Length)
Local rgb888Data:Byte Ptr = MemAlloc(rgb888Length)
If Not rgb565Data Or Not paddedRGB565Data Or Not rgb888Data Then RuntimeError "Could not allocate image sources"
InitializeHeader rgb565Data, sourceWidth, sourceHeight, DisplayPixelFormatRGB565, rgb565Stride
InitializeHeader paddedRGB565Data, sourceWidth, sourceHeight, DisplayPixelFormatRGB565, paddedRGB565Stride
InitializeHeader rgb888Data, sourceWidth, sourceHeight, DisplayPixelFormatRGB888, rgb888Stride

' All sources contain the same RGB565-representable pattern. This makes the
' cross-format and native/PPA comparisons deterministic.
For Local y:Int = 0 Until sourceHeight
	For Local x:Int = 0 Until sourceWidth
		Local red5:Int = x * 31 / (sourceWidth - 1)
		Local green6:Int = y * 63 / (sourceHeight - 1)
		Local blue5:Int = ((x / 8) ~ (y / 8)) & 1
		If blue5 Then blue5 = 31
		Local packed:Int = (red5 Shl 11) | (green6 Shl 5) | blue5
		Local rgb565Offset:Int = 32 + y * rgb565Stride + x * 2
		rgb565Data[rgb565Offset] = Byte(packed)
		rgb565Data[rgb565Offset + 1] = Byte(packed Shr 8)
		Local paddedRGB565Offset:Int = 32 + y * paddedRGB565Stride + x * 2
		paddedRGB565Data[paddedRGB565Offset] = Byte(packed)
		paddedRGB565Data[paddedRGB565Offset + 1] = Byte(packed Shr 8)
		Local rgb888Offset:Int = 32 + y * rgb888Stride + x * 3
		rgb888Data[rgb888Offset] = Byte((red5 Shl 3) | (red5 Shr 2))
		rgb888Data[rgb888Offset + 1] = Byte((green6 Shl 2) | (green6 Shr 4))
		rgb888Data[rgb888Offset + 2] = Byte((blue5 Shl 3) | (blue5 Shr 2))
	Next
Next

Local rgb565Image:TEmbeddedImage = TEmbeddedImage.Create(rgb565Data, rgb565Length)
Local paddedRGB565Image:TEmbeddedImage = TEmbeddedImage.Create(paddedRGB565Data, paddedRGB565Length)
Local rgb888Image:TEmbeddedImage = TEmbeddedImage.Create(rgb888Data, rgb888Length)
If Not rgb565Image Or Not paddedRGB565Image Or Not rgb888Image Then ..
	RuntimeError "Could not open generated images"

Local display:TESP32RGBDisplay = TESP32RGBDisplay.Create(ESP32S31Korvo1ST7262Config())
If Not display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()
If Not display.HasCapability(DisplayCapabilityAcceleratedBlit) Then ..
	RuntimeError "This specimen requires accelerated image drawing"
Local frame:TDisplayBuffer = display.CreateBuffer()
If Not frame Then RuntimeError "Could not access RGB framebuffer"

SetRenderTarget frame, display
SetFont Terminus12x24BoldFont()
SetClsColor 8, 13, 28
Cls

' Exact-size PPA mirroring must match the native padded-stride path.
rgb565Image.Draw(frame, 0, 0, $ffffff, EmbeddedImageDrawFlipX)
paddedRGB565Image.Draw(frame, sourceWidth, 0, $ffffff, EmbeddedImageDrawFlipX)
Local mirrorMismatches:Int
For Local y:Int = 0 Until sourceHeight
	For Local x:Int = 0 Until sourceWidth
		If frame.Pixel(x, y) <> frame.Pixel(sourceWidth + x, y) Then mirrorMismatches :+ 1
	Next
Next
Cls

Local scaleStarted:ULong = MonotonicMicroseconds()
DrawImageRect rgb565Image, 8, 58, 384, 288
DrawImageRect paddedRGB565Image, 408, 58, 384, 288
Local scaleElapsed:ULong = MonotonicMicroseconds() - scaleStarted
Local scaleMismatches:Int
For Local y:Int = 0 Until 288
	For Local x:Int = 0 Until 384
		If frame.Pixel(8 + x, 58 + y) <> frame.Pixel(408 + x, 58 + y) Then ..
			scaleMismatches :+ 1
	Next
Next

' Compare unscaled RGB888 conversion with a padded RGB565 source. Both take
' the native path, so this isolates conversion from PPA sampling behavior.
paddedRGB565Image.Draw(frame, 72, 360)
rgb888Image.Draw(frame, 136, 360)
Local conversionMismatches:Int
For Local y:Int = 0 Until sourceHeight
	For Local x:Int = 0 Until sourceWidth
		If frame.Pixel(72 + x, 360 + y) <> frame.Pixel(136 + x, 360 + y) Then ..
			conversionMismatches :+ 1
	Next
Next

' Compare the native constant-opacity result with the established generic
' RGB888 compositing semantics, quantized to the RGB565 scanout format.
Local referencePixels:Byte Ptr = MemAlloc(rgb888Stride * sourceHeight)
Local reference:TDisplayBuffer = TDisplayBuffer.CreateExternal(sourceWidth, sourceHeight, ..
	DisplayPixelFormatRGB888, referencePixels, rgb888Stride)
If Not reference Then RuntimeError "Could not allocate reference buffer"
reference.Clear($001060)
rgb565Image.Draw(reference, 0, 0, $ffffff, 0, 128)
frame.FillRect 232, 360, sourceWidth, sourceHeight, $001060
Local blendStarted:ULong = MonotonicMicroseconds()
DrawImage rgb565Image, 232, 360, 0, 128
Local blendElapsed:ULong = MonotonicMicroseconds() - blendStarted
Local blendMismatches:Int
For Local y:Int = 0 Until sourceHeight
	For Local x:Int = 0 Until sourceWidth
		If frame.Pixel(232 + x, 360 + y) <> QuantizedRGB565(reference.Pixel(x, y)) Then ..
			blendMismatches :+ 1
	Next
Next

SetColor 255, 255, 255
DrawText "CONTIGUOUS SCALE", 110, 24
DrawText "PADDED SCALE", 540, 24
If conversionMismatches = 0 And blendMismatches = 0 And scaleMismatches = 0 And mirrorMismatches = 0 Then
	SetColor 80, 255, 120
	DrawText "ALL IMAGE PATHS: EXACT", 380, 412
Else
	SetColor 255, 80, 80
	DrawText "MISMATCH scale=" + scaleMismatches + " mirror=" + mirrorMismatches + ..
		" convert=" + conversionMismatches + " opacity=" + blendMismatches, 20, 412
End If
SetColor 255, 220, 80
DrawText "scale " + scaleElapsed + " us   blend " + blendElapsed + " us", 250, 448

If Present() <= 0 Then RuntimeError "Could not present RGB framebuffer"
Print "Native RGB888 conversion mismatches: " + conversionMismatches
Print "Native RGB565 opacity mismatches: " + blendMismatches
Print "Native scaling consistency mismatches: " + scaleMismatches
Print "PPA/native mirroring mismatches: " + mirrorMismatches
Print "Scaled image pair time: " + scaleElapsed + " us; opacity blend time: " + blendElapsed + " us"

While True
	Delay 1000
Wend
