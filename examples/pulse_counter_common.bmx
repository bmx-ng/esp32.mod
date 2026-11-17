SuperStrict

Import BRL.StandardIO
Import ESP32.Hardware.GPIO
Import ESP32.Hardware.PulseCounter
Import ESP32.System.Time

' PCNT can observe a GPIO while the same pad is driven as an output, making
' this a useful no-jumper hardware smoke test.
Function RunPulseCounterTest(testPin:UInt, pinLabel:String)
	Const ExpectedPulses:Int = 100
	If Not PulseCounterSupported() Then RuntimeError "PCNT is unavailable on this ESP32 target"
	If Not GPIOInit(testPin) Or Not GPIOSetOutput(testPin) Then RuntimeError "test GPIO setup failed"
	GPIOPut testPin, False

	Local counter:TPulseCounter = PulseCounterCreate(-1000, 1000)
	If Not counter Then RuntimeError "PCNT creation failed: " + PulseCounterLastCreateError()
	If counter.AddChannel(Int(testPin)) <> PulseCounterOK Then RuntimeError "PCNT channel setup failed"
	If counter.SetGlitchFilter(1000) <> PulseCounterOK Then RuntimeError "PCNT filter setup failed"
	If counter.Start() <> PulseCounterOK Then RuntimeError "PCNT start failed"

	For Local index:Int = 0 Until ExpectedPulses
		GPIOPut testPin, True
		SleepMicroseconds 10
		GPIOPut testPin, False
		SleepMicroseconds 10
	Next

	If counter.Stop() <> PulseCounterOK Then RuntimeError "PCNT stop failed"
	Local count:Int
	If counter.Read(count) <> PulseCounterOK Then RuntimeError "PCNT read failed"
	Print "PCNT counted " + count + " of " + ExpectedPulses + " pulses on " + pinLabel
	If count <> ExpectedPulses Then RuntimeError "PCNT count mismatch"

	counter.Close()
	Print "ESP32 pulse-counter test passed"
End Function
