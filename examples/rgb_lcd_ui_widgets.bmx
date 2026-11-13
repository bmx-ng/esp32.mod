SuperStrict

Framework BRL.StandardIO
Import BRL.System
Import BRL.SystemDefault
Import Embedded.Drivers.Display.Fonts.Spleen.Size5x8
Import Embedded.UI
Import ESP32.Drivers.Displays.RGB
Import ESP32.Drivers.Touch.GT1151

Global UI:TUIContext
Global Status:TUILabel
Global Dialog:TUIDialog

Function QualityChanged(group:TUIRadioGroup)
	Status.SetText("Quality preset: " + group.OptionAt(group.SelectedIndex()).Text())
End Function

Function GainChanged(stepper:TUIStepper)
	Status.SetText("Gain: " + stepper.ValueText())
End Function

Function ViewChanged(control:TUISegmentedControl)
	Status.SetText("View: " + control.ItemText(control.SelectedIndex()))
End Function

Function DialogClosed(dialog:TUIDialog, result:Int)
	If result = 1 Then
		Status.SetText("Settings applied")
	Else
		Status.SetText("No changes applied")
	End If
End Function

Function ShowConfirmation(button:TUIButton)
	Dialog.Show(440)
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
theme.control.SetPaddingEdges(12, 9, 12, 9)
theme.controlActive.SetPaddingEdges(12, 9, 12, 9)
theme.controlPressed.SetPaddingEdges(12, 9, 12, 9)
theme.controlDisabled.SetPaddingEdges(12, 9, 12, 9)
theme.button.SetPaddingEdges(16, 10, 16, 10)
theme.buttonPressed.SetPaddingEdges(16, 10, 16, 10)
theme.buttonDisabled.SetPaddingEdges(16, 10, 16, 10)

UI = TUIContext.Create(display, touch, theme)
If Not UI Then RuntimeError "Could not create embedded UI"

Local page:TUIColumn = UI.Root()
page.SetPaddingEdges(30, 22, 30, 24)
page.SetGap(12)

Local title:TUILabel = page.Label("Useful widgets")
title.Style().fontScale = 5
title.Style().foreground = $5ccfff
Local introduction:TUILabel = page.Label("Retained controls with touch, focus and semantic events")
introduction.SetWordWrap(True)

Local content:TUIRow = page.Row()
content.SetGrow(1)
content.SetGap(28)

Local choices:TUIColumn = content.Column()
choices.SetGrow(1)
choices.SetGap(8)
Local qualityTitle:TUILabel = choices.Label("Quality preset")
qualityTitle.Style().foreground = $ffd84a
Local quality:TUIRadioGroup = choices.RadioGroup()
quality.SetGap(3)
quality.Option("Economy")
quality.Option("Balanced")
quality.Option("Best")
quality.SetSelectedIndex(1)
quality.OnSelectionChanged(QualityChanged)

Local values:TUIColumn = content.Column()
values.SetGrow(1)
values.SetGap(10)
Local viewTitle:TUILabel = values.Label("View")
viewTitle.Style().foreground = $ffd84a
Local view:TUISegmentedControl = values.SegmentedControl()
view.AddItem("List")
view.AddItem("Grid")
view.AddItem("Chart")
view.SetSelectedIndex(0)
view.OnSelectionChanged(ViewChanged)
values.Label("Gain")
Local gain:TUIStepper = values.DecimalStepper(0.5, 5.0, 2.5, 0.5, 1)
gain.OnChange(GainChanged)

Local footer:TUIRow = page.Row()
footer.SetGap(16)
Status = footer.Label("Ready")
Status.SetGrow(1)
Status.Style().foreground = $ffd84a
Local apply:TUIButton = footer.Button("Review changes")
apply.OnClick(ShowConfirmation)

Dialog = UI.Dialog("Apply settings?", "The selected settings will be used immediately.")
Dialog.TitleLabel().Style().fontScale = 4
Dialog.TitleLabel().Style().foreground = $5ccfff
Dialog.Action("Cancel", 0)
Dialog.Action("Apply", 1)
Dialog.OnResult(DialogClosed)

If Not UI.Present() Then RuntimeError "Could not present embedded UI"
Print "Embedded.UI useful widgets example active"

While True
	PollSystem()
	UI.Present()
	Delay(5)
Wend
