' S31 Thread border router with Wi-Fi backbone and a mesh UDP echo port.
SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import ESP32.Network.ThreadBorderRouter
Import ESP32.Network.WiFi
Import Embedded.Network.Thread

Include "thread_border_router_wifi.local.bmx"

If BorderRouterWiFiSSID = "" Then RuntimeError "Set the Wi-Fi SSID in thread_border_router_wifi.local.bmx"
Local result:Int = WiFiInitialize()
If result <> 0 Then RuntimeError "Wi-Fi initialization failed: " + result
result = WiFiConnectWait(BorderRouterWiFiSSID, BorderRouterWiFiPassword)
If result <> 0 Then RuntimeError "Wi-Fi connection failed: " + result
result = WiFiSetPowerSave(WiFiPowerSaveNone)
If result <> 0 Then RuntimeError "Wi-Fi power-save setup failed: " + result
Print "BORDER_ROUTER_WIFI=connected"

result = ThreadBorderRouterUseWiFi()
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
