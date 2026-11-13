SuperStrict

Framework BRL.StandardIO
Import BRL.System
Import BRL.SystemDefault
Import Embedded.Drivers.Display.Fonts.Spleen.Size5x8
Import Embedded.UI
Import Embedded.UI.Interactive
Import ESP32.Drivers.Displays.RGB
Import ESP32.Drivers.Touch.GT1151

Global UI:TUIContext
Global Status:TUILabel
Global StartButton:TUIButton
Global PauseButton:TUIButton
Global ResetButton:TUIButton
Global AlertsCheck:TUICheckBox
Global LiveSwitch:TUISwitch
Global LevelSlider:TUISlider
Global LevelProgress:TUIProgressBar
Global ActionCount:Int

Function Started(button:TUIButton)
	ActionCount :+ 1
	Status.SetText("Running  -  action " + ActionCount)
	StartButton.SetEnabled(False)
	PauseButton.SetEnabled(True)
End Function

Function Paused(button:TUIButton)
	Status.SetText("Paused")
	StartButton.SetEnabled(True)
	PauseButton.SetEnabled(False)
End Function

Function ResetState(button:TUIButton)
	ActionCount = 0
	Status.SetText("Ready")
	StartButton.SetEnabled(True)
	PauseButton.SetEnabled(False)
End Function

Function AlertsChanged(checkBox:TUICheckBox)
	If checkBox.Checked() Then Status.SetText("Alerts enabled") Else Status.SetText("Alerts disabled")
End Function

Function LiveChanged(switchControl:TUISwitch)
	If switchControl.Value() Then Status.SetText("Live mode on") Else Status.SetText("Live mode off")
End Function

Function LevelChanged(slider:TUISlider)
	LevelProgress.SetValue(slider.Value())
	Status.SetText("Level " + slider.Value())
End Function

Local display:TESP32RGBDisplay = TESP32RGBDisplay.Create(ESP32S31Korvo1ST7262Config())
If Not display Then RuntimeError "Could not initialize RGB display: " + RGBDisplayLastError()

Local touchConfig:TGT1151Config = ESP32S31Korvo1GT1151Config()
touchConfig.orientation = display.Orientation()
Local touch:TGT1151Touch = TGT1151Touch.Create(touchConfig)
If Not touch Then RuntimeError "Could not initialize GT1151 touch: " + GT1151LastError()
If Not touch.StartEvents() Then RuntimeError "Could not start GT1151 touch events"

Local theme:TUITheme = TUITheme.Dark(Spleen5x8Font())
theme.SetFont(Spleen5x8Font(), 3)
theme.screen.background = $0b1119
theme.button.SetPaddingEdges(18, 13, 18, 13)
theme.buttonPressed.SetPaddingEdges(18, 13, 18, 13)
theme.buttonDisabled.SetPaddingEdges(18, 13, 18, 13)

UI = TUITouchInput.CreateContext(display, touch, theme)
If Not UI Then RuntimeError "Could not create embedded UI"

Local page:TUIColumn = UI.Root()
page.SetPaddingEdges(38, 32, 38, 34)
page.SetGap(18)

Local title:TUILabel = page.Label("Embedded.UI")
title.Style().fontScale = 6
title.Style().foreground = $5ccfff

Local introduction:TUILabel = page.Label("A small retained interface, drawn by BlitzMax")
introduction.Style().fontScale = 3
introduction.Style().foreground = $b9c8d6

Status = page.Label("Ready")
Status.Style().fontScale = 4
Status.Style().foreground = $ffd84a
Status.SetGrow(1)

Local options:TUIRow = page.Row()
options.SetGap(24)
AlertsCheck = options.CheckBox("Alerts", True)
AlertsCheck.OnChange(AlertsChanged)
Local liveLabel:TUILabel = options.Label("Live")
liveLabel.SetAlign(UIAlignCenter)
LiveSwitch = options.Switch(False)
LiveSwitch.OnChange(LiveChanged)

Local level:TUIRow = page.Row()
level.SetGap(18)
Local levelLabel:TUILabel = level.Label("Level")
levelLabel.SetAlign(UIAlignCenter)
LevelSlider = level.Slider(0, 100, 35)
LevelSlider.SetGrow(1)
LevelSlider.OnChange(LevelChanged)
LevelProgress = level.ProgressBar(0, 100, 35)
LevelProgress.SetWidth(150)
LevelProgress.SetAlign(UIAlignCenter)

Local actions:TUIRow = page.Row()
actions.SetGap(16)
StartButton = actions.Button("Start")
PauseButton = actions.Button("Pause")
ResetButton = actions.Button("Reset")
StartButton.SetGrow(1)
PauseButton.SetGrow(1)
ResetButton.SetGrow(1)
StartButton.OnClick(Started)
PauseButton.OnClick(Paused)
ResetButton.OnClick(ResetState)
PauseButton.SetEnabled(False)
UI.SetFocus(StartButton)

If Not UI.Present() Then RuntimeError "Could not present embedded UI"
Print "Embedded.UI touch example active"

While True
	PollSystem()
	UI.Present()
	Delay(5)
Wend
