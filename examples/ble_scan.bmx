' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP32.Network.BLE

Local result:Int = BLEInitialize("BlitzMax ESP32")
If result <> 0 Then RuntimeError "BLE initialization failed: " + BLEResultName(result)
If Not BLEWaitReady() Then RuntimeError "BLE host did not become ready"

' Leave time to attach a serial monitor after a one-command upload.
Delay 2000
Print "Starting active BLE scan"
result = BLEStartScan(10000, True, True)
If result <> 0 Then RuntimeError "BLE scan failed: " + BLEResultName(result)

Local count:Int
While BLEScanActive()
	Select WaitEvent()
		Case EVENT_BLESCANRESULT
			Local advertisement:TBLEAdvertisement = TBLEAdvertisement(EventExtra())
			If advertisement
				Local name:String = advertisement.LocalName()
				If Not name Then name = "(unnamed)"
				Print advertisement.AddressString() + "  " + advertisement.rssi + " dBm  " + name
				count :+ 1
			End If
		Case EVENT_BLERESET
			RuntimeError "BLE host reset: " + EventData()
	End Select
Wend
PollSystem()
Print "BLE scan complete: " + count + " reports; dropped=" + BLEDroppedEvents()
result = BLEDeinitialize()
If result <> 0 Then RuntimeError "BLE deinitialization failed: " + BLEResultName(result)
Print "BLE deinitialized cleanly"
