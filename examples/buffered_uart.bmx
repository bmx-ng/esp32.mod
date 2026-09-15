SuperStrict

Framework BRL.EventQueue
Import ESP32.Hardware.UART
Import ESP32.IO.BufferedUART

Const Controller:Int = UARTController1
Const TXPin:UInt = 17
Const RXPin:UInt = 18

If Not UARTConfigurePins(Controller, TXPin, RXPin) Then RuntimeError "UART pin configuration failed"
If Not UARTInit(Controller, 115200) Then RuntimeError "UART initialization failed"

Local stream:TBufferedUARTStream = OpenBufferedUARTStream(Controller, 256, 256)
If Not stream Then RuntimeError "Buffered UART open failed"

Local message:Byte[] = [Byte(Asc("E")), Byte(Asc("S")), Byte(Asc("P")), Byte(Asc("3")), Byte(Asc("2")), Byte(10)]
stream.Write(message, message.length)
stream.Flush()
stream.Close()
UARTDeinit(Controller)
