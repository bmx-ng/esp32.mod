SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP32.Network.WiFi

If WiFiInitialize(WiFiCountryUK) <> 0 Then RuntimeError "WiFi initialization failed"
If WiFiStartScan() <> 0 Then RuntimeError "WiFi scan failed to start"

Print "Scanning for WiFi networks..."
Local complete:Int
While Not complete
	Select WaitEvent()
			Case EVENT_WIFISCANRESULT
				Local network:TWiFiNetwork = TWiFiNetwork(EventExtra())
				Print network.ssid + "  channel=" + network.channel + " rssi=" + network.rssi + " security=" + network.security
			Case EVENT_WIFISCANCOMPLETE
				complete = True
	End Select
Wend

Print "Scan complete; dropped events=" + WiFiDroppedEvents()
WiFiDeinitialize()
