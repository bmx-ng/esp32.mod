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
Global DeviceList:TUIList
Global Selection:TUILabel

Function UpdateSelection()
	Local item:TUIListItem = DeviceList.SelectedItem()
	If item Then
		Selection.SetText("Selected~n" + item.Text())
	Else
		Selection.SetText("Nothing selected")
	End If
End Function

Function DeviceSelected(list:TUIList)
	UpdateSelection()
End Function

Function PreviousItem(button:TUIButton)
	DeviceList.MoveSelection(-1)
	UpdateSelection()
End Function

Function SelectNextItem(button:TUIButton)
	DeviceList.MoveSelection(1)
	UpdateSelection()
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
theme.control.SetPaddingEdges(14, 10, 14, 10)
theme.controlActive.SetPaddingEdges(14, 10, 14, 10)
theme.controlPressed.SetPaddingEdges(14, 10, 14, 10)
theme.controlDisabled.SetPaddingEdges(14, 10, 14, 10)
theme.button.SetPaddingEdges(18, 12, 18, 12)
theme.buttonPressed.SetPaddingEdges(18, 12, 18, 12)
theme.buttonDisabled.SetPaddingEdges(18, 12, 18, 12)

UI = TUITouchInput.CreateContext(display, touch, theme)
If Not UI Then RuntimeError "Could not create embedded UI"

Local page:TUIColumn = UI.Root()
page.SetPaddingEdges(34, 26, 34, 28)
page.SetGap(14)

Local title:TUILabel = page.Label("Scrolling & selection")
title.Style().fontScale = 5
title.Style().foreground = $5ccfff

Local introduction:TUILabel = page.Label("Drag or tap the list.~nUse Previous / Next to navigate.")
introduction.Style().foreground = $b9c8d6

Local content:TUIRow = page.Row()
content.SetGrow(1)
content.SetGap(22)

DeviceList = content.List()
DeviceList.SetGrow(1)
DeviceList.SetGap(5)
DeviceList.SetPadding(6)
DeviceList.SetBackgroundVisible(True)
DeviceList.Style().background = $141e28
DeviceList.Style().backgroundVisible = True
DeviceList.OnSelectionChanged(DeviceSelected)

Local names:String[] = ["Baguette S3", "XIAO ESP32-S3 Plus", "XIAO ESP32-C6", ..
	"Waveshare ESP32-H2", "XIAO ESP32-C5", "Stamp-P4", "ESP32-S31 Korvo-1", ..
	"Function CoreBoard-1", "ESP32-WROOM-32D", "Baguette C3", ..
	"TENSTAR ESP32-C6", "ESP32-C3 OLED"]
For Local name:String = EachIn names
	DeviceList.Item(name)
Next

Local details:TUIColumn = content.Column()
details.SetWidth(260)
details.SetGap(14)
Selection = details.Label("")
Selection.Style().fontScale = 4
Selection.Style().foreground = $ffd84a
Selection.SetGrow(1)

Local previous:TUIButton = details.Button("Previous")
previous.OnClick(PreviousItem)
Local nextButton:TUIButton = details.Button("Next")
nextButton.OnClick(SelectNextItem)

DeviceList.SetSelectedIndex(0)
UI.SetFocus(DeviceList.ItemAt(0))
UpdateSelection()

If Not UI.Present() Then RuntimeError "Could not present embedded UI"
Print "Embedded.UI scrolling and selection example active"

While True
	PollSystem()
	UI.Present()
	Delay(5)
Wend
