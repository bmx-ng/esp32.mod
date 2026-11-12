SuperStrict

Framework BRL.StandardIO
Import BRL.System
Import BRL.SystemDefault
Import Embedded.Drivers.Display.Fonts.Spleen.Size5x8
Import Embedded.UI
Import ESP32.Drivers.Displays.RGB
Import ESP32.Drivers.Touch.GT1151

Global UI:TUIContext
Global Mode:TUIComboBox
Global Status:TUILabel
Global ActionButton:TUIButton
Global ActionMenu:TUIPopupMenu

Function ModeChanged(combo:TUIComboBox)
	Status.SetText("Mode: " + combo.SelectedText())
End Function

Function ShowActions(button:TUIButton)
	UI.ShowOverlay(ActionMenu, button.X(), button.Y() + button.Height(), button.Width())
End Function

Function ActionSelected(list:TUIList)
	Local item:TUIListItem = list.SelectedItem()
	If item Then Status.SetText("Action: " + item.Text())
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
theme.button.SetPaddingEdges(16, 10, 16, 10)
theme.buttonPressed.SetPaddingEdges(16, 10, 16, 10)
theme.buttonDisabled.SetPaddingEdges(16, 10, 16, 10)

UI = TUIContext.Create(display, touch, theme)
If Not UI Then RuntimeError "Could not create embedded UI"

Local page:TUIColumn = UI.Root()
page.SetPaddingEdges(30, 22, 30, 24)
page.SetGap(12)

Local title:TUILabel = page.Label("Embedded.UI navigation")
title.Style().fontScale = 5
title.Style().foreground = $5ccfff

Local introduction:TUILabel = page.Label("Tabs retain their pages. Menus and combo boxes use overlays.")
introduction.Style().foreground = $b9c8d6

Local tabs:TUITabView = page.TabView()
tabs.SetGrow(1)
tabs.SetGap(8)

Local general:TUITabPage = tabs.Page("General")
general.SetPaddingEdges(12, 16, 12, 8)
general.SetGap(12)
general.Label("Connection mode")
Mode = general.ComboBox()
Mode.AddItem("Automatic")
Mode.AddItem("Station")
Mode.AddItem("Access point")
Mode.SetSelectedIndex(0)
Mode.OnChange(ModeChanged)
Status = general.Label("Mode: Automatic")
Status.Style().foreground = $ffd84a

Local network:TUITabPage = tabs.Page("Network")
network.SetPaddingEdges(12, 16, 12, 8)
network.SetGap(14)
network.Label("Network services")
network.CheckBox("Start Wi-Fi automatically", True)
Local serviceRow:TUIRow = network.Row()
serviceRow.SetGap(12)
serviceRow.Label("Discovery")
serviceRow.Switch(True)

Local actions:TUITabPage = tabs.Page("Actions")
actions.SetPaddingEdges(12, 16, 12, 8)
actions.SetGap(14)
actions.Label("A reusable popup menu is anchored to this button.")
ActionButton = actions.Button("Open actions")
ActionButton.OnClick(ShowActions)

ActionMenu = UI.PopupMenu()
ActionMenu.SetGap(4)
ActionMenu.SetPadding(5)
ActionMenu.Item("Restart service")
ActionMenu.Item("Save settings")
ActionMenu.Item("Restore defaults")
ActionMenu.OnSelectionChanged(ActionSelected)

If Not UI.Present() Then RuntimeError "Could not present embedded UI"
Print "Embedded.UI tabs and overlays example active"

While True
	PollSystem()
	UI.Present()
	Delay(5)
Wend
