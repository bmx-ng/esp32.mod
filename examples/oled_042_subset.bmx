SuperStrict

Framework BRL.StandardIO
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Font
Import Embedded.Drivers.Displays.SSD1306
Import Embedded.Hardware.I2C

Incbin "assets/oled_042_subset.ebf"

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
Local font:TBitmapFont = TBitmapFont.Create(IncbinPtr("assets/oled_042_subset.ebf"), ..
	IncbinLen("assets/oled_042_subset.ebf"))
If Not font Then RuntimeError "Could not load the subset font"

frame.Clear()
font.DrawText(frame, "Subset font~n123.4 C~nMissing: Ω", 1, 0)
If display.Present(frame) <= 0 Then RuntimeError "Could not present the OLED frame"

Print "OLED subset font displayed; glyphs=" + font.GlyphCount()
While True
	Delay 1000
Wend
