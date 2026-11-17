SuperStrict

Framework BRL.StandardIO
Import Embedded.Network.WiFi
Import ESP32.Network.ESPNow

' Replace this with the station MAC printed by espnow_identity.bmx on the peer.
Const PeerAddressText:String = "24:6f:28:12:34:56"
Const Channel:Int = 1

Function Check:Int(result:Int, operation:String)
	If result <> 0 Then RuntimeError operation + ": " + ESPNowResultName(result)
	Return result
End Function

Function TestPayload:Byte[](sequence:Int)
	Local payload:Byte[32]
	payload[0] = Byte(sequence)
	payload[1] = Byte(sequence Shr 8)
	For Local index:Int = 2 Until payload.length
		payload[index] = Byte((sequence + index * 17) & $ff)
	Next
	Return payload
End Function

Local peerAddress:Byte[] = ESPNowParseAddress(PeerAddressText)
If Not peerAddress Then RuntimeError "Invalid peer address"

Check WiFiInitialize(), "Wi-Fi initialization failed"
Check ESPNowSetChannel(Channel), "Channel selection failed"
Check ESPNowInitialize(), "ESP-NOW initialization failed"
Check ESPNowAddPeer(peerAddress, Channel), "Peer registration failed"

Print "Unencrypted peer ready: " + ESPNowFormatAddress(peerAddress)

Local sequence:Int
Local waitingForDelivery:Int
Local nextSend:UInt = MilliSecs() + 1000
While True
	Local packet:SESPNowReceivedPacket
	While ESPNowPollReceived(packet)
		Print "Received " + packet.data.length + " bytes from " + ..
			ESPNowFormatAddress(packet.sourceAddress) + " at " + packet.rssi + " dBm"
	Wend

	Local report:SESPNowDeliveryReport
	While ESPNowPollDelivery(report)
		waitingForDelivery = False
		Print "Delivery to " + ESPNowFormatAddress(report.destinationAddress) + ": " + report.delivered
	Wend

	Local now:UInt = MilliSecs()
	If Not waitingForDelivery And now >= nextSend Then
		sequence :+ 1
		Check ESPNowSend(peerAddress, TestPayload(sequence)), "Unicast send failed"
		waitingForDelivery = True
		nextSend = now + 3000
	End If
	Delay 5
Wend
