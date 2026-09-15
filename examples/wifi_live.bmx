SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import BRL.FileSystem
Import BRL.TextStream
Import BRL.Socket
Import ESP32.Network.WiFi
Import ESP32.Storage.SDCard

Function WaitForSocketEvent:Int(eventId:Int, source:Object, timeout:UInt)
	Local started:UInt = MilliSecs()
	Repeat
		Local current:Int = PollEvent()
		If current = eventId And EventSource() = source Then Return True
		If current = EVENT_SOCKETERROR And EventSource() = source Then Return False
		Delay 1
	Until MilliSecs() - started >= timeout
	Return False
End Function

' Give a serial monitor time to reconnect after a fresh upload.
Delay 1500

If Not MountSDCard() Then RuntimeError "SD card mount failed: " + SDCardLastError()
Local settings:String[] = LoadText("sd::wifi.conf").Replace("~r", "").Split("~n")
If settings.length < 2 Or Not settings[0] Then RuntimeError "Expected SSID and password on the first two lines of sd::wifi.conf"

Local ssid:String = settings[0]
Local password:String = settings[1]
Local result:Int = WiFiInitialize(WiFiCountryUK)
If result <> 0 Then RuntimeError "WiFi initialization failed: " + result

Print "Connecting to WiFi..."
result = WiFiConnectWait(ssid, password, WiFiAuthenticationWPA2MixedPSK, 3, 20000, 1000)
Local stationInfo:SESP32WiFiStationInfo
If result = 0 Then result = WiFiGetStationInfo(stationInfo)
If result = 0 And stationInfo.ssid <> ssid Then result = -1
' Drop the only managed references to the credentials as soon as association is complete.
ssid = ""
password = ""
settings = Null
If result <> 0 Then RuntimeError "WiFi connection failed: " + result

Print "WiFi connected"
Print "Address: " + WiFiIPv4Address()
Print "Netmask: " + WiFiIPv4Netmask()
Print "Gateway: " + WiFiIPv4Gateway()
Print "Channel: " + stationInfo.channel + ", RSSI: " + stationInfo.rssi + " dBm"
Print "Bandwidth: " + stationInfo.bandwidth + ", protocols: " + stationInfo.protocols

' ESP32 can keep the upstream station connection while serving local clients.
result = WiFiStartAccessPoint("BlitzMax-S3-live", "", WiFiAuthenticationOpen, 6, 2)
If result <> 0 Then RuntimeError "SoftAP start failed: " + WiFiResultName(result)
If WiFiLinkStatus() <> WiFiLinkUp Then RuntimeError "Starting SoftAP dropped the station link"
Print "Concurrent SoftAP address: " + WiFiAccessPointIPv4Address()

' A DNS lookup and NTP exchange verify real UDP traffic beyond the local stack.
Local udpHints:TAddrInfo = New TAddrInfo(AF_INET_, SOCK_DGRAM_)
Local addresses:TAddrInfo[] = AddrInfo("time.cloudflare.com", "123", udpHints)
If Not addresses Or addresses.length = 0 Then RuntimeError "NTP DNS lookup failed"
Print "DNS resolved time.cloudflare.com to " + addresses[0].HostIp()

Local udp:TSocket = TSocket.Create(addresses[0])
If Not udp Or Not ConnectSocket(udp, addresses[0]) Then RuntimeError "NTP UDP connect failed"
If Not udp.EnableEvents(SocketEventReadable | SocketEventError) Then RuntimeError "NTP UDP events failed"
Local request:Byte[48]
request[0] = $1b
If udp.Send(request, request.length) <> request.length Then RuntimeError "NTP request failed"
If Not WaitForSocketEvent(EVENT_SOCKETREADABLE, udp, 10000) Then RuntimeError "NTP response timed out"
Local response:Byte[48]
If udp.Recv(response, response.length) < 48 Then RuntimeError "NTP response was incomplete"
CloseSocket udp
Print "UDP/NTP exchange passed"

' A separate DNS lookup and HTTP request verify a real TCP stream.
Local tcpHints:TAddrInfo = New TAddrInfo(AF_INET_, SOCK_STREAM_)
addresses = AddrInfo("example.com", "80", tcpHints)
If Not addresses Or addresses.length = 0 Then RuntimeError "HTTP DNS lookup failed"
Print "DNS resolved example.com to " + addresses[0].HostIp()

Local tcp:TSocket = TSocket.Create(addresses[0])
If Not tcp Or Not ConnectSocket(tcp, addresses[0]) Then RuntimeError "HTTP TCP connect failed"
If Not tcp.EnableEvents(SocketEventReadable | SocketEventClosed | SocketEventError) Then RuntimeError "HTTP TCP events failed"
Local httpRequest:String = "GET / HTTP/1.0~r~nHost: example.com~r~nConnection: close~r~n~r~n"
Local requestLength:Size_T
Local requestBytes:Byte Ptr = httpRequest.ToUTF8String(requestLength)
Local sent:Long = tcp.Send(requestBytes, requestLength)
MemFree(requestBytes)
If sent <> requestLength Then RuntimeError "HTTP request failed"
If Not WaitForSocketEvent(EVENT_SOCKETREADABLE, tcp, 10000) Then RuntimeError "HTTP response timed out"
Local httpResponse:Byte[512]
Local received:Long = tcp.Recv(httpResponse, httpResponse.length)
If received <= 0 Then RuntimeError "HTTP response was empty"
CloseSocket tcp
Print "TCP/HTTP exchange passed; received " + received + " bytes"

If WiFiStopAccessPoint() <> 0 Then RuntimeError "SoftAP stop failed"
If WiFiDisconnect() <> 0 Then RuntimeError "WiFi disconnect failed"
If WiFiDeinitialize() <> 0 Then RuntimeError "WiFi teardown failed"
Print "Live WiFi networking test passed"
