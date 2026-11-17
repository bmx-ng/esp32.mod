SuperStrict

Framework BRL.StandardIO
Import Embedded.Network.WiFi
Import ESP32.Network.WiFi
Import ESP32.Network.ESPNow

' Replace this with the station MAC printed by espnow_identity.bmx on the peer.
Const PeerAddressText:String = "24:6f:28:12:34:56"
Const Channel:Int = 1
Const EnablePowerSaving:Int = False
Const UseAccessPointInterface:Int = False

Function Check:Int(result:Int, operation:String)
	If result <> 0 Then RuntimeError operation + ": " + ESPNowResultName(result)
	Return result
End Function

Check WiFiInitialize(), "Wi-Fi initialization failed"
Check ESPNowSetChannel(Channel), "Channel selection failed"
Local peerInterface:Int = ESPNowInterfaceStation
If UseAccessPointInterface Then
	Check WiFiStartAccessPoint("BlitzMax ESP-NOW", "", WiFiAuthenticationOpen, Channel), ..
		"SoftAP start failed"
	peerInterface = ESPNowInterfaceAccessPoint
End If
Check ESPNowInitialize(), "ESP-NOW initialization failed"

Local peerAddress:Byte[] = ESPNowParseAddress(PeerAddressText)
If Not peerAddress Then RuntimeError "Invalid peer address"
Check ESPNowAddPeer(peerAddress, Channel, Null, peerInterface), "Peer registration failed"

' Select a deliberately robust legacy rate supported by old and new ESP32 radios.
Local rate:SESPNowRateConfig
rate.phyMode = ESPNowPHYMode80211B
rate.rate = ESPNowRate1MbpsLong
Check ESPNowSetPeerRate(peerAddress, rate), "Peer rate selection failed"

' Battery applications can enable a short receive window at a periodic interval.
If EnablePowerSaving Then
	Check WiFiSetPowerSave(WiFiPowerSaveMaximumModem), "Modem sleep selection failed"
	Check WiFiSetConnectionlessWakeInterval(1000), "Wake interval selection failed"
	Check ESPNowSetWakeWindow(100), "ESP-NOW wake window selection failed"
End If

Local peer:SESPNowPeerInfo
Check ESPNowGetPeer(peerAddress, peer), "Peer query failed"
Print "Peer " + ESPNowFormatAddress(peer.address) + ", channel " + peer.channel + ..
	", interface " + peer.interfaceIndex + ", encrypted " + peer.encrypted

Print "Registered unicast peers:"
Local result:Int = ESPNowFirstPeer(peer)
While result = 0
	Print "  " + ESPNowFormatAddress(peer.address)
	result = ESPNowNextPeer(peer)
Wend

' A null ESP-IDF destination is wrapped as an explicit, readable operation.
Local payload:Byte[] = [$42, $4d, $58]
Local nextSend:UInt = MilliSecs() + 2000

While True
	Local packet:SESPNowReceivedPacket
	While ESPNowPollReceived(packet)
		Print "RX " + packet.data.length + " bytes from " + ..
			ESPNowFormatAddress(packet.sourceAddress) + ", RSSI " + packet.rssi + ..
			", noise " + packet.noiseFloor + ", rate " + packet.rate
	Wend

	Local delivery:SESPNowDeliveryReport
	While ESPNowPollDelivery(delivery)
		Print "TX " + ESPNowFormatAddress(delivery.destinationAddress) + ..
			", interface " + delivery.interfaceIndex + ", rate " + delivery.rate + ..
			", delivered " + delivery.delivered
	Wend

	Local now:UInt = MilliSecs()
	If now >= nextSend Then
		Check ESPNowSendToAll(payload), "Send-to-all failed"
		Print "Sent to all registered peers"
		nextSend = now + 5000
	End If
	Delay 5
Wend
