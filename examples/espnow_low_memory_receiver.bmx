SuperStrict

Framework BRL.StandardIO
Import Embedded.Network.WiFi
Import ESP32.Network.ESPNow

Const Channel:Int = 1

Local result:Int = WiFiInitialize()
If result <> 0 Then RuntimeError "Wi-Fi initialization failed: " + ESPNowResultName(result)

result = ESPNowSetChannel(Channel)
If result <> 0 Then RuntimeError "Channel selection failed: " + ESPNowResultName(result)

' Two 250-byte receive slots reserve only 500 payload bytes. Packets above the
' v1 limit are deliberately discarded and included in the dropped count.
result = ESPNowInitialize(2, 4, ESPNowMaximumPayloadV1)
If result <> 0 Then RuntimeError "ESP-NOW initialization failed: " + ESPNowResultName(result)

Print "Low-memory ESP-NOW receiver ready on channel " + Channel

Local lastDropped:UInt
While True
	Local packet:SESPNowReceivedPacket
	While ESPNowPollReceived(packet)
		Print "RX " + packet.data.length + " bytes from " + ..
			ESPNowFormatAddress(packet.sourceAddress)
	Wend

	Local dropped:UInt = ESPNowDroppedReceived()
	If dropped <> lastDropped Then
		Print "Dropped received packets: " + dropped
		lastDropped = dropped
	End If
	Delay 5
Wend
