' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP32.Network.BLE

Const ServiceUUID:String = "7c9a0021-8e5f-4b2a-9d74-7b7d25f1a100"
Const CharacteristicUUID:String = "7c9a0022-8e5f-4b2a-9d74-7b7d25f1a100"

If BLEGATTReset() <> 0 Then RuntimeError "Could not reset the GATT definition"
Local service:TBLEGATTService = CreateBLEGATTService(ServiceUUID)
If Not service Then RuntimeError "Could not create GATT service: " + BLEGATTLastError()
Local value:Byte[] = [Byte(66), Byte(108), Byte(105), Byte(116), Byte(122), Byte(77), Byte(97), Byte(120)]
Local characteristic:TBLEGATTCharacteristic = service.AddCharacteristic(CharacteristicUUID, ..
	BLEGATTRead | BLEGATTIndicate, value)
If Not characteristic Then RuntimeError "Could not create GATT characteristic: " + BLEGATTLastError()

Local result:Int = BLEInitialize("BlitzMax Indication")
If result <> 0 Then RuntimeError "BLE initialization failed: " + BLEResultName(result)
If Not BLEWaitReady() Then RuntimeError "BLE host did not become ready"
result = BLEStartAdvertising(service)
If result <> 0 Then RuntimeError "BLE advertising failed: " + BLEResultName(result)
Print "BLE indication peripheral ready"

While True
	Select WaitEvent()
		Case EVENT_BLEMTUCHANGED
			Local mtuEvent:TBLEMTUEvent = TBLEMTUEvent(EventExtra())
			Print "MTU: " + mtuEvent.mtu
		Case EVENT_BLEGATTSUBSCRIBE
			Local subscription:TBLEGATTSubscriptionEvent = TBLEGATTSubscriptionEvent(EventExtra())
			If subscription.characteristic = characteristic And subscription.indications
				result = characteristic.Indicate(subscription.connectionHandle)
				If result <> 0 Then RuntimeError "BLE indication failed to start: " + BLEResultName(result)
			End If
		Case EVENT_BLEGATTUPDATECOMPLETE
			Local update:TBLEGATTUpdateEvent = TBLEGATTUpdateEvent(EventExtra())
			If update.status <> 0 Then RuntimeError "BLE update failed: " + BLEResultName(update.status)
			If update.indication And update.confirmed Then Print "BLE indication confirmed"
		Case EVENT_BLEDISCONNECTED
			Local disconnection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			Print "Disconnected: " + disconnection.reason
		Case EVENT_BLERESET
			RuntimeError "BLE host reset: " + BLEResultName(EventData())
	End Select
Wend
