SuperStrict

Import BRL.StandardIO
Import ESP32.Hardware.I2C

Local passed:Int = I2CControllerCount() >= 1
passed :& I2CConfigurePins(I2CController0, 4, 5, True)
Local actualRate:UInt = I2CInit(I2CController0, I2CSpeedStandard)
passed :& actualRate = I2CSpeedStandard
passed :& I2CSetBaudrate(I2CController0, I2CSpeedFast) = I2CSpeedFast

' No peripheral is expected at the reserved address. This validates bounded
' driver transactions and error translation without external wiring.
Local register:Byte = 0
Local value:Byte
Local result:Int = I2CWriteTimeout(I2CController0, $7f, Varptr register, 1, 2000)
passed :& result = I2CErrorGeneric Or result = I2CErrorTimeout
result = I2CWriteReadTimeout(I2CController0, $7f, Varptr register, 1, Varptr value, 1, 2000)
passed :& result = I2CErrorGeneric Or result = I2CErrorTimeout
passed :& I2CDeinit(I2CController0)

While True
	If passed Then
		Print "ESP32 I2C controller checks passed"
	Else
		Print "ESP32 I2C controller checks failed"
	End If
	Delay(1000)
Wend
