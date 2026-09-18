SuperStrict

Import BRL.StandardIO
Import ESP32.Hardware.GPIO

' The XIAO ESP32-C5's yellow user LED is active-low on GPIO27.
Const LedPin:UInt = 27
If Not GPIOInit(LedPin) Or Not GPIOSetOutput(LedPin) Then
	RuntimeError "Could not configure the XIAO C5 user LED"
End If

While True
	GPIOPut(LedPin, False)
	Print "XIAO C5 LED on"
	Delay(500)
	GPIOPut(LedPin, True)
	Print "XIAO C5 LED off"
	Delay(500)
Wend
