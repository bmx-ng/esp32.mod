SuperStrict

Import BRL.StandardIO
Import ESP32.Hardware.RMT

' Connect an oscilloscope or logic analyser to GPIO4 to see these pulses.
Const OutputPin:UInt = 4
Local tx:TRMTTransmitter = RMTCreateTransmitter(OutputPin)
If Not tx Then RuntimeError "RMT TX creation failed: " + RMTLastCreateError()

Local shortPulse:UInt = RMTMakeSymbol(5, True, 5, False)
Local longPulse:UInt = RMTMakeSymbol(15, True, 15, False)
If RMTSymbolDuration0(shortPulse) <> 5 Or RMTSymbolLevel1(shortPulse) <> 0 Then
	RuntimeError "RMT symbol packing failed"
End If

Local symbols:UInt[4]
symbols[0] = shortPulse
symbols[1] = longPulse
symbols[2] = shortPulse
symbols[3] = longPulse

While True
	Local result:Int = tx.WriteSymbols(symbols)
	If result <> RMTOK Then RuntimeError "RMT transmit failed: " + result
	Delay(1000)
Wend
