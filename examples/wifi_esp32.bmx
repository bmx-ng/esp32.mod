SuperStrict

Import BRL.StandardIO
Import ESP32.Network.WiFi

Function RequireSuccess(result:Int, operation:String)
	If result <> 0 Then RuntimeError operation + " failed: " + WiFiResultName(result)
End Function

' Give a serial monitor time to reconnect after a fresh upload.
Delay 1500

RequireSuccess WiFiInitialize(WiFiCountryUK), "WiFi initialization"

Local powerSave:Int
RequireSuccess WiFiGetPowerSave(powerSave), "Reading power-save mode"
Print "Initial power-save mode: " + powerSave
RequireSuccess WiFiSetPowerSave(WiFiPowerSaveNone), "Disabling power save"
RequireSuccess WiFiGetPowerSave(powerSave), "Checking power-save mode"
If powerSave <> WiFiPowerSaveNone Then RuntimeError "Power-save mode did not change"
RequireSuccess WiFiSetPowerSave(WiFiPowerSaveMinimumModem), "Restoring modem power save"

Local transmitPower:Int
RequireSuccess WiFiGetMaximumTransmitPower(transmitPower), "Reading transmit power"
Print "Maximum transmit power: " + transmitPower + " quarter-dBm"
RequireSuccess WiFiSetMaximumTransmitPower(transmitPower), "Restoring transmit power"

Local protocols:UInt
RequireSuccess WiFiGetStationProtocols(protocols), "Reading station protocols"
If protocols = 0 Then RuntimeError "Station protocol bitmap was empty"
Print "Station protocol bitmap: " + protocols
RequireSuccess WiFiSetStationProtocols(protocols), "Restoring station protocols"

Local bandwidth:Int
RequireSuccess WiFiGetStationBandwidth(bandwidth), "Reading station bandwidth"
Print "Station bandwidth: " + bandwidth
RequireSuccess WiFiSetStationBandwidth(bandwidth), "Restoring station bandwidth"

If WiFiStartAccessPoint("BlitzMax-S3", "short", WiFiAuthenticationWPA2AESPSK) = 0 Then ..
	RuntimeError "SoftAP accepted an invalid WPA2 password"
RequireSuccess WiFiStartAccessPoint("BlitzMax-S3", "blitzmax-test", ..
	WiFiAuthenticationWPA2AESPSK, 6, 2), "Starting WPA2 SoftAP"
If Not WiFiAccessPointActive() Then RuntimeError "SoftAP did not become active"

Local clientCount:UInt
RequireSuccess WiFiAccessPointClientCount(clientCount), "Reading SoftAP client count"
Print "SoftAP address: " + WiFiAccessPointIPv4Address()
Print "SoftAP netmask: " + WiFiAccessPointIPv4Netmask()
Print "SoftAP clients: " + clientCount
If Not WiFiAccessPointIPv4Address() Then RuntimeError "SoftAP has no IPv4 address"

RequireSuccess WiFiStopAccessPoint(), "Stopping SoftAP"
If WiFiAccessPointActive() Then RuntimeError "SoftAP remained active"
If WiFiAccessPointIPv4Address() Then RuntimeError "Inactive SoftAP exposed an address"
RequireSuccess WiFiDeinitialize(), "WiFi teardown"
RequireSuccess WiFiInitialize(WiFiCountryUK), "WiFi reinitialization"
RequireSuccess WiFiDeinitialize(), "Second WiFi teardown"

Print "ESP32 WiFi controls and SoftAP test passed"
