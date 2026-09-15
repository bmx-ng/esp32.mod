' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP32.Network.BLE

Const ServiceUUID:String = "7c9a0041-8e5f-4b2a-9d74-7b7d25f1a100"
Const CharacteristicUUID:String = "7c9a0042-8e5f-4b2a-9d74-7b7d25f1a100"
Const DemonstrationPasskey:Int = 123456

If BLEGATTReset() <> 0 Then RuntimeError "Could not reset the GATT definition"
Local service:TBLEGATTService = CreateBLEGATTService(ServiceUUID)
If Not service Then RuntimeError "Could not create GATT service: " + BLEGATTLastError()
Local value:Byte[] = [Byte(65), Byte(117), Byte(116), Byte(104), Byte(101), Byte(110), Byte(116), Byte(105), Byte(99), Byte(97), Byte(116), Byte(101), Byte(100)]
Local characteristic:TBLEGATTCharacteristic = service.AddCharacteristic(CharacteristicUUID, ..
	BLEGATTRead | BLEGATTReadAuthenticated, value)
If Not characteristic Then RuntimeError "Could not create authenticated characteristic: " + BLEGATTLastError()

Local result:Int = BLEConfigureSecurity(BLEIOCapabilityDisplayOnly, True, True, True)
If result <> 0 Then RuntimeError "BLE security configuration failed: " + BLEResultName(result)
result = BLEInitialize("BlitzMax Passkey GATT")
If result <> 0 Then RuntimeError "BLE initialization failed: " + BLEResultName(result)
If Not BLEWaitReady() Then RuntimeError "BLE host did not become ready"
result = BLEStartAdvertising(service)
If result <> 0 Then RuntimeError "BLE advertising failed: " + BLEResultName(result)
Print "BLE passkey peripheral ready"

While True
	Select WaitEvent()
		Case EVENT_BLECONNECTED
			Local connection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			If connection.status <> 0 Then RuntimeError "BLE connection failed: " + BLEResultName(connection.status)
			result = connection.Secure()
			If result <> 0 Then RuntimeError "BLE security could not start: " + BLEResultName(result)
		Case EVENT_BLEPASSKEYACTION
			Local passkey:TBLEPasskeyEvent = TBLEPasskeyEvent(EventExtra())
			If passkey.action <> BLEPasskeyActionDisplay Then ..
				RuntimeError "Unexpected passkey action: " + passkey.action
			Print "Enter passkey " + DemonstrationPasskey + " on the peer"
			result = passkey.Provide(DemonstrationPasskey)
			If result <> 0 Then RuntimeError "Could not provide BLE passkey: " + BLEResultName(result)
		Case EVENT_BLESECURITYCHANGED
			Local security:TBLESecurityEvent = TBLESecurityEvent(EventExtra())
			If security.status <> 0 Then RuntimeError "BLE security failed: " + BLEResultName(security.status)
			Print "Authenticated: encrypted=" + security.encrypted + " authenticated=" + ..
				security.authenticated + " bonded=" + security.bonded + " keySize=" + security.keySize
		Case EVENT_BLEDISCONNECTED
			Local disconnection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			Print "Disconnected: " + disconnection.reason
		Case EVENT_BLERESET
			RuntimeError "BLE host reset: " + BLEResultName(EventData())
	End Select
Wend
