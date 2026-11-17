SuperStrict

Framework BRL.StandardIO
Import Embedded.Network.WiFi
Import ESP32.Network.ESPNow

Local result:Int = WiFiInitialize()
If result <> 0 Then RuntimeError "Wi-Fi initialization failed: " + ESPNowResultName(result)

result = ESPNowInitialize()
If result <> 0 Then RuntimeError "ESP-NOW initialization failed: " + ESPNowResultName(result)

Local address:Byte[ESPNowAddressLength]
result = ESPNowLocalAddress(address)
If result <> 0 Then RuntimeError "MAC address query failed: " + ESPNowResultName(result)

Local version:UInt
result = ESPNowVersion(version)
If result <> 0 Then RuntimeError "ESP-NOW version query failed: " + ESPNowResultName(result)

ESPNowSetChannel(1)
Local channel:Int
ESPNowGetChannel(channel)

Print "ESP-NOW ready"
Print "MAC: " + ESPNowFormatAddress(address)
Print "Version: " + version
Print "Channel: " + channel

While True
	Delay 1000
Wend
