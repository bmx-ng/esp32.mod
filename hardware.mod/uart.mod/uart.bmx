' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Hardware universal asynchronous receivers/transmitters for ESP32 targets.
about: Operation names mirror Pico.Hardware.UART where the hardware semantics
align. ESP-IDF supplies the native receive and transmit rings used underneath
these synchronous operations.
End Rem
Module ESP32.Hardware.UART
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const UARTController0:Int = 0
Const UARTController1:Int = 1
Const UARTController2:Int = 2
Const UARTUnavailableController:Int = -1

Const UARTParityNone:UInt = 0
Const UARTParityEven:UInt = 1
Const UARTParityOdd:UInt = 2

Const UARTErrorFraming:UInt = $1
Const UARTErrorParity:UInt = $2
Const UARTErrorBreak:UInt = $4
Const UARTErrorOverrun:UInt = $8
Const UARTErrorInvalidArgument:Int = -5

Extern "C"
	Function UARTControllerCount:Int() = "bmx_esp32_uart_controller_count"
	Rem
	bbdoc: Returns the UART used by the ESP-IDF console, or UARTUnavailableController.
	about: A USB Serial/JTAG console, such as the Baguette S3 default, does not
	consume a hardware UART and therefore returns UARTUnavailableController.
	End Rem
	Function UARTDefaultController:Int() = "bmx_esp32_uart_default_controller"
	Function UARTDefaultBaudrate:UInt() = "bmx_esp32_uart_default_baudrate"
	Function UARTSupportsFlexiblePinMappings:Int() = "bmx_esp32_uart_supports_flexible_pin_mappings"
	Function UARTConfigurePins:Int(controller:Int, txPin:UInt, rxPin:UInt) = "bmx_esp32_uart_configure_pins"
	Function UARTConfigureFlowControlPins:Int(controller:Int, ctsPin:UInt, rtsPin:UInt) = "bmx_esp32_uart_configure_flow_pins"
	Function UARTInit:UInt(controller:Int, baudrate:UInt) = "bmx_esp32_uart_init"
	Function UARTDeinit:Int(controller:Int) = "bmx_esp32_uart_deinit"
	Function UARTSetBaudrate:UInt(controller:Int, baudrate:UInt) = "bmx_esp32_uart_set_baudrate"
	Function UARTGetBaudrate:UInt(controller:Int) = "bmx_esp32_uart_get_baudrate"
	Function UARTSetFormat:Int(controller:Int, dataBits:UInt, stopBits:UInt, parity:UInt) = "bmx_esp32_uart_set_format"
	Function UARTSetFlowControl:Int(controller:Int, ctsEnabled:Int, rtsEnabled:Int) = "bmx_esp32_uart_set_flow_control"

	Rem
	bbdoc: Confirms the always-enabled ESP hardware FIFO.
	returns: True when enabled is true and the controller is initialized. ESP32
	UART FIFOs cannot be disabled, so a request to disable one returns False.
	End Rem
	Function UARTSetFIFOEnabled:Int(controller:Int, enabled:Int) = "bmx_esp32_uart_set_fifo_enabled"
	Function UARTIsEnabled:Int(controller:Int) = "bmx_esp32_uart_is_enabled"
	Function UARTIsWritable:Int(controller:Int) = "bmx_esp32_uart_is_writable"
	Function UARTIsReadable:Int(controller:Int) = "bmx_esp32_uart_is_readable"
	Function UARTIsReadableWithin:Int(controller:Int, timeoutMicroseconds:UInt) = "bmx_esp32_uart_is_readable_within_us"
	Function UARTWriteBlocking:Int(controller:Int, source:Byte Ptr, length:Int) = "bmx_esp32_uart_write_blocking"
	Function UARTReadBlocking:Int(controller:Int, destination:Byte Ptr, length:Int) = "bmx_esp32_uart_read_blocking"
	Function UARTReadTimeout:Int(controller:Int, destination:Byte Ptr, length:Int, timeoutMicroseconds:UInt) = "bmx_esp32_uart_read_timeout_us"
	Function UARTReadAvailable:Int(controller:Int, destination:Byte Ptr, capacity:Int) = "bmx_esp32_uart_read_available"
	Function UARTPutByte:Int(controller:Int, value:UInt) = "bmx_esp32_uart_put_byte"
	Function UARTTXWaitBlocking(controller:Int) = "bmx_esp32_uart_tx_wait_blocking"

	Rem
	bbdoc: Writes bytes followed by a finite break lasting breakBits bit periods.
	about: This is ESP-IDF's driver-coordinated break operation. breakBits must
	be between 1 and 255.
	End Rem
	Function UARTWriteWithBreak:Int(controller:Int, source:Byte Ptr, length:Int, breakBits:UInt) = "bmx_esp32_uart_write_with_break"

	Rem
	bbdoc: Assert or release a persistent break condition on the configured TX pin.
	about: Assertion first drains pending output and then holds TX low. Writes are
	rejected until the break is released and UART routing is restored.
	End Rem
	Function UARTSetBreak:Int(controller:Int, enabled:Int) = "bmx_esp32_uart_set_break"
	Function UARTIsBreakAsserted:Int(controller:Int) = "bmx_esp32_uart_is_break_asserted"
	Function UARTSetTranslateCRLF:Int(controller:Int, enabled:Int) = "bmx_esp32_uart_set_translate_crlf"
	Function UARTGetErrors:UInt(controller:Int) = "bmx_esp32_uart_get_errors"
	Function UARTClearErrors(controller:Int) = "bmx_esp32_uart_clear_errors"

	' ESP32-specific driver operations.
	Function UARTFlushInput:Int(controller:Int) = "bmx_esp32_uart_flush_input"
	Function UARTSetLoopback:Int(controller:Int, enabled:Int) = "bmx_esp32_uart_set_loopback"
End Extern
?
