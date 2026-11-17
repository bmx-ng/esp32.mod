' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP-NOW connectionless peer-to-peer networking for ESP32 devices.
about: ESP-NOW uses the ESP32 Wi-Fi radio without requiring an access point.
Initialize Embedded.Network.WiFi before this module. Native radio callbacks copy
received packets and delivery reports into bounded queues; managed objects are
only created when the application polls those queues.
End Rem
Module ESP32.Network.ESPNow
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Network.WiFi

Const ESPNowAddressLength:Int = 6
Const ESPNowKeyLength:Int = 16
Const ESPNowMaximumPayloadV1:Int = 250
Const ESPNowMaximumPayloadV2:Int = 1470
Const ESPNowMaximumPayload:Int = ESPNowMaximumPayloadV2
Const ESPNowBroadcastAddress:String = "ff:ff:ff:ff:ff:ff"

Struct SESPNowReceivedPacket
	Field sourceAddress:Byte[]
	Field destinationAddress:Byte[]
	Field rssi:Int
	Field channel:Int
	Field data:Byte[]
End Struct

Struct SESPNowDeliveryReport
	Field destinationAddress:Byte[]
	Field delivered:Int
End Struct

Private

Extern "C"
	Function _ESPNowInitialize:Int(receiveCapacity:Int, deliveryCapacity:Int, maximumReceivedPayload:Int) = "bmx_esp32_espnow_initialize"
	Function _ESPNowDeinitialize:Int() = "bmx_esp32_espnow_deinitialize"
	Function _ESPNowInitialized:Int() = "bmx_esp32_espnow_initialized"
	Function _ESPNowResultName:String(result:Int) = "bmx_esp32_espnow_result_name"
	Function _ESPNowVersion:Int(version:UInt Var) = "bmx_esp32_espnow_version"
	Function _ESPNowLocalAddress:Int(address:Byte Ptr) = "bmx_esp32_espnow_local_address"
	Function _ESPNowSetChannel:Int(channel:Int) = "bmx_esp32_espnow_set_channel"
	Function _ESPNowGetChannel:Int(channel:Int Var) = "bmx_esp32_espnow_get_channel"
	Function _ESPNowSetPrimaryMasterKey:Int(key:Byte Ptr, length:Int) = "bmx_esp32_espnow_set_primary_master_key"
	Function _ESPNowAddPeer:Int(address:Byte Ptr, channel:Int, key:Byte Ptr, keyLength:Int) = "bmx_esp32_espnow_add_peer"
	Function _ESPNowDeletePeer:Int(address:Byte Ptr) = "bmx_esp32_espnow_delete_peer"
	Function _ESPNowPeerExists:Int(address:Byte Ptr) = "bmx_esp32_espnow_peer_exists"
	Function _ESPNowPeerCounts:Int(total:Int Var, encrypted:Int Var) = "bmx_esp32_espnow_peer_counts"
	Function _ESPNowSend:Int(address:Byte Ptr, data:Byte Ptr, length:Int) = "bmx_esp32_espnow_send"
	Function _ESPNowTakeReceived:Int(sourceAddress:Byte Ptr, destinationAddress:Byte Ptr, ..
		rssi:Int Var, channel:Int Var, data:Byte Ptr, capacity:Int, length:Int Var) = "bmx_esp32_espnow_take_received"
	Function _ESPNowTakeDelivery:Int(destinationAddress:Byte Ptr, delivered:Int Var) = "bmx_esp32_espnow_take_delivery"
	Function _ESPNowDroppedReceived:UInt() = "bmx_esp32_espnow_dropped_received"
	Function _ESPNowDroppedDeliveries:UInt() = "bmx_esp32_espnow_dropped_deliveries"
End Extern

Function _ESPNowValidAddress:Int(address:Byte[])
	Return address And address.length = ESPNowAddressLength
End Function

Public

Function ESPNowInitialize:Int(receiveQueueCapacity:Int = 8, deliveryQueueCapacity:Int = 16, ..
		maximumReceivedPayload:Int = ESPNowMaximumPayload)
	Return _ESPNowInitialize(receiveQueueCapacity, deliveryQueueCapacity, maximumReceivedPayload)
End Function

Function ESPNowDeinitialize:Int()
	Return _ESPNowDeinitialize()
End Function

Function ESPNowInitialized:Int()
	Return _ESPNowInitialized()
End Function

Function ESPNowResultName:String(result:Int)
	Return _ESPNowResultName(result)
End Function

Function ESPNowVersion:Int(version:UInt Var)
	Return _ESPNowVersion(version)
End Function

Function ESPNowLocalAddress:Int(address:Byte[])
	If Not _ESPNowValidAddress(address) Then Return -5
	Return _ESPNowLocalAddress(address)
End Function

Function ESPNowSetChannel:Int(channel:Int)
	Return _ESPNowSetChannel(channel)
End Function

Function ESPNowGetChannel:Int(channel:Int Var)
	Return _ESPNowGetChannel(channel)
End Function

Function ESPNowSetPrimaryMasterKey:Int(key:Byte[])
	If Not key Or key.length <> ESPNowKeyLength Then Return -5
	Return _ESPNowSetPrimaryMasterKey(key, key.length)
End Function

Function ESPNowAddPeer:Int(address:Byte[], channel:Int = 0, localMasterKey:Byte[] = Null)
	If Not _ESPNowValidAddress(address) Then Return -5
	If localMasterKey And localMasterKey.length <> ESPNowKeyLength Then Return -5
	If localMasterKey Then Return _ESPNowAddPeer(address, channel, localMasterKey, localMasterKey.length)
	Return _ESPNowAddPeer(address, channel, Null, 0)
End Function

Function ESPNowAddBroadcastPeer:Int(channel:Int = 0)
	Local address:Byte[] = ESPNowParseAddress(ESPNowBroadcastAddress)
	Return ESPNowAddPeer(address, channel)
End Function

Function ESPNowDeletePeer:Int(address:Byte[])
	If Not _ESPNowValidAddress(address) Then Return -5
	Return _ESPNowDeletePeer(address)
End Function

Function ESPNowPeerExists:Int(address:Byte[])
	If Not _ESPNowValidAddress(address) Then Return False
	Return _ESPNowPeerExists(address)
End Function

Function ESPNowPeerCounts:Int(total:Int Var, encrypted:Int Var)
	Return _ESPNowPeerCounts(total, encrypted)
End Function

Function ESPNowSend:Int(address:Byte[], data:Byte[])
	If Not _ESPNowValidAddress(address) Or Not data Or data.length > ESPNowMaximumPayload Then Return -5
	Return _ESPNowSend(address, data, data.length)
End Function

Function ESPNowBroadcast:Int(data:Byte[])
	Return ESPNowSend(ESPNowParseAddress(ESPNowBroadcastAddress), data)
End Function

Function ESPNowPollReceived:Int(packet:SESPNowReceivedPacket Var)
	Local sourceAddress:Byte[ESPNowAddressLength]
	Local destinationAddress:Byte[ESPNowAddressLength]
	Local payload:Byte[ESPNowMaximumPayload]
	Local length:Int
	If Not _ESPNowTakeReceived(sourceAddress, destinationAddress, packet.rssi, packet.channel, ..
			payload, payload.length, length) Then Return False
	packet.sourceAddress = New Byte[ESPNowAddressLength]
	packet.destinationAddress = New Byte[ESPNowAddressLength]
	packet.data = New Byte[length]
	MemCopy(Varptr packet.sourceAddress[0], Varptr sourceAddress[0], Size_T(sourceAddress.length))
	MemCopy(Varptr packet.destinationAddress[0], Varptr destinationAddress[0], Size_T(destinationAddress.length))
	If length Then MemCopy(Varptr packet.data[0], Varptr payload[0], Size_T(length))
	Return True
End Function

Function ESPNowPollDelivery:Int(report:SESPNowDeliveryReport Var)
	Local destinationAddress:Byte[ESPNowAddressLength]
	If Not _ESPNowTakeDelivery(destinationAddress, report.delivered) Then Return False
	report.destinationAddress = New Byte[ESPNowAddressLength]
	MemCopy(Varptr report.destinationAddress[0], Varptr destinationAddress[0], Size_T(destinationAddress.length))
	Return True
End Function

Function ESPNowDroppedReceived:UInt()
	Return _ESPNowDroppedReceived()
End Function

Function ESPNowDroppedDeliveries:UInt()
	Return _ESPNowDroppedDeliveries()
End Function

Function ESPNowFormatAddress:String(address:Byte[])
	If Not _ESPNowValidAddress(address) Then Return ""
	Local result:String
	For Local index:Int = 0 Until address.length
		If index Then result :+ ":"
		Local value:Int = address[index] & $ff
		result :+ Chr(_ESPNowHexCharacter(value Shr 4))
		result :+ Chr(_ESPNowHexCharacter(value & $f))
	Next
	Return result.ToLower()
End Function

Function ESPNowParseAddress:Byte[](value:String)
	Local compact:String = value.Replace(":", "").Replace("-", "")
	If compact.length <> ESPNowAddressLength * 2 Then Return Null
	Local address:Byte[ESPNowAddressLength]
	For Local index:Int = 0 Until ESPNowAddressLength
		Local high:Int = _ESPNowHexDigit(compact[index * 2])
		Local low:Int = _ESPNowHexDigit(compact[index * 2 + 1])
		If high < 0 Or low < 0 Then Return Null
		address[index] = Byte((high Shl 4) | low)
	Next
	Return address
End Function

Private

Function _ESPNowHexDigit:Int(value:Int)
	If value >= Asc("0") And value <= Asc("9") Then Return value - Asc("0")
	If value >= Asc("a") And value <= Asc("f") Then Return value - Asc("a") + 10
	If value >= Asc("A") And value <= Asc("F") Then Return value - Asc("A") + 10
	Return -1
End Function

Function _ESPNowHexCharacter:Int(value:Int)
	If value < 10 Then Return Asc("0") + value
	Return Asc("a") + value - 10
End Function
?
