' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP32.Network.BLE

Const PeripheralName:String = "BlitzMax Test Peripheral"
Const ServiceUUID:String = "7c9a0001-8e5f-4b2a-9d74-7b7d25f1a100"
Const CharacteristicUUID:String = "7c9a0002-8e5f-4b2a-9d74-7b7d25f1a100"
Const ClientConfigurationUUID:String = "0x2902"

Local result:Int = BLEInitialize("BlitzMax Central")
If result <> 0 Then RuntimeError "BLE initialization failed: " + BLEResultName(result)
If Not BLEWaitReady() Then RuntimeError "BLE host did not become ready"

Print "Scanning for " + PeripheralName
result = BLEStartScan(30000, True, True)
If result <> 0 Then RuntimeError "BLE scan failed: " + BLEResultName(result)

Local target:TBLEAdvertisement
Local service:TBLEClientService
Local characteristic:TBLEClientCharacteristic
Local configuration:TBLEClientDescriptor
Local subscriptionWritten:Int
Local valueWritten:Int
Local notificationReceived:Int

While Not notificationReceived
	Select WaitEvent()
		Case EVENT_BLESCANRESULT
			Local advertisement:TBLEAdvertisement = TBLEAdvertisement(EventExtra())
			' Active scanning may deliver the connectable advertisement and its
			' name-bearing scan response as separate reports with the same address.
			If Not target And advertisement And (advertisement.LocalName() = PeripheralName Or ..
					advertisement.AdvertisesService(ServiceUUID))
				target = advertisement
				Print "Found " + advertisement.AddressString() + "; stopping scan"
				result = BLEStopScan()
				If result <> 0 Then RuntimeError "Could not stop BLE scan: " + BLEResultName(result)
			End If
		Case EVENT_BLESCANCOMPLETE
			If Not target Then RuntimeError "BLE peripheral was not found"
			Print "Connecting"
			result = BLEConnectAdvertisement(target)
			If result <> 0 Then RuntimeError "BLE connection could not start: " + BLEResultName(result)
		Case EVENT_BLECONNECTED
			Local connection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			If connection.status <> 0 Then RuntimeError "BLE connection failed: " + BLEResultName(connection.status)
			If connection.role <> BLEConnectionRoleCentral Then Continue
			Print "Connected; discovering services"
			result = connection.DiscoverServices()
			If result <> 0 Then RuntimeError "Service discovery could not start: " + BLEResultName(result)
		Case EVENT_BLESERVICEDISCOVERED
			Local discoveredService:TBLEClientService = TBLEClientService(EventExtra())
			Print "Service: " + discoveredService.UUID()
			If discoveredService.UUID() = ServiceUUID Then service = discoveredService
		Case EVENT_BLESERVICEDISCOVERYCOMPLETE
			Local serviceComplete:TBLEDiscoveryCompleteEvent = TBLEDiscoveryCompleteEvent(EventExtra())
			If serviceComplete.status <> 0 Then RuntimeError "Service discovery failed: " + BLEResultName(serviceComplete.status)
			If Not service Then RuntimeError "Required BLE service was not found"
			Print "Discovering characteristics"
			result = service.DiscoverCharacteristics()
			If result <> 0 Then RuntimeError "Characteristic discovery could not start: " + BLEResultName(result)
		Case EVENT_BLECHARACTERISTICDISCOVERED
			Local discoveredCharacteristic:TBLEClientCharacteristic = TBLEClientCharacteristic(EventExtra())
			Print "Characteristic: " + discoveredCharacteristic.UUID()
			If discoveredCharacteristic.UUID() = CharacteristicUUID Then characteristic = discoveredCharacteristic
		Case EVENT_BLECHARACTERISTICDISCOVERYCOMPLETE
			Local characteristicComplete:TBLEDiscoveryCompleteEvent = TBLEDiscoveryCompleteEvent(EventExtra())
			If characteristicComplete.status <> 0 Then RuntimeError "Characteristic discovery failed: " + BLEResultName(characteristicComplete.status)
			If Not characteristic Then RuntimeError "Required BLE characteristic was not found"
			If Not characteristic.CanRead() Or Not characteristic.CanWrite() Or Not characteristic.CanNotify() Then ..
				RuntimeError "Required BLE characteristic properties are missing"
			Print "Reading characteristic"
			result = characteristic.Read()
			If result <> 0 Then RuntimeError "BLE read could not start: " + BLEResultName(result)
		Case EVENT_BLEREADCOMPLETE
			Local readEvent:TBLEClientValueEvent = TBLEClientValueEvent(EventExtra())
			If readEvent.status <> 0 Then RuntimeError "BLE read failed: " + BLEResultName(readEvent.status)
			Print "Read: " + String.FromUTF8Bytes(readEvent.value, readEvent.value.length)
			Print "Discovering descriptors"
			result = characteristic.DiscoverDescriptors()
			If result <> 0 Then RuntimeError "Descriptor discovery could not start: " + BLEResultName(result)
		Case EVENT_BLEDESCRIPTORDISCOVERED
			Local descriptor:TBLEClientDescriptor = TBLEClientDescriptor(EventExtra())
			Print "Descriptor: " + descriptor.UUID()
			If descriptor.UUID() = ClientConfigurationUUID Then configuration = descriptor
		Case EVENT_BLEDESCRIPTORDISCOVERYCOMPLETE
			Local descriptorComplete:TBLEDiscoveryCompleteEvent = TBLEDiscoveryCompleteEvent(EventExtra())
			If descriptorComplete.status <> 0 Then RuntimeError "Descriptor discovery failed: " + BLEResultName(descriptorComplete.status)
			If Not configuration Then RuntimeError "Client configuration descriptor was not found"
			Print "Enabling notifications"
			result = configuration.SetSubscription(True, False)
			If result <> 0 Then RuntimeError "BLE subscription could not start: " + BLEResultName(result)
		Case EVENT_BLEWRITECOMPLETE
			Local writeEvent:TBLEClientValueEvent = TBLEClientValueEvent(EventExtra())
			If writeEvent.status <> 0 Then RuntimeError "BLE write failed: " + BLEResultName(writeEvent.status)
			If writeEvent.attributeHandle = configuration.Handle()
				subscriptionWritten = True
				Local message:Byte[] = [Byte(66), Byte(108), Byte(105), Byte(116), Byte(122), ..
					Byte(77), Byte(97), Byte(120), Byte(32), Byte(114), Byte(111), Byte(117), ..
					Byte(110), Byte(100), Byte(32), Byte(116), Byte(114), Byte(105), Byte(112)]
				Print "Writing characteristic"
				result = characteristic.Write(message)
				If result <> 0 Then RuntimeError "BLE write could not start: " + BLEResultName(result)
			Else If writeEvent.attributeHandle = characteristic.ValueHandle()
				valueWritten = True
				Print "Write acknowledged"
			End If
		Case EVENT_BLENOTIFICATION
			Local notification:TBLEClientValueEvent = TBLEClientValueEvent(EventExtra())
			If notification.status <> 0 Then RuntimeError "BLE notification was truncated: " + BLEResultName(notification.status)
			Print "Notification: " + String.FromUTF8Bytes(notification.value, notification.value.length)
			notificationReceived = True
		Case EVENT_BLEDISCONNECTED
			Local disconnection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			RuntimeError "BLE disconnected: " + BLEResultName(disconnection.reason)
		Case EVENT_BLERESET
			RuntimeError "BLE host reset: " + BLEResultName(EventData())
	End Select
Wend

If Not subscriptionWritten Or Not valueWritten Then RuntimeError "BLE round trip completed out of sequence"
Print "BLE central round trip passed"
result = BLEDisconnect(characteristic.Service().ConnectionHandle())
If result <> 0 Then RuntimeError "BLE disconnect failed: " + BLEResultName(result)

While True
	If WaitEvent() = EVENT_BLEDISCONNECTED Then Exit
Wend
result = BLEDeinitialize()
If result <> 0 Then RuntimeError "BLE deinitialization failed: " + BLEResultName(result)
Print "BLE deinitialized cleanly"
