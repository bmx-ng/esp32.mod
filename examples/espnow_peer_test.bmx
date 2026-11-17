SuperStrict

Framework BRL.StandardIO
Import Embedded.Network.WiFi
Import ESP32.Network.ESPNow

Const Channel:Int = 1

Function Check:Int(result:Int, operation:String)
	If result <> 0 Then RuntimeError operation + " failed: " + ESPNowResultName(result) + " (" + result + ")"
	Return result
End Function

Function AddressesEqual:Int(left:Byte[], right:Byte[])
	If Not left Or Not right Or left.length <> right.length Then Return False
	For Local index:Int = 0 Until left.length
		If left[index] <> right[index] Then Return False
	Next
	Return True
End Function

Function TestPayload:Byte[](length:Int, sequence:Int)
	Local data:Byte[length]
	For Local index:Int = 0 Until length
		data[index] = Byte((index * 17 + sequence * 29) & $ff)
	Next
	Return data
End Function

Function PayloadChecksum:UInt(data:Byte[])
	Local checksum:UInt
	For Local value:Byte = EachIn data
		checksum = (checksum * 33) ~ UInt(value & $ff)
	Next
	Return checksum
End Function

Check WiFiInitialize(), "Wi-Fi initialization"
Check ESPNowSetChannel(Channel), "channel selection"
Check ESPNowInitialize(8, 16, ESPNowMaximumPayloadV2), "ESP-NOW initialization"

Local wroomAddress:Byte[] = ESPNowParseAddress("7c:d5:44:35:44:1c")
Local c6Address:Byte[] = ESPNowParseAddress("10:bd:a3:90:62:50")
Local localAddress:Byte[ESPNowAddressLength]
Check ESPNowLocalAddress(localAddress), "local address query"

Local peerAddress:Byte[]
Local role:String
If AddressesEqual(localAddress, wroomAddress) Then
	role = "WROOM-32D"
	peerAddress = c6Address
Else If AddressesEqual(localAddress, c6Address) Then
	role = "XIAO-C6"
	peerAddress = wroomAddress
Else
	RuntimeError "Unexpected test device: " + ESPNowFormatAddress(localAddress)
End If

Check ESPNowAddBroadcastPeer(Channel), "broadcast peer registration"
Local primaryMasterKey:Byte[ESPNowKeyLength]
Local localMasterKey:Byte[ESPNowKeyLength]
For Local index:Int = 0 Until ESPNowKeyLength
	primaryMasterKey[index] = Byte($31 + index)
	localMasterKey[index] = Byte($a0 + index)
Next
Check ESPNowSetPrimaryMasterKey(primaryMasterKey), "primary master key selection"
Check ESPNowAddPeer(peerAddress, Channel, localMasterKey), "encrypted unicast peer registration"

Local version:UInt
Check ESPNowVersion(version), "version query"
Print role + " ready at " + ESPNowFormatAddress(localAddress)
Print "Peer: " + ESPNowFormatAddress(peerAddress) + ", ESP-NOW v" + version + ", channel " + Channel
Local totalPeers:Int, encryptedPeers:Int
Check ESPNowPeerCounts(totalPeers, encryptedPeers), "peer count query"
Print "Peers: " + totalPeers + " total, " + encryptedPeers + " encrypted"

Local sequence:Int
Local nextSend:UInt = MilliSecs() + 1500
While True
	Local packet:SESPNowReceivedPacket
	While ESPNowPollReceived(packet)
		Print "RX " + packet.data.length + " bytes from " + ESPNowFormatAddress(packet.sourceAddress) + ..
			" RSSI " + packet.rssi + " checksum " + PayloadChecksum(packet.data)
	Wend

	Local report:SESPNowDeliveryReport
	While ESPNowPollDelivery(report)
		Local delivery:String = "failed"
		If report.delivered Then delivery = "ok"
		Print "TX " + ESPNowFormatAddress(report.destinationAddress) + ..
			" delivery " + delivery
	Wend

	Local now:UInt = MilliSecs()
	If now >= nextSend Then
		Local length:Int
		Local destination:Byte[]
		Select sequence Mod 3
			Case 0
				length = 24
				destination = ESPNowParseAddress(ESPNowBroadcastAddress)
			Case 1
				length = ESPNowMaximumPayloadV1
				destination = peerAddress
			Default
				length = ESPNowMaximumPayloadV2
				destination = peerAddress
		End Select
		Local data:Byte[] = TestPayload(length, sequence)
		Local result:Int = ESPNowSend(destination, data)
		Print "SEND " + length + " bytes to " + ESPNowFormatAddress(destination) + ..
			" checksum " + PayloadChecksum(data) + " result " + ESPNowResultName(result)
		sequence :+ 1
		nextSend = now + 2000
	End If
	Delay 5
Wend
