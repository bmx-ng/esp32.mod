SuperStrict

Framework BRL.StandardIO
Import Embedded.Network.WiFi
Import ESP32.Network.ESPNow

Const Channel:Int = 1

Function Check:Int(result:Int, operation:String)
	If result <> 0 Then RuntimeError operation + ": " + ESPNowResultName(result)
	Return result
End Function

Function UTF8Payload:Byte[](value:String)
	Local length:Size_T
	Local source:Byte Ptr = value.ToUTF8String(length)
	Local payload:Byte[Int(length)]
	If length Then MemCopy(Varptr payload[0], source, length)
	MemFree source
	Return payload
End Function

Check WiFiInitialize(), "Wi-Fi initialization failed"
Check ESPNowSetChannel(Channel), "Channel selection failed"
Check ESPNowInitialize(), "ESP-NOW initialization failed"
Check ESPNowAddBroadcastPeer(Channel), "Broadcast registration failed"

Local localAddress:Byte[ESPNowAddressLength]
Check ESPNowLocalAddress(localAddress), "Local address query failed"
Local localName:String = ESPNowFormatAddress(localAddress)
Print "Broadcast chat ready as " + localName + " on channel " + Channel

Local sequence:Int
Local nextSend:UInt = MilliSecs() + 1000
While True
	Local packet:SESPNowReceivedPacket
	While ESPNowPollReceived(packet)
		Print ESPNowFormatAddress(packet.sourceAddress) + " [" + packet.rssi + " dBm]: " + ..
			String.FromUTF8Bytes(packet.data, packet.data.length)
	Wend

	Local report:SESPNowDeliveryReport
	While ESPNowPollDelivery(report)
		If Not report.delivered Then Print "Broadcast delivery failed"
	Wend

	Local now:UInt = MilliSecs()
	If now >= nextSend Then
		sequence :+ 1
		Local message:String = "Hello from " + localName + " #" + sequence
		Check ESPNowBroadcast(UTF8Payload(message)), "Broadcast send failed"
		Print "Sent: " + message
		nextSend = now + 3000
	End If
	Delay 5
Wend
