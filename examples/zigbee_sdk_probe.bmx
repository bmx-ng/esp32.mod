SuperStrict

Framework BRL.StandardIO
Import ESP32.Network.Zigbee

Print "ESP Zigbee SDK major version: " + ZigbeeSdkMajorVersion()
Local result:Int = ZigbeeBringupStart()
Print "Zigbee start result: " + result
If result Then RuntimeError "Zigbee start failed: " + result
While True
	Delay 10000
	ZigbeeBringupToggle()
	Delay 500
	ZigbeeBringupRead()
Wend
