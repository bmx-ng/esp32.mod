SuperStrict

Import BRL.StandardIO
Import BRL.FileSystem
Import BRL.TextStream
Import ESP32.Storage.LittleFS
Import ESP32.Storage.SDCard

If Not LittleFSIsMounted() Then Throw "LittleFS did not mount (error " + LittleFSLastError() + ")"
If DefaultStorageVolume() <> "littlefs" Then Throw "LittleFS is not the deterministic default volume"

SaveText("internal flash", "storage-smoke.txt")
If LoadText("littlefs::storage-smoke.txt") <> "internal flash" Then Throw "LittleFS protocol routing failed"
If LoadText("file::storage-smoke.txt") <> "internal flash" Then Throw "default file protocol routing failed"
Print "LittleFS: " + LittleFSUsed() + " / " + LittleFSCapacity() + " bytes used"

If MountSDCard()
	SaveText("sd card", "sd::storage-smoke.txt")
	If LoadText("sd::storage-smoke.txt") <> "sd card" Then Throw "SD protocol routing failed"
	If LoadText("storage-smoke.txt") <> "internal flash" Then Throw "mounting SD changed the plain-path default"
	If Not CopyFile("littlefs::storage-smoke.txt", "sd::copied-from-flash.txt") Then Throw "cross-volume copy failed"
	If LoadText("sd::copied-from-flash.txt") <> "internal flash" Then Throw "cross-volume copy contents differ"
	If RenameFile("littlefs::storage-smoke.txt", "sd::renamed-across-volumes.txt") Then Throw "cross-volume rename was not rejected"
	If Not SetDefaultStorageVolume("sd") Then Throw "could not select SD as the default volume"
	SaveText("plain path on sd", "default-storage-smoke.txt")
	If LoadText("sd::default-storage-smoke.txt") <> "plain path on sd" Then Throw "plain paths did not follow the selected SD default"
	If LoadText("file::default-storage-smoke.txt") <> "plain path on sd" Then Throw "file protocol did not follow the selected SD default"
	If Not SetDefaultStorageVolume("littlefs") Then Throw "could not restore LittleFS as the default volume"
	Print "SD card: " + SDCardFree() + " / " + SDCardCapacity() + " bytes free"
	Print "Storage smoke test passed on LittleFS and SD"
Else
	Print "Storage smoke test passed on LittleFS; no SD card mounted (error " + SDCardLastError() + ")"
End If

While True
	Delay 2000
	Print "Storage smoke test alive; default=" + DefaultStorageVolume() + ..
		", sd=" + SDCardIsMounted() + ", sdError=" + SDCardLastError() + ..
		", sdFree=" + SDCardFree()
Wend
