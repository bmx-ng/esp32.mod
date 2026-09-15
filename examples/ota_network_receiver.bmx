' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Framework BRL.EventQueue
Import BRL.FileSystem
Import BRL.SocketStream
Import BRL.StandardIO
Import BRL.TextStream
Import ESP32.Network.WiFi
Import ESP32.Storage.SDCard
Import ESP32.System.Device
Import ESP32.System.OTA

Const OTAPort:Int = 8765

If Not MountSDCard() Then RuntimeError "Could not mount the SD card"
Local settings:String[] = LoadText("sd::wifi.conf").Replace("~r", "").Split("~n")
If settings.length < 2 Or Not settings[0] Then RuntimeError "sd::wifi.conf must contain an SSID and password"

Local result:Int = WiFiInitialize(WiFiCountryUK)
If result = 0 Then result = WiFiConnectWait(settings[0], settings[1], WiFiAuthenticationWPA2MixedPSK, 3, 20000, 1000)
settings = Null
If result <> 0 Then RuntimeError "Wi-Fi connection failed: " + WiFiResultName(result)
If OTAApplicationSlotCount() < 2 Or Not OTAIsSupported() Then RuntimeError "The partition table does not support OTA updates"

Local listener:TSocket = CreateTCPSocket()
If Not listener Or Not BindSocket(listener, OTAPort, AF_INET_) Or Not SocketListen(listener, 1) Then RuntimeError "Could not start OTA listener"
Print "Waiting for an OTA image at " + WiFiIPv4Address() + ":" + OTAPort

Local client:TSocket
While Not client
	client = SocketAccept(listener, 1000)
Wend
CloseSocket listener

Local header:Byte[4]
Local received:Int
While received < header.length
	Local count:Int = client.Recv(Varptr header[received], header.length - received)
	If count <= 0 Then RuntimeError "OTA image-size header was incomplete"
	received :+ count
Wend
Local imageSize:UInt = UInt(header[0]) | (UInt(header[1]) Shl 8) | (UInt(header[2]) Shl 16) | (UInt(header[3]) Shl 24)
If imageSize = 0 Then RuntimeError "OTA image size was invalid"

Local update:TESP32OTAUpdate = BeginOTAUpdate(imageSize)
If Not update Then RuntimeError "Could not begin OTA update: " + OTAResultName(OTALastBeginError())
Print "Receiving " + imageSize + " bytes into " + update.TargetPartition()

Local source:TSocketStream = CreateSocketStream(client, False)
result = update.WriteStream(source, imageSize)
If result = 0 Then result = update.Finish()
If result = 0 Then result = update.Activate()

Local reply:String
If result = 0 Then
	reply = "OTA image validated and selected~n"
Else
	reply = "OTA update failed: " + OTAResultName(result) + "~n"
End If
Local replySize:Size_T
Local replyBytes:Byte Ptr = reply.ToUTF8String(replySize)
client.Send(replyBytes, replySize)
MemFree(replyBytes)
CloseSocket client

If result <> 0 Then
	update.Abort()
	RuntimeError reply
End If
update.Close()
Print "Rebooting into the new image"
Delay 250
Reboot()
