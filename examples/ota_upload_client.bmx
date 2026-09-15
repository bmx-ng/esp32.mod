' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Framework BRL.StandardIO
Import BRL.FileSystem
Import BRL.SocketStream

If AppArgs.length < 3 Or AppArgs.length > 4 Then
	Print "Usage: ota_upload_client <device-host> <firmware.bin> [port]"
	End
End If

Local host:String = AppArgs[1]
Local firmwarePath:String = AppArgs[2]
Local port:Int = 8765
If AppArgs.length = 4 Then port = AppArgs[3].ToInt()
If port < 1 Or port > 65535 Then RuntimeError "Invalid TCP port"

Local firmware:TStream = ReadStream(firmwarePath)
If Not firmware Then RuntimeError "Could not open " + firmwarePath
Local imageSize:Long = StreamSize(firmware)
If imageSize <= 0 Or imageSize > $ffffffff:Long Then RuntimeError "Invalid firmware image size"

Local hints:TAddrInfo = New TAddrInfo(AF_UNSPEC_, SOCK_STREAM_)
Local addresses:TAddrInfo[] = AddrInfo(host, String(port), hints)
If Not addresses Or addresses.length = 0 Then RuntimeError "Could not resolve " + host
Local socket:TSocket = TSocket.Create(addresses[0])
If Not socket Or Not ConnectSocket(socket, addresses[0]) Then RuntimeError "Could not connect to " + host + ":" + port
Local network:TSocketStream = CreateSocketStream(socket)

Local size:UInt = UInt(imageSize)
Local header:Byte[4]
header[0] = Byte(size)
header[1] = Byte(size Shr 8)
header[2] = Byte(size Shr 16)
header[3] = Byte(size Shr 24)
network.WriteBytes(header, header.length)

Local buffer:Byte[16384]
Local sent:Long
While sent < imageSize
	Local wanted:Int = buffer.length
	If imageSize - sent < wanted Then wanted = Int(imageSize - sent)
	Local count:Long = firmware.Read(buffer, wanted)
	If count <= 0 Then RuntimeError "Firmware file ended before its declared size"
	network.WriteBytes(buffer, count)
	sent :+ count
Wend
CloseStream firmware

Print "Sent " + sent + " bytes; waiting for validation"
Local reply:String = network.ReadLine()
CloseStream network
If Not reply Then RuntimeError "The device closed the connection without a result"
Print reply
