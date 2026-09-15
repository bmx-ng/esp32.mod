SuperStrict

Framework BRL.StandardIO
Import ESP32.Hardware.GPIO

Const TestPin:UInt = 4

Local checksPassed:Int = GPIOIsValid(TestPin) And GPIOIsOutputCapable(TestPin)
checksPassed :& GPIOIsPullCapable(TestPin)
checksPassed :& Not GPIOIsValid($ffffffff) And Not GPIOInit($ffffffff)
checksPassed :& GPIOInit(TestPin)
checksPassed :& GPIOSetOutput(TestPin)
checksPassed :& GPIOGetDirection(TestPin) = GPIOOutput
checksPassed :& GPIOPut(TestPin, False)
checksPassed :& Not GPIOGetOutput(TestPin) And Not GPIOGet(TestPin)
checksPassed :& GPIOPut(TestPin, True)
checksPassed :& GPIOGetOutput(TestPin) And GPIOGet(TestPin)
checksPassed :& GPIOSetDriveStrength(TestPin, GPIODriveCapabilityMedium)
checksPassed :& GPIOGetDriveStrength(TestPin) = GPIODriveCapabilityMedium
checksPassed :& GPIOPullUp(TestPin)
checksPassed :& GPIOIsPulledUp(TestPin) And Not GPIOIsPulledDown(TestPin)
checksPassed :& GPIOPullDown(TestPin)
checksPassed :& Not GPIOIsPulledUp(TestPin) And GPIOIsPulledDown(TestPin)
checksPassed :& GPIODisablePulls(TestPin)
checksPassed :& Not GPIOIsPulledUp(TestPin) And Not GPIOIsPulledDown(TestPin)
checksPassed :& GPIOSetInput(TestPin)
checksPassed :& GPIOGetDirection(TestPin) = GPIOInput

If checksPassed Then
	Print "ESP32 mirrored GPIO checks passed"
Else
	RuntimeError "ESP32 mirrored GPIO check failed"
End If
