' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP32.Network.BLE

Const ServiceUUID:String = "7c9a0051-8e5f-4b2a-9d74-7b7d25f1a100"
Const CharacteristicUUID:String = "7c9a0052-8e5f-4b2a-9d74-7b7d25f1a100"

If BLEGATTReset() <> 0 Then RuntimeError "Could not reset the GATT definition"
Local service:TBLEGATTService = CreateBLEGATTService(ServiceUUID)
If Not service Then RuntimeError "Could not create GATT service: " + BLEGATTLastError()
Local value:Byte[] = [Byte(67), Byte(111), Byte(110), Byte(110), Byte(101), Byte(99), Byte(116), Byte(101), Byte(100)]
If Not service.AddCharacteristic(CharacteristicUUID, BLEGATTRead, value) Then ..
	RuntimeError "Could not create GATT characteristic: " + BLEGATTLastError()

Local result:Int = BLEInitialize("BlitzMax Connection")
If result <> 0 Then RuntimeError "BLE initialization failed: " + BLEResultName(result)
If Not BLEWaitReady() Then RuntimeError "BLE host did not become ready"

Print "Stored bonds: " + BLEBondCount()
For Local index:Int = 0 Until BLEBondCount()
	Local bond:TBLEBond = BLEBondAt(index)
	If bond.status = 0 Then Print "Bond " + index + ": " + bond.AddressString()
Next

result = BLEStartAdvertising(service)
If result <> 0 Then RuntimeError "BLE advertising failed: " + BLEResultName(result)
Print "BLE connection-management peripheral ready"

While True
	Select WaitEvent()
		Case EVENT_BLECONNECTED
			Local connection:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
			If connection.status <> 0 Then RuntimeError "BLE connection failed: " + BLEResultName(connection.status)
			Local info:TBLEConnectionInfo = BLEConnectionAt(0)
			If info.status <> 0 Then RuntimeError "Could not inspect connection: " + BLEResultName(info.status)
			If info.connectionHandle <> connection.connectionHandle Then RuntimeError "Connection enumeration mismatch"
			Print "Connections: " + BLEConnectionCount() + ", peer: " + info.AddressString()
			Print "Initial interval: " + info.intervalMicroseconds + " us, latency: " + ..
				info.latency + ", supervision timeout: " + info.supervisionTimeoutMilliseconds + " ms"
			If info.rssiStatus = 0 Then Print "RSSI: " + info.rssi + " dBm"
			If info.phyStatus = 0 Then Print "Initial PHY: tx=" + info.txPHY + " rx=" + info.rxPHY
			Print "Stored bonds at connect: " + BLEBondCount()
			For Local index:Int = 0 Until BLEBondCount()
				Local bond:TBLEBond = BLEBondAt(index)
				If bond.status = 0 Then Print "Bond " + index + ": " + bond.AddressString()
			Next

			result = connection.UpdateParameters(30000, 50000, 0, 4000)
			If result <> 0 Then RuntimeError "Could not request connection parameters: " + BLEResultName(result)
			result = connection.PreferPHY(BLEPHYMask2M, BLEPHYMask2M)
			If result <> 0 Then RuntimeError "Could not request 2M PHY: " + BLEResultName(result)
		Case EVENT_BLECONNECTIONUPDATED
			Local update:TBLEConnectionUpdateEvent = TBLEConnectionUpdateEvent(EventExtra())
			If update.status <> 0 Then RuntimeError "Connection update failed: " + BLEResultName(update.status)
			Print "Updated interval: " + update.intervalMicroseconds + " us, latency: " + ..
				update.latency + ", supervision timeout: " + update.supervisionTimeoutMilliseconds + " ms"
		Case EVENT_BLEPHYUPDATED
			Local phy:TBLEPHYEvent = TBLEPHYEvent(EventExtra())
			If phy.status <> 0 Then RuntimeError "PHY update failed: " + BLEResultName(phy.status)
			Print "Updated PHY: tx=" + phy.txPHY + " rx=" + phy.rxPHY
		Case EVENT_BLEDISCONNECTED
			Print "Disconnected; active connections: " + BLEConnectionCount()
		Case EVENT_BLERESET
			RuntimeError "BLE host reset: " + BLEResultName(EventData())
	End Select
Wend
