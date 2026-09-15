' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP32.Network.BLE

Const ServiceUUID:String = "7c9a0001-8e5f-4b2a-9d74-7b7d25f1a100"
Const CharacteristicUUID:String = "7c9a0002-8e5f-4b2a-9d74-7b7d25f1a100"

If BLEGATTReset() <> 0 Then RuntimeError "Could not reset the GATT definition"
Local initialValue:Byte[] = [Byte(72), Byte(101), Byte(108), Byte(108), Byte(111)]
Local service:TBLEGATTService = CreateBLEGATTService(ServiceUUID)
If Not service Then RuntimeError "Could not create GATT service: " + BLEGATTLastError()
Local characteristic:TBLEGATTCharacteristic = service.AddCharacteristic(CharacteristicUUID, ..
	BLEGATTRead | BLEGATTWrite | BLEGATTNotify, initialValue, 256)
If Not characteristic Then RuntimeError "Could not create GATT characteristic: " + BLEGATTLastError()

Local result:Int = BLEInitialize("BlitzMax GATT")
If result <> 0 Then RuntimeError "BLE initialization failed: " + BLEResultName(result)
If Not BLEWaitReady() Then RuntimeError "BLE host did not become ready"

result = BLEStartAdvertising(service)
If result <> 0 Then RuntimeError "BLE advertising failed: " + BLEResultName(result)
Print "BLE GATT peripheral ready"

While True
	Select WaitEvent()
		Case EVENT_BLECONNECTED
			Local connection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			Print "Connected: handle=" + connection.connectionHandle
		Case EVENT_BLEDISCONNECTED
			Local disconnection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			Print "Disconnected: reason=" + disconnection.reason
		Case EVENT_BLEGATTWRITE
			Local writeEvent:TBLEGATTWriteEvent = TBLEGATTWriteEvent(EventExtra())
			Print "Write: " + writeEvent.value.length + " bytes"
			' Echo the new value to subscribed clients.
			writeEvent.characteristic.SetValue(writeEvent.value, True)
		Case EVENT_BLEGATTSUBSCRIBE
			Local subscription:TBLEGATTSubscriptionEvent = TBLEGATTSubscriptionEvent(EventExtra())
			Print "Subscription: notify=" + subscription.notifications + ..
				" indicate=" + subscription.indications
		Case EVENT_BLERESET
			RuntimeError "BLE host reset: " + EventData()
	End Select
Wend
