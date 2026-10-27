SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import BRL.Socket
Import ESP32.Network.Ethernet

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

Function HexByte:String(value:Byte)
	Const digits:String = "0123456789abcdef"
	Return Chr(digits[(value Shr 4) & $f]) + Chr(digits[value & $f])
End Function

Function MACString:String(mac:Byte[])
	If Not mac Or mac.length <> 6 Then Return ""
	Local value:String
	For Local index:Int = 0 Until mac.length
		If index Then value :+ ":"
		value :+ HexByte(mac[index])
	Next
	Return value
End Function

' Give a serial monitor time to reconnect after upload/reset.
Delay 1500

Local result:Int = EthernetInitialize()
If result <> 0 Then RuntimeError "Ethernet initialization failed: " + EthernetResultName(result)
Print "Waiting for Ethernet DHCP..."
If EthernetWaitForIP(20000) <> 0 Then RuntimeError "Ethernet DHCP timed out"

Local phyAddress:Int
result = EthernetPHYAddress(phyAddress)
If result <> 0 Then RuntimeError "Ethernet PHY query failed: " + EthernetResultName(result)
Print "Ethernet connected"
Print "Address: " + EthernetIPv4Address()
Print "Netmask: " + EthernetIPv4Netmask()
Print "Gateway: " + EthernetIPv4Gateway()
Print "MAC: " + MACString(EthernetMACAddress())
Print "PHY address: " + phyAddress
Print "Link: " + EthernetLinkSpeed() + " Mbps, full duplex: " + EthernetFullDuplex()
Local routePriority:Int = EthernetRoutePriority()
Print "Route priority: " + routePriority
If routePriority < 0 Then RuntimeError "Ethernet route priority query failed"
If EthernetSetRoutePriority(routePriority) <> routePriority Then RuntimeError "Ethernet route priority update failed"
result = EthernetMakeDefault()
If result <> 0 Or Not EthernetIsDefault() Then RuntimeError "Ethernet default-route selection failed"

' DNS and NTP verify real UDP traffic through the ordinary BRL.Socket API.
Local udpHints:TAddrInfo = New TAddrInfo(AF_INET_, SOCK_DGRAM_)
Local addresses:TAddrInfo[] = AddrInfo("time.cloudflare.com", "123", udpHints)
If Not addresses Or addresses.length = 0 Then RuntimeError "NTP DNS lookup failed"
Print "DNS resolved time.cloudflare.com to " + addresses[0].HostIp()

Local udp:TSocket = TSocket.Create(addresses[0])
If Not udp Or Not ConnectSocket(udp, addresses[0]) Then RuntimeError "NTP UDP connect failed"
If Not udp.EnableEvents(SocketEventReadable | SocketEventError) Then RuntimeError "NTP UDP events failed"
Local request:Byte[48]
request[0] = $1b
If udp.Send(Varptr request[0], Size_T(request.length)) <> request.length Then RuntimeError "NTP request failed"
If Not WaitForSocketEvent(EVENT_SOCKETREADABLE, udp, 10000) Then RuntimeError "NTP response timed out"
Local response:Byte[48]
If udp.Recv(Varptr response[0], Size_T(response.length)) < 48 Then RuntimeError "NTP response was incomplete"
CloseSocket udp
Print "UDP/NTP exchange passed"

' A separate lookup and HTTP request verify TCP and Internet routing.
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
Local received:Long = tcp.Recv(Varptr httpResponse[0], Size_T(httpResponse.length))
If received <= 0 Then RuntimeError "HTTP response was empty"
CloseSocket tcp
Print "TCP/HTTP exchange passed; received " + received + " bytes"

' Teardown must not invalidate sockets which still belong to lwIP.
Local guardSocket:TSocket = CreateUDPSocket()
If Not guardSocket Then RuntimeError "Ethernet teardown guard socket creation failed"
If EthernetDeinitialize() = 0 Then RuntimeError "Ethernet teardown accepted an active socket"
CloseSocket guardSocket
If EthernetDeinitialize() <> 0 Then RuntimeError "Ethernet teardown failed"
Print "Live Ethernet networking test passed"
