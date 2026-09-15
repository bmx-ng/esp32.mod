' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP32.Network.BLE

Const ServiceUUID:String = "7c9a0031-8e5f-4b2a-9d74-7b7d25f1a100"
Const CharacteristicUUID:String = "7c9a0032-8e5f-4b2a-9d74-7b7d25f1a100"

If BLEGATTReset() <> 0 Then RuntimeError "Could not reset the GATT definition"
Local service:TBLEGATTService = CreateBLEGATTService(ServiceUUID)
If Not service Then RuntimeError "Could not create GATT service: " + BLEGATTLastError()
Local value:Byte[] = [Byte(83), Byte(101), Byte(99), Byte(117), Byte(114), Byte(101)]
Local characteristic:TBLEGATTCharacteristic = service.AddCharacteristic(CharacteristicUUID, ..
	BLEGATTRead | BLEGATTWrite | BLEGATTReadEncrypted | BLEGATTWriteEncrypted, value)
If Not characteristic Then RuntimeError "Could not create protected characteristic: " + BLEGATTLastError()

' No-input/no-output pairing uses LE Secure Connections when the peer supports it.
' It encrypts the link and persists a bond, but does not provide MITM authentication.
Local result:Int = BLEConfigureSecurity(BLEIOCapabilityNone, True, False, True)
If result <> 0 Then RuntimeError "BLE security configuration failed: " + BLEResultName(result)
result = BLEInitialize("BlitzMax Secure GATT")
If result <> 0 Then RuntimeError "BLE initialization failed: " + BLEResultName(result)
If Not BLEWaitReady() Then RuntimeError "BLE host did not become ready"
Print "Stored bonds: " + BLEBondCount()
result = BLEStartAdvertising(service)
If result <> 0 Then RuntimeError "BLE advertising failed: " + BLEResultName(result)
Print "BLE secure peripheral ready"

While True
	Select WaitEvent()
		Case EVENT_BLECONNECTED
			Local connection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			If connection.status <> 0 Then RuntimeError "BLE connection failed: " + BLEResultName(connection.status)
			Print "Stored bonds at connect: " + BLEBondCount()
			result = connection.Secure()
			If result <> 0 Then RuntimeError "BLE security could not start: " + BLEResultName(result)
		Case EVENT_BLESECURITYCHANGED
			Local security:TBLESecurityEvent = TBLESecurityEvent(EventExtra())
			If security.status <> 0 Then RuntimeError "BLE security failed: " + BLEResultName(security.status)
			Print "Secure: encrypted=" + security.encrypted + " authenticated=" + ..
				security.authenticated + " bonded=" + security.bonded + " keySize=" + security.keySize
		Case EVENT_BLEPASSKEYACTION
			RuntimeError "Unexpected passkey action for a no-input/no-output device"
		Case EVENT_BLEGATTWRITE
			Local writeEvent:TBLEGATTWriteEvent = TBLEGATTWriteEvent(EventExtra())
			Print "Protected value written: " + writeEvent.value.length + " bytes"
		Case EVENT_BLEDISCONNECTED
			Local disconnection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			Print "Disconnected: " + disconnection.reason
		Case EVENT_BLERESET
			RuntimeError "BLE host reset: " + BLEResultName(EventData())
	End Select
Wend
