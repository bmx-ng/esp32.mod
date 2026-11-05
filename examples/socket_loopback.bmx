SuperStrict

Framework BRL.StandardIO
Import BRL.EventQueue
Import BRL.Socket

Function WaitForSocketEvent:Int(eventId:Int, source:Object, timeout:UInt = 1000)
	Local started:UInt = MilliSecs()
	Repeat
		Local current:Int = PollEvent()
		If current = eventId And EventSource() = source Then Return True
		If current = EVENT_SOCKETERROR And EventSource() = source Then Return False
		Delay 1
	Until MilliSecs() - started >= timeout
	Return False
End Function

' Leave time for a serial monitor to attach after upload/reset.
Delay 1000

Local receiver:TSocket = CreateUDPSocket()
If Not receiver Or Not BindSocket(receiver, 0) Then RuntimeError "UDP bind failed"

Local addresses:TAddrInfo[] = AddrInfo("127.0.0.1", String(SocketLocalPort(receiver)), AF_INET_)
If Not addresses Or addresses.length = 0 Then RuntimeError "Loopback resolution failed"

Local sender:TSocket = CreateUDPSocket()
If Not sender Or Not ConnectSocket(sender, addresses[0]) Then RuntimeError "UDP connect failed"

Local outgoing:Byte Ptr = MemAlloc(4)
Local incoming:Byte Ptr = MemAlloc(4)
If Not outgoing Or Not incoming Then RuntimeError "Socket buffer allocation failed"
outgoing[0] = 66
outgoing[1] = 77
outgoing[2] = 88
If sender.Send(outgoing, Size_T(3)) <> 3 Then RuntimeError "UDP send failed"

If receiver.Recv(incoming, Size_T(3)) <> 3 Then RuntimeError "UDP receive failed"
If incoming[0] <> 66 Or incoming[1] <> 77 Or incoming[2] <> 88 Then RuntimeError "UDP payload mismatch"

Print "UDP loopback passed on port " + SocketLocalPort(receiver)
CloseSocket sender
CloseSocket receiver

Local listener:TSocket = CreateTCPSocket()
If Not listener Or Not BindSocket(listener, 0, AF_INET_) Then RuntimeError "TCP bind failed"
If Not SocketListen(listener, 1) Then RuntimeError "TCP listen failed"
If Not listener.EnableEvents(SocketEventAccept | SocketEventError) Then RuntimeError "TCP listener events failed"

Local hints:TAddrInfo = New TAddrInfo(AF_INET_, SOCK_STREAM_)
addresses = AddrInfo("127.0.0.1", String(SocketLocalPort(listener)), hints)
If Not addresses Or addresses.length = 0 Then RuntimeError "TCP loopback resolution failed"

Local client:TSocket = TSocket.Create(addresses[0])
If Not client Or Not ConnectSocket(client, addresses[0]) Then RuntimeError "TCP connect failed"
If Not WaitForSocketEvent(EVENT_SOCKETACCEPT, listener) Then RuntimeError "TCP accept event failed"
Local server:TSocket = SocketAccept(listener, 1000)
If Not server Then RuntimeError "TCP accept failed"
If Not client.EnableEvents(SocketEventWritable | SocketEventError) Then RuntimeError "TCP client events failed"
If Not server.EnableEvents(SocketEventReadable | SocketEventClosed | SocketEventError) Then RuntimeError "TCP server events failed"
If Not WaitForSocketEvent(EVENT_SOCKETWRITABLE, client) Then RuntimeError "TCP writable event failed"

outgoing[0] = 11
outgoing[1] = 22
outgoing[2] = 33
outgoing[3] = 44
If client.Send(outgoing, Size_T(4)) <> 4 Then RuntimeError "TCP send failed"
If Not WaitForSocketEvent(EVENT_SOCKETREADABLE, server) Then RuntimeError "TCP readable event failed"
Local available:Int = SocketReadAvail(server)
Print "TCP bytes available: " + available
If available < 1 Then RuntimeError "TCP available byte count failed"
Local received:Int
While received < 4
	Local count:Int = server.Recv(incoming + received, Size_T(4 - received))
	If count <= 0 Then RuntimeError "TCP receive failed"
	received :+ count
Wend
For Local index:Int = 0 Until 4
	If incoming[index] <> outgoing[index] Then RuntimeError "TCP payload mismatch"
Next

Print "TCP loopback passed on port " + SocketLocalPort(listener)
CloseSocket client
If Not WaitForSocketEvent(EVENT_SOCKETCLOSED, server) Then RuntimeError "TCP closed event failed"
CloseSocket server
CloseSocket listener
MemFree(incoming)
MemFree(outgoing)
