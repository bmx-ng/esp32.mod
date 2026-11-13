SuperStrict

' This example retains five complete pages. On PSRAM-equipped S31 boards use:
' bmk makeapp -a -r -board esp32_s31_korvo_1 -heap-region psram -heap 512k -x rgb_lcd_ui_pages.bmx

Framework BRL.StandardIO
Import BRL.System
Import BRL.SystemDefault
Import Embedded.Drivers.Display.Fonts.Spleen.Size5x8
Import Embedded.UI
Import Embedded.UI.Interactive
Import ESP32.Drivers.Displays.RGB
Import ESP32.Drivers.Touch.GT1151

Global UI:TUIContext
Global Navigation:TUIPageStack
Global HomePage:TUIPage
Global SettingsPage:TUIPage
Global DisplayPage:TUIPage
Global DiagnosticsPage:TUIPage
Global ResultsPage:TUIPage
Global Brightness:TUISlider
Global SettingsSummary:TUILabel

Function OpenSettings(button:TUIButton)
	Navigation.Push(SettingsPage)
End Function

Function OpenDisplay(button:TUIButton)
	Navigation.Push(DisplayPage)
End Function

Function OpenDiagnostics(button:TUIButton)
	Navigation.Push(DiagnosticsPage)
End Function

Function ShowResults(button:TUIButton)
	Navigation.Replace(ResultsPage)
End Function

Function GoBack(button:TUIButton)
	UI.Back()
End Function

Function BrightnessChanged(slider:TUISlider)
	SettingsSummary.SetText("Brightness retained at " + slider.Value() + "%")
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
theme.SetFont(Spleen5x8Font(), 2)
theme.SetButtonMinimumHeight(52)
theme.SetSelectionMinimumHeight(52)
theme.screen.background = $0b1119
theme.button.background = $173451
theme.buttonPressed.background = $20b8ea
theme.controlActive.background = $20b8ea
theme.controlActive.foreground = $eaffff

UI = TUITouchInput.CreateContext(display, touch, theme)
If Not UI Then RuntimeError "Could not create embedded UI"

Local root:TUIColumn = UI.Root()
root.SetPaddingEdges(28, 18, 28, 18)
Navigation = root.PageStack()
Navigation.SetGrow(1)

' Home is the first page, so it becomes the stack root automatically.
HomePage = Navigation.Page("Home")
HomePage.SetGap(12)
Local homeHeader:TUIHeader = HomePage.Header("Device dashboard")
homeHeader.TitleLabel().Style().fontScale = 4
homeHeader.TitleLabel().Style().foreground = $5ccfff
homeHeader.Action("Settings", OpenSettings)

Local dashboard:TUIGrid = HomePage.Grid(3)
dashboard.SetGrow(1)
dashboard.SetGap(12)
dashboard.SetRowHeight(122)

Local systemCard:TUICard = dashboard.Card("System")
systemCard.TitleLabel().Style().foreground = $5ccfff
systemCard.StatusIndicator("Display online", True)
systemCard.StatusIndicator("Touch online", True)

Local networkCard:TUICard = dashboard.Card("Network")
networkCard.TitleLabel().Style().foreground = $5ccfff
Local networkState:TUILabel = networkCard.Label("Connected")
networkState.Style().foreground = $55e88b
networkCard.Label("LAN  192.168.1.42")

Local loadCard:TUICard = dashboard.Card("Activity")
loadCard.TitleLabel().Style().foreground = $5ccfff
loadCard.ProgressBar(0, 100, 68)
loadCard.Label("Load  68%")

Local quickCard:TUICard = dashboard.Card("Quick actions")
quickCard.SetGridSpan(2)
quickCard.TitleLabel().Style().foreground = $5ccfff
Local quickActions:TUIRow = quickCard.Row()
quickActions.SetGap(10)
Local settingsButton:TUIButton = quickActions.Button("Settings")
settingsButton.SetGrow(1)
settingsButton.OnClick(OpenSettings)
Local diagnosticsButton:TUIButton = quickActions.Button("Diagnostics")
diagnosticsButton.SetGrow(1)
diagnosticsButton.OnClick(OpenDiagnostics)

Local storageCard:TUICard = dashboard.Card("Storage")
storageCard.TitleLabel().Style().foreground = $5ccfff
storageCard.Label("SD card ready")
storageCard.ProgressBar(0, 100, 34)

Local homeNav:TUINavigationBar = HomePage.NavigationBar()
homeNav.Action("Home")
homeNav.Action("Settings", OpenSettings)
homeNav.Action("Diagnostics", OpenDiagnostics)

SettingsPage = Navigation.Page("Settings")
SettingsPage.SetGap(12)
Local settingsHeader:TUIRow = SettingsPage.Row()
settingsHeader.SetGap(14)
Local settingsBack:TUIButton = settingsHeader.Button("< Back")
settingsBack.SetWidth(150)
settingsBack.OnClick(GoBack)
Local settingsTitle:TUILabel = settingsHeader.Label("Settings")
settingsTitle.Style().fontScale = 4
settingsTitle.Style().foreground = $5ccfff
SettingsSummary = SettingsPage.Label("Brightness retained at 65%")
SettingsSummary.Style().foreground = $ffd84a
Brightness = SettingsPage.Slider(0, 100, 65)
Brightness.SetHeight(70)
Brightness.OnChange(BrightnessChanged)
Local displayButton:TUIButton = SettingsPage.Button("Display options  >")
displayButton.SetHeight(74)
displayButton.OnClick(OpenDisplay)
Local settingsNote:TUILabel = SettingsPage.Label("Adjust the slider, visit Display options, and come back. The value remains.")
settingsNote.SetWordWrap(True)

DisplayPage = Navigation.Page("Display")
DisplayPage.SetGap(12)
Local displayHeader:TUIRow = DisplayPage.Row()
displayHeader.SetGap(14)
Local displayBack:TUIButton = displayHeader.Button("< Back")
displayBack.SetWidth(150)
displayBack.OnClick(GoBack)
Local displayTitle:TUILabel = displayHeader.Label("Display options")
displayTitle.Style().fontScale = 4
displayTitle.Style().foreground = $5ccfff
Local orientation:TUIComboBox = DisplayPage.ComboBox()
orientation.AddItem("Landscape")
orientation.AddItem("Portrait")
orientation.AddItem("Landscape inverted")
orientation.SetSelectedIndex(0)
Local automaticRow:TUIRow = DisplayPage.Row()
automaticRow.SetGap(16)
Local automaticLabel:TUILabel = automaticRow.Label("Automatic orientation")
automaticLabel.SetAlign(UIAlignCenter)
Local displaySwitch:TUISwitch = automaticRow.Switch(True)
Local displayNote:TUILabel = DisplayPage.Label("Back returns to Settings and restores its focused control.")
displayNote.SetWordWrap(True)

DiagnosticsPage = Navigation.Page("Diagnostics")
DiagnosticsPage.SetGap(12)
Local diagnosticsHeader:TUIRow = DiagnosticsPage.Row()
diagnosticsHeader.SetGap(14)
Local diagnosticsBack:TUIButton = diagnosticsHeader.Button("< Back")
diagnosticsBack.SetWidth(150)
diagnosticsBack.OnClick(GoBack)
Local diagnosticsTitle:TUILabel = diagnosticsHeader.Label("Diagnostics")
diagnosticsTitle.Style().fontScale = 4
diagnosticsTitle.Style().foreground = $5ccfff
DiagnosticsPage.StatusIndicator("Display online", True)
DiagnosticsPage.StatusIndicator("Touch online", True)
DiagnosticsPage.StatusIndicator("Storage ready", True)
Local runButton:TUIButton = DiagnosticsPage.Button("Run check")
runButton.SetHeight(74)
runButton.OnClick(ShowResults)

ResultsPage = Navigation.Page("Results")
ResultsPage.SetGap(14)
Local resultsTitle:TUILabel = ResultsPage.Label("All checks passed")
resultsTitle.Style().fontScale = 4
resultsTitle.Style().foreground = $55e88b
Local resultsCopy:TUILabel = ResultsPage.Label("This page replaced Diagnostics without adding another history entry.")
resultsCopy.SetWordWrap(True)
Local resultsBack:TUIButton = ResultsPage.Button("Return home")
resultsBack.SetHeight(74)
resultsBack.OnClick(GoBack)

If Not UI.Present() Then RuntimeError "Could not present embedded UI"
Print "Embedded.UI page navigation example active"

While True
	PollSystem()
	If UI.Present() <= 0 Then RuntimeError "Could not present embedded UI frame"
	Delay(5)
Wend
