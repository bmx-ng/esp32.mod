SuperStrict

Framework BRL.StandardIO
Import Embedded.Network.WiFi
Import ESP32.Network.ESPNow

' Give each device the other device's station MAC. Both devices must use the
' same channel, PMK, and LMK. Use securely generated keys in real firmware.
Const PeerAddressText:String = "24:6f:28:12:34:56"
Const Channel:Int = 1

Function Check:Int(result:Int, operation:String)
	If result <> 0 Then RuntimeError operation + ": " + ESPNowResultName(result)
	Return result
End Function

Local peerAddress:Byte[] = ESPNowParseAddress(PeerAddressText)
If Not peerAddress Then RuntimeError "Invalid peer address"

' Demonstration keys only. Do not deploy or commit production keys in source.
Local primaryMasterKey:Byte[ESPNowKeyLength]
Local localMasterKey:Byte[ESPNowKeyLength]
For Local index:Int = 0 Until ESPNowKeyLength
	primaryMasterKey[index] = Byte($31 + index)
	localMasterKey[index] = Byte($a0 + index)
Next

Check WiFiInitialize(), "Wi-Fi initialization failed"
Check ESPNowSetChannel(Channel), "Channel selection failed"
Check ESPNowInitialize(), "ESP-NOW initialization failed"
Check ESPNowSetPrimaryMasterKey(primaryMasterKey), "PMK selection failed"
Check ESPNowAddPeer(peerAddress, Channel, localMasterKey), "Encrypted peer registration failed"

Local totalPeers:Int, encryptedPeers:Int
Check ESPNowPeerCounts(totalPeers, encryptedPeers), "Peer count query failed"
Print "Encrypted peer ready: " + ESPNowFormatAddress(peerAddress)
Print "Peers: " + totalPeers + " total, " + encryptedPeers + " encrypted"

Local sequence:Int
Local waitingForDelivery:Int
Local nextSend:UInt = MilliSecs() + 1000
While True
	Local packet:SESPNowReceivedPacket
	While ESPNowPollReceived(packet)
		Print "Encrypted RX " + packet.data.length + " bytes from " + ..
			ESPNowFormatAddress(packet.sourceAddress)
	Wend

	Local report:SESPNowDeliveryReport
	While ESPNowPollDelivery(report)
		waitingForDelivery = False
		Print "Encrypted delivery: " + report.delivered
	Wend

	Local now:UInt = MilliSecs()
	If Not waitingForDelivery And now >= nextSend Then
		sequence :+ 1
		Local payload:Byte[ESPNowMaximumPayloadV2]
		For Local index:Int = 0 Until payload.length
			payload[index] = Byte((index * 17 + sequence * 29) & $ff)
		Next
		Check ESPNowSend(peerAddress, payload), "Encrypted send failed"
		waitingForDelivery = True
		nextSend = now + 3000
	End If
	Delay 5
Wend
