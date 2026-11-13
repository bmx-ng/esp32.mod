SuperStrict

Framework BRL.StandardIO
Import BRL.System
Import BRL.SystemDefault
Import Embedded.Drivers.Display.Fonts.Spleen.Size5x8
Import Embedded.UI
Import ESP32.Drivers.Displays.RGB
Import ESP32.Drivers.Touch.GT1151

Global UI:TUIContext
Global LoadGauge:TUIArcGauge
Global SetPoint:TUIRotaryDial
Global History:TUISparkline
Global LinkState:TUIStatusIndicator
Global SensorState:TUIStatusIndicator
Global Status:TUILabel
Global SamplePhase:Int
Global LastSample:UInt

Function SetPointChanged(dial:TUIRotaryDial)
	LoadGauge.SetValue(dial.Value())
	Status.SetText("Set point: " + dial.Value() + "%")
End Function

Function UpdateDashboard()
	Local now:UInt = UInt(MilliSecs())
	If now - LastSample < 120 Then Return
	LastSample = now
	SamplePhase = (SamplePhase + 3) Mod 200
	Local triangle:Int = SamplePhase
	If triangle > 100 Then triangle = 200 - triangle
	Local sample:Int = 20 + triangle * 3 / 5
	History.AddValue(sample)
	LinkState.SetActive((SamplePhase / 25) Mod 2 = 0)
	SensorState.SetActive(sample < 70)
End Function

Local displayConfig:TESP32RGBDisplayConfig = ESP32S31Korvo1ST7262Config()
displayConfig.framebufferCount = 3
Local display:TESP32RGBDisplay = TESP32RGBDisplay.Create(displayConfig)
If Not display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()

Local touchConfig:TGT1151Config = ESP32S31Korvo1GT1151Config()
touchConfig.orientation = display.Orientation()
Local touch:TGT1151Touch = TGT1151Touch.Create(touchConfig)
If Not touch Then RuntimeError "Could not initialize GT1151 touch: " + GT1151LastError()
If Not touch.StartEvents() Then RuntimeError "Could not start GT1151 touch events"

Local theme:TUITheme = TUITheme.Dark(Spleen5x8Font())
theme.SetFont(Spleen5x8Font(), 3)
theme.screen.background = $0b1119
theme.progress.background = $26323d
theme.progress.border = $71818e
theme.progressFill.background = $20b8ea
theme.controlActive.background = $20b8ea
theme.controlActive.foreground = $eaffff

UI = TUIContext.Create(display, touch, theme)
If Not UI Then RuntimeError "Could not create embedded UI"

Local page:TUIColumn = UI.Root()
page.SetPaddingEdges(28, 18, 28, 18)
page.SetGap(10)

Local title:TUILabel = page.Label("Device dashboard")
title.Style().fontScale = 4
title.Style().foreground = $5ccfff
Local stateRow:TUIRow = page.Row()
stateRow.SetGap(16)
LinkState = stateRow.StatusIndicator("Network", True)
SensorState = stateRow.StatusIndicator("Sensor", True)
Local storage:TUIStatusIndicator = stateRow.StatusIndicator("Storage", True)

Local content:TUIRow = page.Row()
content.SetGrow(1)
content.SetGap(20)

Local gaugePanel:TUIColumn = content.Column()
gaugePanel.SetGrow(1)
gaugePanel.SetGap(3)
Local gaugeTitle:TUILabel = gaugePanel.Label("SYSTEM LOAD")
gaugeTitle.Style().foreground = $ffd84a
LoadGauge = gaugePanel.ArcGauge(0, 100, 48)
LoadGauge.SetGrow(1)
LoadGauge.SetLabel("Current")
LoadGauge.SetUnit("%")
LoadGauge.SetArcWidth(8)
LoadGauge.SetTickCount(10)

Local historyPanel:TUIColumn = content.Column()
historyPanel.SetGrow(2)
historyPanel.SetGap(8)
Local historyTitle:TUILabel = historyPanel.Label("RECENT SAMPLES")
historyTitle.Style().foreground = $ffd84a
History = historyPanel.Sparkline(64)
History.SetRange(0, 100)
History.SetGrow(1)
For Local seed:Int = 0 Until 32
	History.AddValue(35 + (seed Mod 12) * 3)
Next
Local note:TUILabel = historyPanel.Label("Fixed-capacity history; no allocation while sampling")
note.SetWordWrap(True)

Local dialPanel:TUIColumn = content.Column()
dialPanel.SetGrow(1)
dialPanel.SetGap(3)
Local dialTitle:TUILabel = dialPanel.Label("SET POINT")
dialTitle.Style().foreground = $ffd84a
SetPoint = dialPanel.RotaryDial(0, 100, 48)
SetPoint.SetGrow(1)
SetPoint.SetStep(5)
SetPoint.OnChange(SetPointChanged)
SetPoint.SetFocusIndicatorVisible(False)
Local hint:TUILabel = dialPanel.Label("Drag left~nor right")
hint.Style().foreground = $9bd5ff

Status = page.Label("Dashboard samples update continuously")
Status.Style().foreground = $ffd84a

If Not UI.Present() Then RuntimeError "Could not present embedded UI"
Print "Embedded.UI dashboard example active"

While True
	PollSystem()
	UpdateDashboard()
	UI.Present()
	Delay(5)
Wend
