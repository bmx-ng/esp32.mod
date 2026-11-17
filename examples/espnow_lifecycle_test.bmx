SuperStrict

Framework BRL.StandardIO
Import Embedded.Network.WiFi
Import ESP32.Network.ESPNow

Function Check:Int(result:Int, operation:String)
	If result <> 0 Then RuntimeError operation + " failed: " + ESPNowResultName(result) + " (" + result + ")"
	Return result
End Function

Check WiFiInitialize(), "Wi-Fi initialization"
Check ESPNowInitialize(), "ESP-NOW initialization"

If Not ESPNowInitialized() Then RuntimeError "ESP-NOW did not report initialized"

Local result:Int = WiFiDeinitialize()
If result = 0 Then RuntimeError "Wi-Fi deinitialized while ESP-NOW was active"
Print "Wi-Fi shutdown correctly refused while ESP-NOW was active: " + ESPNowResultName(result)

Check ESPNowDeinitialize(), "ESP-NOW deinitialization"
If ESPNowInitialized() Then RuntimeError "ESP-NOW still reports initialized after shutdown"

result = WiFiDeinitialize()
If result <> 0 Then RuntimeError "Wi-Fi deinitialization failed after ESP-NOW shutdown: " + result

Print "ESP-NOW lifecycle test passed"
