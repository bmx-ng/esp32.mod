SuperStrict

Framework BRL.StandardIO
Import ESP32.Hardware.UART

Const TestController:Int = UARTController1
Const TestTXPin:UInt = 17
Const TestRXPin:UInt = 18

Local checksPassed:Int = UARTControllerCount() = 3
checksPassed :& UARTSupportsFlexiblePinMappings()
Local defaultController:Int = UARTDefaultController()
checksPassed :& (defaultController = UARTUnavailableController And UARTDefaultBaudrate() = 0) Or ..
	(defaultController >= 0 And defaultController < UARTControllerCount() And UARTDefaultBaudrate() > 0)
checksPassed :& Not UARTConfigurePins(3, TestTXPin, TestRXPin)
checksPassed :& UARTConfigurePins(TestController, TestTXPin, TestRXPin)

Local actualRate:UInt = UARTInit(TestController, 115200)
checksPassed :& actualRate >= 114000 And actualRate <= 116000
checksPassed :& UARTIsEnabled(TestController) And UARTIsWritable(TestController)
checksPassed :& UARTSetFIFOEnabled(TestController, True)
checksPassed :& Not UARTSetFIFOEnabled(TestController, False)
actualRate = UARTSetBaudrate(TestController, 230400)
checksPassed :& actualRate >= 228000 And actualRate <= 233000
checksPassed :& UARTGetBaudrate(TestController) = actualRate
checksPassed :& UARTSetFormat(TestController, 7, 2, UARTParityEven)
checksPassed :& UARTSetFormat(TestController, 8, 1, UARTParityNone)
checksPassed :& Not UARTSetFormat(TestController, 9, 1, UARTParityNone)
checksPassed :& UARTSetFlowControl(TestController, False, False)
Local persistentAsserted:Int = UARTSetBreak(TestController, True)
Local persistentState:Int = UARTIsBreakAsserted(TestController)
Local writableDuringBreak:Int = UARTIsWritable(TestController)
checksPassed :& persistentAsserted
checksPassed :& persistentState And Not writableDuringBreak
Delay 1
Local persistentReleased:Int = UARTSetBreak(TestController, False)
Local releasedState:Int = UARTIsBreakAsserted(TestController)
Local writableAfterBreak:Int = UARTIsWritable(TestController)
checksPassed :& persistentReleased
checksPassed :& Not releasedState And writableAfterBreak
checksPassed :& UARTSetLoopback(TestController, True)

Local outgoing:Byte[5]
outgoing[0] = Asc("U")
outgoing[1] = Asc("A")
outgoing[2] = Asc("R")
outgoing[3] = Asc("T")
outgoing[4] = Asc("1")
checksPassed :& UARTWriteBlocking(TestController, outgoing, outgoing.length) = outgoing.length
UARTTXWaitBlocking(TestController)

Local incoming:Byte[5]
checksPassed :& UARTIsReadableWithin(TestController, 100000)
checksPassed :& UARTReadTimeout(TestController, incoming, incoming.length, 100000) = incoming.length
For Local index:Int = 0 Until outgoing.length
	checksPassed :& incoming[index] = outgoing[index]
Next

Local breakByte:Byte[1]
breakByte[0] = Asc("B")
Local finiteBreakWritten:Int = UARTWriteWithBreak(TestController, breakByte, 1, 10)
checksPassed :& finiteBreakWritten = 1
UARTTXWaitBlocking(TestController)
Local finiteBreakRead:Int = UARTReadTimeout(TestController, incoming, 1, 100000)
checksPassed :& finiteBreakRead = 1
checksPassed :& incoming[0] = breakByte[0]
Delay 1 ' The RX byte and the driver's break event use separate native queues.
checksPassed :& (UARTGetErrors(TestController) & UARTErrorBreak) <> 0
checksPassed :& UARTWriteWithBreak(TestController, breakByte, 1, 0) = UARTErrorInvalidArgument
UARTClearErrors(TestController)
checksPassed :& UARTFlushInput(TestController)

checksPassed :& UARTSetTranslateCRLF(TestController, True)
Local line:Byte[2]
line[0] = Asc("X")
line[1] = 10
checksPassed :& UARTWriteBlocking(TestController, line, line.length) = line.length
UARTTXWaitBlocking(TestController)
Local translated:Byte[3]
checksPassed :& UARTReadBlocking(TestController, translated, translated.length) = translated.length
checksPassed :& translated[0] = Asc("X") And translated[1] = 13 And translated[2] = 10
checksPassed :& UARTSetTranslateCRLF(TestController, False)

checksPassed :& UARTReadAvailable(TestController, incoming, incoming.length) = 0
checksPassed :& UARTReadTimeout(TestController, incoming, incoming.length, 1000) = 0
checksPassed :& Not UARTIsReadable(TestController)
checksPassed :& UARTGetErrors(TestController) = 0
UARTClearErrors(TestController)
checksPassed :& UARTFlushInput(TestController)
checksPassed :& UARTSetLoopback(TestController, False)
checksPassed :& UARTWriteBlocking(3, outgoing, 1) = UARTErrorInvalidArgument
checksPassed :& UARTDeinit(TestController)
checksPassed :& Not UARTIsEnabled(TestController) And Not UARTDeinit(TestController)

If checksPassed Then
	Print "ESP32 UART configuration and loopback checks passed"
Else
	RuntimeError "ESP32 UART configuration or loopback check failed"
End If
