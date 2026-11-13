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
Global NoticeNumber:Int
Global Dialog:TUIDialog
Global BurstActive:Int
Global BurstStage:Int
Global BurstLast:UInt

Function ShowInfo(button:TUIButton)
	UI.Toast("Network scan started", 2500, UIToastInfo)
	Status.SetText("Information notification shown")
End Function

Function ShowSuccess(button:TUIButton)
	UI.Toast("Settings saved successfully", 2500, UIToastSuccess)
	Status.SetText("Success notification shown")
End Function

Function ShowWarning(button:TUIButton)
	UI.Toast("Signal is weak; move closer to the access point", 3500, UIToastWarning)
	Status.SetText("Wrapped warning notification shown")
End Function

Function ShowError(button:TUIButton)
	UI.Toast("Connection failed", 3000, UIToastError)
	Status.SetText("Error notification shown")
End Function

Function ShowBurst(button:TUIButton)
	NoticeNumber :+ 1
	BurstActive = True
	BurstStage = 0
	BurstLast = UInt(MilliSecs()) - 500
	Status.SetText("Adding one notification every half-second")
End Function

Function UpdateBurst()
	If Not BurstActive Then Return
	Local now:UInt = UInt(MilliSecs())
	If now - BurstLast < 500 Then Return
	Select BurstStage
		Case 0
			UI.Toast("Job " + NoticeNumber + " started", 2000, UIToastInfo)
		Case 1
			UI.Toast("Job " + NoticeNumber + " is running", 2000, UIToastSuccess)
		Case 2
			UI.Toast("Job " + NoticeNumber + " needs attention", 2000, UIToastWarning)
	End Select
	BurstStage :+ 1
	BurstLast = now
	If BurstStage >= 3 Then
		BurstActive = False
		Status.SetText("Notifications expire and close the stack in order")
	End If
End Function

Function ShowPersistent(button:TUIButton)
	UI.Toast("Offline mode is active. Dismiss this notification explicitly.", 0, UIToastWarning)
	Status.SetText("Persistent notification shown")
End Function

Function DismissNotice(button:TUIButton)
	If UI.DismissToast() Then
		Status.SetText("Current notification dismissed")
	Else
		Status.SetText("No notification to dismiss")
	End If
End Function

Function ShowDialog(button:TUIButton)
	Dialog.Show(430)
	UI.Toast("Toast remains visible above the modal dialog", 3000, UIToastInfo)
End Function

Function DialogClosed(dialog:TUIDialog, result:Int)
	Status.SetText("Dialog closed; controls retained focus semantics")
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
theme.button.SetPaddingEdges(14, 10, 14, 10)
theme.buttonPressed.SetPaddingEdges(14, 10, 14, 10)
theme.buttonDisabled.SetPaddingEdges(14, 10, 14, 10)

UI = TUIContext.Create(display, touch, theme)
If Not UI Then RuntimeError "Could not create embedded UI"
UI.SetToastPlacement(UIToastBottomCenter)
UI.SetToastVisibleLimit(3)
UI.SetToastGap(8)

Local page:TUIColumn = UI.Root()
page.SetPaddingEdges(30, 22, 30, 22)
page.SetGap(14)

Local title:TUILabel = page.Label("Notifications")
title.Style().fontScale = 5
title.Style().foreground = $5ccfff
Local introduction:TUILabel = page.Label("Non-modal messages stay out of the way of touch and focus")
introduction.SetWordWrap(True)

Local severityRow:TUIRow = page.Row()
severityRow.SetGap(12)
Local info:TUIButton = severityRow.Button("Info")
Local success:TUIButton = severityRow.Button("Success")
Local warning:TUIButton = severityRow.Button("Warning")
Local failure:TUIButton = severityRow.Button("Error")
info.SetGrow(1)
success.SetGrow(1)
warning.SetGrow(1)
failure.SetGrow(1)
info.OnClick(ShowInfo)
success.OnClick(ShowSuccess)
warning.OnClick(ShowWarning)
failure.OnClick(ShowError)

Local behaviorRow:TUIRow = page.Row()
behaviorRow.SetGap(12)
Local burst:TUIButton = behaviorRow.Button("Stagger")
Local persistent:TUIButton = behaviorRow.Button("Persistent")
Local dismiss:TUIButton = behaviorRow.Button("Dismiss")
Local modal:TUIButton = behaviorRow.Button("With dialog")
burst.SetGrow(1)
persistent.SetGrow(1)
dismiss.SetGrow(1)
modal.SetGrow(1)
burst.OnClick(ShowBurst)
persistent.OnClick(ShowPersistent)
dismiss.OnClick(DismissNotice)
modal.OnClick(ShowDialog)

Local spacer:TUIContainer = page.Column()
spacer.SetGrow(1)
Status = page.Label("Ready")
Status.Style().foreground = $ffd84a

Dialog = UI.Dialog("Modal coexistence", "The notification is drawn above this dialog without becoming part of its touch or focus tree.")
Dialog.TitleLabel().Style().fontScale = 4
Dialog.TitleLabel().Style().foreground = $5ccfff
Dialog.Action("Close", 0)
Dialog.OnResult(DialogClosed)

If Not UI.Present() Then RuntimeError "Could not present embedded UI"
Print "Embedded.UI notifications example active"

While True
	PollSystem()
	UpdateBurst()
	UI.Present()
	Delay(5)
Wend
