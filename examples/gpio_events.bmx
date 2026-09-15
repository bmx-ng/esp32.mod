SuperStrict

Framework BRL.EventQueue
Import ESP32.Hardware.GPIO
Import ESP32.Hardware.GPIOEvents

Const InputPin:UInt = 4

If Not GPIOInit(InputPin) Then RuntimeError "GPIO initialization failed"
If Not GPIOSetInput(InputPin) Then RuntimeError "GPIO input configuration failed"

Local source:TGPIOIRQSource = TGPIOIRQSource.Create(InputPin, ..
	GPIOIRQEdgeRise | GPIOIRQEdgeFall)
If Not source Then RuntimeError "GPIO event source open failed"

' A normal application can now use PollEvent or WaitEvent and inspect
' EventData, EventSource, and GPIOIRQTimeUS(CurrentEvent).
source.Close()
