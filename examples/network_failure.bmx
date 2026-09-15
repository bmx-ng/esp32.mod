SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import BRL.FileSystem
Import BRL.TextStream
Import BRL.Socket
Import ESP32.Network.WiFi
Import ESP32.Storage.SDCard

Function WaitForLinkStatus:Int(status:Int, timeout:UInt)
	Local started:UInt = MilliSecs()
	Repeat
		PollSystem()
		If WiFiLinkStatus() = status Then Return True
		Delay 10
	Until MilliSecs() - started >= timeout
	Return False
End Function

Function MakeWrongPassword:String(password:String)
	If Not password Then Return ""
	If password[0] = Asc("x") Then Return "y" + password[1..]
	Return "x" + password[1..]
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

If password Then
	Print "Checking authentication failure..."
	result = WiFiConnectWait(ssid, MakeWrongPassword(password), ..
		WiFiAuthenticationWPA2MixedPSK, 1, 20000)
	If result >= 0 Then RuntimeError "Incorrect credentials were not rejected"
	Print "Incorrect credentials rejected with link status " + result
	WiFiDisconnect()
	Delay 1000
End If

Print "Connecting with the configured credentials..."
result = WiFiConnectWait(ssid, password, WiFiAuthenticationWPA2MixedPSK, 3, 20000, 1000)
If result <> 0 Then RuntimeError "Recovery connection failed: " + result
Print "Recovery connection obtained " + WiFiIPv4Address()

Local missing:TAddrInfo[] = AddrInfo("failure-hardening.invalid", "80", AF_INET_)
If missing And missing.length Then RuntimeError "Reserved invalid DNS name unexpectedly resolved"
Print "DNS failure returned an empty result"

Local listener:TSocket = CreateTCPSocket()
If Not listener Or Not BindSocket(listener, 0) Or Not SocketListen(listener, 1) Then RuntimeError "Listener setup failed"
Local started:UInt = MilliSecs()
Local unexpected:TSocket = SocketAccept(listener, 75)
Local elapsed:UInt = MilliSecs() - started
If unexpected Then RuntimeError "Accept unexpectedly returned a socket"
If elapsed < 60 Or elapsed > 1000 Then RuntimeError "Accept timeout was outside its expected range: " + elapsed
Print "Empty accept timed out after " + elapsed + " ms"
CloseSocket listener

Local first:TSocket = CreateUDPSocket()
If Not first Or Not BindSocket(first, 0) Then RuntimeError "First UDP bind failed"
Local second:TSocket = CreateUDPSocket()
If Not second Then RuntimeError "Second UDP socket creation failed"
If BindSocket(second, SocketLocalPort(first)) Then RuntimeError "Conflicting UDP bind unexpectedly succeeded"
CloseSocket second
CloseSocket first
Print "Conflicting bind was rejected"

For Local cycle:Int = 0 Until 40
	Local socket:TSocket = CreateUDPSocket()
	If Not socket Then RuntimeError "Socket allocation failed during reuse cycle " + cycle
	If Not socket.EnableEvents(SocketEventReadable | SocketEventError) Then RuntimeError "Event registration leaked during reuse cycle " + cycle
	socket.DisableEvents()
	CloseSocket socket
Next
Print "Socket and event resources survived 40 reuse cycles"

Local active:TSocket = CreateUDPSocket()
If Not active Then RuntimeError "Active-socket teardown guard setup failed"
If WiFiDeinitialize() = 0 Then RuntimeError "WiFi teardown succeeded while a socket was active"
CloseSocket active
Print "WiFi teardown correctly rejected an active socket"

If WiFiDisconnect() <> 0 Then RuntimeError "WiFi disconnect failed"
If Not WaitForLinkStatus(WiFiLinkDown, 5000) Then RuntimeError "WiFi did not report the disconnected state"
Print "Explicit disconnect reached link-down state"

result = WiFiConnectWait(ssid, password, WiFiAuthenticationWPA2MixedPSK, 3, 20000, 1000)
ssid = ""
password = ""
settings = Null
If result <> 0 Then RuntimeError "Reconnect after explicit disconnect failed: " + result
Print "Reconnect succeeded with " + WiFiIPv4Address()

If WiFiDisconnect() <> 0 Then RuntimeError "Final WiFi disconnect failed"
If Not WaitForLinkStatus(WiFiLinkDown, 5000) Then RuntimeError "Final link-down state timed out"
If WiFiDeinitialize() <> 0 Then RuntimeError "Final WiFi teardown failed"

Print "Network failure hardening test passed"

