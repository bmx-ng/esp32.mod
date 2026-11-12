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

Const sourceWidth:Int = 128
Const sourceHeight:Int = 96
Const sourceStride:Int = sourceWidth * 2
Const sourceLength:Int = 32 + sourceStride * sourceHeight

' Build a DMA-accessible RGB565 image. Firmware-resident EBI assets remain valid,
' but use the portable native fallback when their flash mapping is not DMA-capable.
Local sourceData:Byte Ptr = MemAlloc(sourceLength)
If Not sourceData Then RuntimeError "Could not allocate image source"
MemClear sourceData, sourceLength
sourceData[0] = Asc("E"); sourceData[1] = Asc("B")
sourceData[2] = Asc("I"); sourceData[3] = Asc("1")
PutU16 sourceData, 4, EmbeddedImageVersion
PutU16 sourceData, 8, sourceWidth
PutU16 sourceData, 10, sourceHeight
PutU16 sourceData, 12, DisplayPixelFormatRGB565
PutU32 sourceData, 16, sourceStride
PutU32 sourceData, 20, 32

For Local y:Int = 0 Until sourceHeight
	For Local x:Int = 0 Until sourceWidth
		Local red:Int = x * 31 / (sourceWidth - 1)
		Local green:Int = y * 63 / (sourceHeight - 1)
		Local blue:Int = ((x / 12) ~ (y / 12)) & 1
		If x < 10 Or y < 10 Then red = 31; green = 63; blue = 1
		Local rgb565:Int = (red Shl 11) | (green Shl 5) | (blue * 31)
		Local offset:Int = 32 + y * sourceStride + x * 2
		sourceData[offset] = Byte(rgb565)
		sourceData[offset + 1] = Byte(rgb565 Shr 8)
	Next
Next

Local image:TEmbeddedImage = TEmbeddedImage.Create(sourceData, sourceLength)
If Not image Then RuntimeError "Could not open generated image"

Local display:TESP32RGBDisplay = TESP32RGBDisplay.Create(ESP32S31Korvo1ST7262Config())
If Not display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()
If Not display.HasCapability(DisplayCapabilityAcceleratedBlit) Then ..
	RuntimeError "This specimen requires accelerated image drawing"
Local frame:TDisplayBuffer = display.CreateBuffer()
If Not frame Then RuntimeError "Could not access RGB framebuffer"

SetRenderTarget frame, display
SetFont Terminus12x24BoldFont()
SetClsColor 9, 15, 30
Cls
SetColor 255, 255, 255
DrawText "PPA RGB565 IMAGE COPY + MIRROR", 190, 84

Local started:ULong = MonotonicMicroseconds()
DrawImage image, 264, 160
DrawImage image, 408, 160, ImageFlipX
Local elapsed:ULong = MonotonicMicroseconds() - started

SetColor 255, 220, 80
DrawText "two exact-size 128x96 blits: " + elapsed + " us", 180, 310
If Present() <= 0 Then RuntimeError "Could not present RGB framebuffer"
Print "PPA image specimen drew two exact-size RGB565 images in " + elapsed + " us"

While True
	Delay 1000
Wend
