SuperStrict

Framework BRL.StandardIO
Import Embedded.Drivers.Display
Import Embedded.Drivers.Display.Fonts.Terminus.Bold10x20
Import Embedded.Graphics2D
Import Embedded.Graphics2D.Sprite
Import Embedded.System.Time
Import ESP32.Drivers.Displays.RGB

Incbin "assets/rgb_lcd_sprite_atlas.ebi"
Include "assets/rgb_lcd_sprite_atlas.bmx"

Local display:TESP32RGBDisplay = TESP32RGBDisplay.Create(ESP32S31Korvo1ST7262Config())
If Not display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()

Local frame:TDisplayBuffer = display.CreateBuffer()
If Not frame Then RuntimeError "Could not access RGB render framebuffer"

Local animation:TEmbeddedSpriteAnimation = VisualBotRunAnimation()
If Not animation Then RuntimeError "Could not load generated sprite animation"
Local leftRunner:TEmbeddedSpritePlayer = TEmbeddedSpritePlayer.Create(animation)
Local rightRunner:TEmbeddedSpritePlayer = TEmbeddedSpritePlayer.Create(animation)
rightRunner.Advance(270)

SetRenderTarget frame, display
SetFont Terminus10x20BoldFont()

Const Background:UInt = $060c1b
Const Lane:UInt = $183b55
SetClsColorValue Background
Cls
SetColor 238, 244, 255
DrawText "PACKED SPRITES / LIVE ATLAS", 24, 18
SetColor 105, 135, 175
DrawText "6 trimmed frames - stable 128x128 canvas - dirty presentation", 24, 45
SetColorValue Lane
FillRoundRect 20, 112, 760, 184, 24
SetColor 71, 116, 148
DrawLine 30, 269, 748, 269
SetColor 238, 244, 255
DrawText "TRANSPARENT RGB565 + MASK", 24, 328
SetColor 105, 135, 175
DrawText "The yellow guide stays locked to the logical canvas", 24, 358
DrawText "Two distant dirty regions are presented as one frame", 24, 388
If Present() <= 0 Then RuntimeError "Could not present initial sprite frame"
SetDirtyTracking True

Local leftPosition:Long = 30 * 1000
Local leftDirection:Int = 1
Local leftPreviousX:Int = 30
Local rightPosition:Long = 620 * 1000
Local rightDirection:Int = -1
Local rightPreviousX:Int = 620
Local lastTime:ULong = MonotonicMilliseconds()
Local measuredFPS:Int
Local measurementFrames:Int
Local measurementStarted:ULong = lastTime
Local waitMicroseconds:ULong
Local renderMicroseconds:ULong
Local presentMicroseconds:ULong
Local presentedRegions:ULong

While True
	Local frameStarted:ULong = MonotonicMicroseconds()
	Local now:ULong = MonotonicMilliseconds()
	Local delta:Int = Int(now - lastTime)
	If delta > 50 Then delta = 50
	lastTime = now
	leftRunner.Advance(delta)
	rightRunner.Advance(delta)

	leftPosition :+ Long(leftDirection) * 150 * delta
	If leftPosition >= 250000 Then
		leftPosition = 250000
		leftDirection = -1
	Else If leftPosition <= 30000 Then
		leftPosition = 30000
		leftDirection = 1
	End If
	Local leftX:Int = Int(leftPosition / 1000)
	rightPosition :+ Long(rightDirection) * 150 * delta
	If rightPosition >= 620000 Then
		rightPosition = 620000
		rightDirection = -1
	Else If rightPosition <= 420000 Then
		rightPosition = 420000
		rightDirection = 1
	End If
	Local rightX:Int = Int(rightPosition / 1000)

	If display.HasCapability(DisplayCapabilityLiveFramebuffer) And ..
			Not display.WaitVSync() Then RuntimeError "Timed out waiting for RGB VSYNC"
	Local waited:ULong = MonotonicMicroseconds()
	Local dirtyLeft:Int = leftPreviousX
	If leftX < dirtyLeft Then dirtyLeft = leftX
	Local dirtyRight:Int = leftPreviousX
	If leftX > dirtyRight Then dirtyRight = leftX
	dirtyRight :+ 128
	SetColorValue Lane
	FillRect dirtyLeft, 120, dirtyRight - dirtyLeft, 156
	SetColor 74, 112, 145
	DrawLine dirtyLeft, 269, dirtyRight - 1, 269
	SetColor 255, 218, 78
	DrawLine leftX + 64, 124, leftX + 64, 274
	SetColor 255, 255, 255
	leftRunner.Draw Null, leftX, 132

	dirtyLeft = rightPreviousX
	If rightX < dirtyLeft Then dirtyLeft = rightX
	dirtyRight = rightPreviousX
	If rightX > dirtyRight Then dirtyRight = rightX
	dirtyRight :+ 128
	SetColorValue Lane
	FillRect dirtyLeft, 120, dirtyRight - dirtyLeft, 156
	SetColor 74, 112, 145
	DrawLine dirtyLeft, 269, dirtyRight - 1, 269
	SetColor 255, 218, 78
	DrawLine rightX + 64, 124, rightX + 64, 274
	SetColor 255, 255, 255
	rightRunner.Draw Null, rightX, 132, ImageFlipX
	Local rendered:ULong = MonotonicMicroseconds()

	presentedRegions :+ ULong(DirtyRegionCount())
	If PresentDirty() <= 0 Then RuntimeError "Could not present sprite frame"
	Local presented:ULong = MonotonicMicroseconds()
	waitMicroseconds :+ waited - frameStarted
	renderMicroseconds :+ rendered - waited
	presentMicroseconds :+ presented - rendered
	leftPreviousX = leftX
	rightPreviousX = rightX
	measurementFrames :+ 1
	Local elapsed:ULong = now - measurementStarted
	If elapsed >= 1000 Then
		measuredFPS = Int((Long(measurementFrames) * 1000) / Long(elapsed))
		Print "Sprite atlas visual demo: " + measuredFPS + " FPS, regions " + ..
			Int(Long(presentedRegions) / measurementFrames) + ", wait " + ..
			Int(Long(waitMicroseconds) / measurementFrames) + " us, render " + ..
			Int(Long(renderMicroseconds) / measurementFrames) + " us, present " + ..
			Int(Long(presentMicroseconds) / measurementFrames) + " us"
		measurementFrames = 0
		waitMicroseconds = 0
		renderMicroseconds = 0
		presentMicroseconds = 0
		presentedRegions = 0
		measurementStarted = now
	End If
	Delay 0
Wend
