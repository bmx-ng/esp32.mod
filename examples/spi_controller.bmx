SuperStrict

Import BRL.StandardIO
Import ESP32.Hardware.SPI

Local passed:Int = SPIControllerCount() >= 1
passed :& SPIConfigurePins(SPIController0, 14, 15, 16)
Local actualRate:UInt = SPIInit(SPIController0, 1000000)
passed :& actualRate > 0
passed :& SPISetFormat(SPIController0, 8, SPIClockPolarity0, SPIClockPhase0, SPIBitOrderMSBFirst)

' The controller supplies the clock, so transfers complete without a connected
' peripheral. Floating MISO data is deliberately ignored.
Local outgoing:Byte[4]
Local incoming:Byte[4]
outgoing[0] = $9f
outgoing[1] = $aa
outgoing[2] = $55
outgoing[3] = 0
passed :& SPIWriteReadBlocking(SPIController0, outgoing, incoming, 4) = 4
passed :& SPIWriteBlocking(SPIController0, outgoing, 4) = 4
passed :& SPIReadBlocking(SPIController0, $ff, incoming, 4) = 4

Local outgoing16:Short[2]
Local incoming16:Short[2]
outgoing16[0] = $1234
outgoing16[1] = $55aa
passed :& SPISetFormat(SPIController0, 16, SPIClockPolarity1, SPIClockPhase1, SPIBitOrderLSBFirst)
passed :& SPIWrite16Read16Blocking(SPIController0, outgoing16, incoming16, 2) = 2
passed :& SPIWrite16Blocking(SPIController0, outgoing16, 2) = 2
passed :& SPIRead16Blocking(SPIController0, $ffff, incoming16, 2) = 2
passed :& SPISetBaudrate(SPIController0, 2000000) > 0
passed :& SPIGetBaudrate(SPIController0) > 0
passed :& SPIDeinit(SPIController0)

While True
	If passed Then
		Print "ESP32 SPI controller checks passed"
	Else
		Print "ESP32 SPI controller checks failed"
	End If
	Delay(1000)
Wend
