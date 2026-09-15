SuperStrict

Import BRL.StandardIO
Import ESP32.Hardware.Watchdog
Import ESP32.System.Device

If DeviceResetReason() <> DeviceResetReasonSoftware Then
	Print "Requesting a software reset"
	Reboot(25)
End If

Local identifier:String = UniqueDeviceID()
Local identifierBytes:Byte[] = UniqueDeviceIDBytes()
Local passed:Int = identifier.length = 12 And identifierBytes.length = 6
passed :& ChipModelName().length > 0 And ChipModel() > 0
passed :& ChipCoreCount() > 0 And DeviceResetReasonNative() >= 0
passed :& DeviceResetReason() = DeviceResetReasonSoftware
passed :& Not WatchdogEnable(0) And WatchdogEnable(250)
passed :& WatchdogIsEnabled() And WatchdogFeed()
passed :& WatchdogDisable() And Not WatchdogIsEnabled()

If passed Then
	Print "ESP32 watchdog and device checks passed"
Else
	Print "ESP32 watchdog and device checks failed"
End If
