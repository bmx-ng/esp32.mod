' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Framework BRL.StandardIO
Import BRL.Stream
Import ESP32.Storage.LittleFS
Import ESP32.Storage.SDCard
Import ESP32.System.OTA

' This may also be sd::firmware.bin, littlefs::firmware.bin, or a plain path
' resolved through the selected default ESP32 storage volume.
Const FirmwarePath:String = "sd::firmware.bin"

Local source:TStream = ReadStream(FirmwarePath)
If Not source Then RuntimeError "Could not open " + FirmwarePath

Local imageSize:Long = StreamSize(source)
If imageSize <= 0 Or imageSize > $ffffffff:Long Then RuntimeError "Invalid firmware image size"

Local update:TESP32OTAUpdate = BeginOTAUpdate(UInt(imageSize))
If Not update Then RuntimeError "Could not start update: " + OTAResultName(OTALastBeginError())

Print "Writing " + imageSize + " bytes to " + update.TargetPartition()
Local result:Int = update.WriteStream(source, imageSize)
CloseStream source
If result <> 0 Then
	update.Abort()
	RuntimeError "Firmware read/write failed: " + OTAResultName(result)
End If

result = update.Finish()
If result <> 0 Then RuntimeError "Firmware validation failed: " + OTAResultName(result)

' Boot selection is deliberately separate from Finish so an application can
' perform any additional checks before making the new image active.
result = update.Activate()
If result <> 0 Then RuntimeError "Could not select firmware: " + OTAResultName(result)

Print "Validated firmware selected for next reboot."
Print "Running now: " + OTARunningPartition() + "; next boot: " + OTABootPartition()
update.Close()
