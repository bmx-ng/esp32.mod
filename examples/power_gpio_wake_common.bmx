SuperStrict

Import BRL.StandardIO
Import ESP32.Hardware.GPIO
Import ESP32.System.Power
Import ESP32.System.Time

Function RunPowerGPIOWakeTest(wakePin:UInt, pinLabel:String)
	Local wakeMask:ULong = ULong(1) Shl wakePin
	If (PowerWakeGPIOs() & wakeMask) <> 0 Then
		While True
			Print "PASS: deep sleep restarted from " + pinLabel
			Print "wake sources=" + PowerWakeCauses() + ", wake GPIO mask=" + PowerWakeGPIOs()
			Delay 2000
		Wend
	End If

	If Not PowerSupports(PowerCapabilitySleepGPIOWake) Then
		RuntimeError "returning GPIO wake is not reported as supported"
	End If
	If SetSleepGPIOWake($ffffffff, True) <> EPowerResult.InvalidArgument Then
		RuntimeError "invalid returning GPIO wake pin was accepted"
	End If

	If Not GPIOInit(wakePin) Or Not GPIOSetInput(wakePin) Or Not GPIOPullDown(wakePin) Then
		RuntimeError "GPIO wake input setup failed"
	End If
	If SetSleepGPIOWake(wakePin, True) <> EPowerResult.Success Then
		RuntimeError "returning GPIO wake setup failed"
	End If

	Print "LIGHT READY: briefly connect " + pinLabel + " to 3V3"
	Delay 100
	Local lightResult:EPowerResult = LowPowerSleepUntilInterrupt()
	If lightResult <> EPowerResult.Success Then
		RuntimeError "returning GPIO wake failed: " + Int(lightResult)
	End If
	If (PowerWakeGPIOs() & wakeMask) = 0 Then
		RuntimeError "returning GPIO wake did not identify its pin"
	End If
	If SetSleepGPIOWake(wakePin, True, False) <> EPowerResult.Success Then
		RuntimeError "returning GPIO wake removal failed"
	End If

	Print "PASS: light sleep returned from " + pinLabel
	Print "Release " + pinLabel + " so its pull-down makes it low"
	While GPIOGet(wakePin)
		Delay 10
	Wend

	Print "DEEP READY: briefly connect " + pinLabel + " to 3V3 again"
	' Native USB consoles may need time to re-enumerate after light sleep.
	Delay 3000
	Local deepResult:EPowerResult = DeepSleepUntilGPIO(wakePin, True)
	RuntimeError "deep sleep GPIO wake failed: " + Int(deepResult)
End Function
