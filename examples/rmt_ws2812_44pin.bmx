SuperStrict

Import BRL.StandardIO
Import ESP32.Hardware.RMT

' This 44-pin S3 board places its single WS2812 on GPIO48.
Const LedPin:UInt = 48
Print "RMT LED example starting on GPIO" + LedPin
Local tx:TRMTTransmitter = RMTCreateTransmitter(LedPin)
If Not tx Then RuntimeError "RMT TX creation failed: " + RMTLastCreateError()

' 10 MHz RMT ticks: a zero is 0.4 us high/0.8 us low, a one the reverse.
Local zeroBit:UInt = RMTMakeSymbol(4, True, 8, False)
Local oneBit:UInt = RMTMakeSymbol(8, True, 4, False)
Local grb:Byte[3]

While True
	For Local color:Int = 0 Until 3
		grb[0] = 0
		grb[1] = 0
		grb[2] = 0
		grb[color] = 16
		Local result:Int = tx.WriteBytes(grb, zeroBit, oneBit)
		If result <> RMTOK Then RuntimeError "WS2812 RMT transmit failed: " + result
		If color = 0 Then Print "RMT LED frame sent"
		Delay(1000)
	Next
Wend
