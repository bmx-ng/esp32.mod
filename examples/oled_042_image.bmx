SuperStrict

Framework BRL.StandardIO
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Image
Import Embedded.Drivers.Displays.SSD1306
Import Embedded.Graphics2D
Import Embedded.Hardware.I2C

Incbin "assets/oled_042_image.ebi"

Local controller:Int = I2CDefaultController()
Local sda:UInt = I2CDefaultSDAPin()
Local scl:UInt = I2CDefaultSCLPin()
If controller < 0 Or sda = I2CUnavailablePin Or scl = I2CUnavailablePin Then ..
	RuntimeError "The selected board has no default I2C bus"
If Not I2CConfigurePins(controller, sda, scl, True) Then RuntimeError "Could not configure I2C pins"
If I2CInit(controller, I2CSpeedFast) = 0 Then RuntimeError "Could not initialize I2C"

Local display:TSSD1306Display = TSSD1306Display.Create72x40(controller)
If Not display Or Not display.Initialize() Then RuntimeError "Could not initialize the OLED"

Local frame:TDisplayBuffer = display.CreateBuffer()
Local image:TEmbeddedImage = TEmbeddedImage.Create(IncbinPtr("assets/oled_042_image.ebi"), ..
	IncbinLen("assets/oled_042_image.ebi"))
If Not image Then RuntimeError "Could not load the embedded image"

SetRenderTarget frame, display
SetClsColor 0, 0, 0
Cls

' A black, scaled image over white verifies that transparent pixels preserve the background.
SetColor 255, 255, 255
FillRect 1, 3, 34, 34
SetColor 0, 0, 0
DrawImageRect image, 2, 4, 32, 32

' Draw the same image at its native size and normal polarity.
SetColor 255, 255, 255
DrawImage image, 48, 12

If Present() <= 0 Then RuntimeError "Could not present the OLED frame"

Print "OLED image displayed: " + image.Width() + "x" + image.Height() + ..
	", mask=" + image.HasMask()
While True
	Delay 1000
Wend
