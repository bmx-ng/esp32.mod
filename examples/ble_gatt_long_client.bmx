' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP32.Network.BLE

Const PeripheralName:String = "BlitzMax Long GATT"
Const ServiceUUID:String = "7c9a0011-8e5f-4b2a-9d74-7b7d25f1a100"
Const CharacteristicUUID:String = "7c9a0012-8e5f-4b2a-9d74-7b7d25f1a100"

Function Pattern:Byte[](inverted:Int = False)
	Local value:Byte[] = New Byte[BLEMaximumAttributeLength]
	For Local index:Int = 0 Until value.length
		If inverted
			value[index] = Byte(255 - (index & 255))
		Else
			value[index] = Byte(index & 255)
		End If
	Next
	Return value
End Function

Function Matches:Int(value:Byte[], expected:Byte[])
	If Not value Or value.length <> expected.length Then Return False
	For Local index:Int = 0 Until value.length
		If value[index] <> expected[index] Then Return False
	Next
	Return True
End Function

Function MismatchDescription:String(value:Byte[], expected:Byte[])
	If Not value Then Return "null value"
	If value.length <> expected.length Then Return "length=" + value.length + " expected=" + expected.length
	For Local index:Int = 0 Until value.length
		If value[index] <> expected[index] Then ..
			Return "index=" + index + " value=" + Int(value[index]) + " expected=" + Int(expected[index])
	Next
	Return "none"
End Function

Local initial:Byte[] = Pattern()
Local replacement:Byte[] = Pattern(True)
Local result:Int = BLEInitialize("BlitzMax Long GATT Central")
If result <> 0 Then RuntimeError "BLE initialization failed: " + BLEResultName(result)
If Not BLEWaitReady() Then RuntimeError "BLE host did not become ready"

result = BLEStartScan(30000, True, True)
If result <> 0 Then RuntimeError "BLE scan failed: " + BLEResultName(result)

Local target:TBLEAdvertisement
Local service:TBLEClientService
Local characteristic:TBLEClientCharacteristic
Local connectionHandle:Int = -1
Local writing:Int
Local finished:Int

While Not finished
	Select WaitEvent()
		Case EVENT_BLESCANRESULT
			Local advertisement:TBLEAdvertisement = TBLEAdvertisement(EventExtra())
			If Not target And advertisement And (advertisement.LocalName() = PeripheralName Or ..
					advertisement.AdvertisesService(ServiceUUID))
				target = advertisement
				result = BLEStopScan()
				If result <> 0 Then RuntimeError "Could not stop BLE scan: " + BLEResultName(result)
			End If
		Case EVENT_BLESCANCOMPLETE
			If Not target Then RuntimeError "BLE peripheral was not found"
			result = BLEConnectAdvertisement(target)
			If result <> 0 Then RuntimeError "BLE connection could not start: " + BLEResultName(result)
		Case EVENT_BLECONNECTED
			Local connection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			If connection.status <> 0 Then RuntimeError "BLE connection failed: " + BLEResultName(connection.status)
			If connection.role <> BLEConnectionRoleCentral Then Continue
			connectionHandle = connection.connectionHandle
			result = connection.ExchangeMTU()
			If result <> 0 Then RuntimeError "MTU exchange could not start: " + BLEResultName(result)
		Case EVENT_BLEMTUCHANGED
			Local mtuEvent:TBLEMTUEvent = TBLEMTUEvent(EventExtra())
			If mtuEvent.connectionHandle <> connectionHandle Then Continue
			Print "Negotiated MTU: " + mtuEvent.mtu
			If BLEConnectionMTU(connectionHandle) <> mtuEvent.mtu Then RuntimeError "MTU query disagrees with event"
			result = BLEDiscoverServices(connectionHandle)
			If result <> 0 Then RuntimeError "Service discovery could not start: " + BLEResultName(result)
		Case EVENT_BLESERVICEDISCOVERED
			Local discoveredService:TBLEClientService = TBLEClientService(EventExtra())
			If discoveredService.UUID() = ServiceUUID Then service = discoveredService
		Case EVENT_BLESERVICEDISCOVERYCOMPLETE
			Local serviceComplete:TBLEDiscoveryCompleteEvent = TBLEDiscoveryCompleteEvent(EventExtra())
			If serviceComplete.status <> 0 Then RuntimeError "Service discovery failed: " + BLEResultName(serviceComplete.status)
			If Not service Then RuntimeError "Required BLE service was not found"
			result = service.DiscoverCharacteristics()
			If result <> 0 Then RuntimeError "Characteristic discovery could not start: " + BLEResultName(result)
		Case EVENT_BLECHARACTERISTICDISCOVERED
			Local discoveredCharacteristic:TBLEClientCharacteristic = TBLEClientCharacteristic(EventExtra())
			If discoveredCharacteristic.UUID() = CharacteristicUUID Then characteristic = discoveredCharacteristic
		Case EVENT_BLECHARACTERISTICDISCOVERYCOMPLETE
			Local characteristicComplete:TBLEDiscoveryCompleteEvent = TBLEDiscoveryCompleteEvent(EventExtra())
			If characteristicComplete.status <> 0 Then RuntimeError "Characteristic discovery failed: " + BLEResultName(characteristicComplete.status)
			If Not characteristic Or Not characteristic.CanRead() Or Not characteristic.CanWrite() Then ..
				RuntimeError "Required BLE characteristic is unavailable"
			result = characteristic.Read()
			If result <> 0 Then RuntimeError "Initial long read could not start: " + BLEResultName(result)
		Case EVENT_BLEREADCOMPLETE
			Local readEvent:TBLEClientValueEvent = TBLEClientValueEvent(EventExtra())
			If readEvent.status <> 0 Then RuntimeError "Long read failed: " + BLEResultName(readEvent.status)
			If writing
				If Not Matches(readEvent.value, replacement) Then RuntimeError "Long write/read-back differs: " + MismatchDescription(readEvent.value, replacement)
				Print "512-byte GATT write/read-back passed"
				finished = True
			Else
				If Not Matches(readEvent.value, initial) Then RuntimeError "Initial long-read differs: " + MismatchDescription(readEvent.value, initial)
				Print "512-byte GATT read passed"
				writing = True
				result = characteristic.Write(replacement)
				If result <> 0 Then RuntimeError "Long write could not start: " + BLEResultName(result)
			End If
		Case EVENT_BLEWRITECOMPLETE
			Local writeEvent:TBLEClientValueEvent = TBLEClientValueEvent(EventExtra())
			If writeEvent.attributeHandle <> characteristic.ValueHandle() Then Continue
			If writeEvent.status <> 0 Then RuntimeError "Long write failed: " + BLEResultName(writeEvent.status)
			result = characteristic.Read()
			If result <> 0 Then RuntimeError "Read-back could not start: " + BLEResultName(result)
		Case EVENT_BLEDISCONNECTED
			Local disconnection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			RuntimeError "BLE disconnected: " + BLEResultName(disconnection.reason)
		Case EVENT_BLERESET
			RuntimeError "BLE host reset: " + BLEResultName(EventData())
	End Select
Wend

result = BLEDisconnect(connectionHandle)
If result <> 0 Then RuntimeError "BLE disconnect failed: " + BLEResultName(result)
While WaitEvent() <> EVENT_BLEDISCONNECTED
Wend
result = BLEDeinitialize()
If result <> 0 Then RuntimeError "BLE deinitialization failed: " + BLEResultName(result)
Print "BLE long GATT test passed"
