SuperStrict

Import BRL.StandardIO
Import ESP32.Random

Local generator:TRandom = CreateRandom("ESP32")
Local bytes:Byte[32]
Local passed:Int = generator And generator.GetName() = "ESP32" And ..
	Not generator.CanSaveState() And ESP32FillRandom(bytes, bytes.length) And ..
	Not ESP32RandomInternalEntropyEnabled()

ESP32RandomEnableInternalEntropy()
passed :& ESP32RandomInternalEntropyEnabled()
passed :& ESP32FillRandom(bytes, bytes.length)
ESP32RandomDisableInternalEntropy()
passed :& Not ESP32RandomInternalEntropyEnabled()

For Local index:Int = 0 Until 256
	Local value:Int = generator.RandomInt(-100, 100)
	passed :& value >= -100 And value <= 100
Next

If passed Then
	Print "ESP32 random checks passed"
Else
	Print "ESP32 random checks failed"
End If
