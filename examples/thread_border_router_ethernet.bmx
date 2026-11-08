' S31 Thread border router with Ethernet backbone and a mesh UDP echo port.
SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP32.Network.ThreadBorderRouter
Import ESP32.Network.Ethernet
Import Embedded.Network.Thread

Local result:Int = EthernetInitialize()
If result <> 0 Then RuntimeError "Ethernet initialization failed: " + result
If EthernetWaitForIP(20000) <> 0 Then RuntimeError "Ethernet DHCP timed out"
Print "BORDER_ROUTER_ETHERNET=connected"
Print "BACKBONE_IPV4=" + EthernetIPv4Address()

result = ThreadBorderRouterUseEthernet()
If result <> 0 Then RuntimeError "Backbone setup failed: " + result
result = ThreadStartSaved()
If result = ThreadErrorNoSavedNetwork Then result = ThreadForm("BMXThread", 15)
If result <> 0 Then RuntimeError "Thread start failed: " + result
If ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread attach failed"

result = ThreadBorderRouterStart()
If result <> 0 Then RuntimeError "Border router start failed: " + result
Print "BORDER_ROUTER=running"
Delay 3000
For Local index:Int = 0 Until 10
	Local address:String = ThreadIPv6Address(index)
	If address = "" Then Exit
	Print "THREAD_ADDRESS=" + address
Next

Local socket:TThreadUDPSocket = TThreadUDPSocket.Open(12345)
If Not socket Then RuntimeError "Thread UDP bind failed"
Local packet:Byte[512]
While True
	Local source:String, port:Int
	Local length:Int = socket.Receive(packet, source, port)
	If length >= 0 Then
		Print "THREAD_RX=" + length
		Print "THREAD_ECHO=" + socket.Send(source, port, packet, length)
	End If
	Delay 20
Wend
