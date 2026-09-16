SuperStrict

Import BRL.StandardIO
Import ESP32.Hardware.RMT

' Connect GPIO4 (TX) to GPIO5 (RX) with a short jumper wire.
Const TxPin:UInt = 4
Const RxPin:UInt = 5
Const ClockHz:UInt = 1000000

Local tx:TRMTTransmitter = RMTCreateTransmitter(TxPin, ClockHz)
If Not tx Then RuntimeError "RMT TX creation failed: " + RMTLastCreateError()
Local rx:TRMTReceiver = RMTCreateReceiver(RxPin, 32, ClockHz)
If Not rx Then RuntimeError "RMT RX creation failed: " + RMTLastCreateError()

' Each tick is 1 us. The 200 us idle gap ends the received frame.
Local pulses:UInt[3]
pulses[0] = RMTMakeSymbol(20, True, 30, False)
pulses[1] = RMTMakeSymbol(40, True, 50, False)
pulses[2] = RMTMakeSymbol(60, True, 70, False)

If rx.Start(1000, 200000) <> RMTOK Then RuntimeError "RMT receive start failed"
If tx.WriteSymbols(pulses) <> RMTOK Then RuntimeError "RMT transmit failed"

For Local attempt:Int = 0 Until 100
	If rx.Ready() Then Exit
	Delay(1)
Next
If Not rx.Ready() Then RuntimeError "RMT receive timed out"

Local received:UInt[] = rx.ReadSymbols()
If Not received Then RuntimeError "RMT receive read failed"
If received.length <> pulses.length Then RuntimeError "Unexpected RMT symbol count: " + received.length
For Local index:Int = 0 Until received.length
	If RMTSymbolDuration0(received[index]) <> RMTSymbolDuration0(pulses[index]) Or ..
		RMTSymbolLevel0(received[index]) <> RMTSymbolLevel0(pulses[index]) Then
		RuntimeError "RMT first half-pulse mismatch at symbol " + index
	End If
	' The final low level runs into the idle gap, so it has no closing edge.
	If index < received.length - 1 Then
		If RMTSymbolDuration1(received[index]) <> RMTSymbolDuration1(pulses[index]) Or ..
			RMTSymbolLevel1(received[index]) <> RMTSymbolLevel1(pulses[index]) Then
			RuntimeError "RMT second half-pulse mismatch at symbol " + index
		End If
	End If
Next
Print "Received " + received.length + " symbols"
For Local index:Int = 0 Until received.length
	Local symbol:UInt = received[index]
	Print RMTSymbolDuration0(symbol) + "/" + RMTSymbolLevel0(symbol) + " " + RMTSymbolDuration1(symbol) + "/" + RMTSymbolLevel1(symbol)
Next
Print "RMT loopback passed"

rx.Close()
tx.Close()
