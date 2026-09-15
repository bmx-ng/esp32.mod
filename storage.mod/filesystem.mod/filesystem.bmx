' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Shared BRL.FileSystem routing for ESP32 persistent storage volumes.
about: Import the concrete LittleFS or SDCard module to mount a volume. Plain
paths and file:: use the selected default; littlefs:: and sd:: always select
their named mounted volume.
End Rem
Module ESP32.Storage.FileSystem
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import BRL.FileSystem
Import "native/esp32_filesystem.c"

Const ESP32_STORAGE_LITTLEFS:Int = 0
Const ESP32_STORAGE_SD:Int = 1

Extern "C"
	Function _ESP32FSOpen:Byte Ptr(volume:Int, path:String, readable:Int, writeMode:Int) = "bmx_esp32_filesystem_open"
	Function _ESP32FSClose:Int(handle:Byte Ptr) = "bmx_esp32_filesystem_close"
	Function _ESP32FSRead:Long(handle:Byte Ptr, buffer:Byte Ptr, count:Long) = "bmx_esp32_filesystem_read"
	Function _ESP32FSWrite:Long(handle:Byte Ptr, buffer:Byte Ptr, count:Long) = "bmx_esp32_filesystem_write"
	Function _ESP32FSPosition:Long(handle:Byte Ptr) = "bmx_esp32_filesystem_position"
	Function _ESP32FSSize:Long(handle:Byte Ptr) = "bmx_esp32_filesystem_size"
	Function _ESP32FSSeek:Long(handle:Byte Ptr, position:Long, whence:Int) = "bmx_esp32_filesystem_seek"
	Function _ESP32FSResize:Int(handle:Byte Ptr, size:Long) = "bmx_esp32_filesystem_resize"
	Function _ESP32FSFlush:Int(handle:Byte Ptr) = "bmx_esp32_filesystem_flush"
	Function _ESP32FSStat:Int(volume:Int, path:String, fileType:Int Var, size:Long Var, modified:Long Var, created:Long Var, accessed:Long Var, readOnly:Int Var) = "bmx_esp32_filesystem_stat"
	Function _ESP32FSSetTime:Int(volume:Int, path:String, time:Long, timeType:Int) = "bmx_esp32_filesystem_set_time"
	Function _ESP32FSMkdir:Int(volume:Int, path:String) = "bmx_esp32_filesystem_mkdir"
	Function _ESP32FSRemove:Int(volume:Int, path:String) = "bmx_esp32_filesystem_remove"
	Function _ESP32FSRename:Int(volume:Int, oldPath:String, newPath:String) = "bmx_esp32_filesystem_rename"
	Function _ESP32FSDirectoryOpen:Byte Ptr(volume:Int, path:String) = "bmx_esp32_filesystem_directory_open"
	Function _ESP32FSDirectoryNext:String(handle:Byte Ptr) = "bmx_esp32_filesystem_directory_next"
	Function _ESP32FSDirectoryClose(handle:Byte Ptr) = "bmx_esp32_filesystem_directory_close"
End Extern

Type TESP32FileStream Extends TStream
	Field handle:Byte Ptr
	Field readable:Int
	Field writeMode:Int

	Method Pos:Long() Override
		If Not handle Then Return -1
		Return _ESP32FSPosition(handle)
	End Method
	Method Size:Long() Override
		If Not handle Then Return 0
		Return _ESP32FSSize(handle)
	End Method
	Method Seek:Long(position:Long, whence:Int = SEEK_SET_) Override
		If Not handle Then Return -1
		Return _ESP32FSSeek(handle, position, whence)
	End Method
	Method Flush() Override
		If handle And _ESP32FSFlush(handle) < 0 Then Throw New TStreamWriteException
	End Method
	Method Close() Override
		If Not handle Then Return
		Local oldHandle:Byte Ptr = handle
		handle = Null
		If _ESP32FSClose(oldHandle) < 0 Then Throw New TStreamWriteException
	End Method
	Method Read:Long(buffer:Byte Ptr, count:Long) Override
		If Not handle Or Not readable Then Throw New TStreamReadException
		Local result:Long = _ESP32FSRead(handle, buffer, count)
		If result < 0 Then Throw New TStreamReadException
		Return result
	End Method
	Method Write:Long(buffer:Byte Ptr, count:Long) Override
		If Not handle Or Not writeMode Then Throw New TStreamWriteException
		Local result:Long = _ESP32FSWrite(handle, buffer, count)
		If result < 0 Then Throw New TStreamWriteException
		Return result
	End Method
	Method SetSize:Int(size:Long) Override
		Return handle And writeMode And _ESP32FSResize(handle, size) = 0
	End Method

	Function Open:TESP32FileStream(volume:Int, path:String, readable:Int, writeMode:Int)
		Local handle:Byte Ptr = _ESP32FSOpen(volume, path, readable, writeMode)
		If Not handle Then Return Null
		Local stream:TESP32FileStream = New TESP32FileStream
		stream.handle = handle
		stream.readable = readable
		stream.writeMode = writeMode
		Return stream
	End Function
End Type

Type TESP32FileSystemBackend Extends TFileSystemBackend
	Field volume:Int
	Field protocol:String
	Field currentDirectory:String = "/"

	Method New(volume:Int, protocol:String)
		Self.volume = volume
		Self.protocol = protocol
	End Method
	Method HandlesProtocol:Int(value:String) Override
		Return value.ToLower() = protocol
	End Method
	Method OpenPath:TStream(path:String, readable:Int, writeMode:Int) Override
		Return TESP32FileStream.Open(volume, Resolve(path), readable, writeMode)
	End Method
	Method CurrentDirectory:String() Override
		Return currentDirectory
	End Method
	Method Resolve:String(path:String)
		Return ResolvePath(path)
	End Method
	Method ChangeDirectory:Int(path:String) Override
		Local resolved:String = Resolve(path)
		Local info:SFileStat
		If Not Stat(resolved, info) Or info.fileType <> FILETYPE_DIR Then Return False
		currentDirectory = resolved
		Return True
	End Method
	Method Stat:Int(path:String, info:SFileStat Var) Override
		Return _ESP32FSStat(volume, Resolve(path), info.fileType, info.size, info.modifiedTime, info.creationTime, info.accessTime, info.isReadOnly) = 0
	End Method
	Method SetTime(path:String, time:Long, timeType:Int) Override
		_ESP32FSSetTime(volume, Resolve(path), time, timeType)
	End Method
	Method FileMode:Int(path:String) Override
		Local info:SFileStat
		If Not Stat(path, info) Then Return -1
		If info.fileType = FILETYPE_DIR Then Return $1FF
		Return $1B6
	End Method
	Method CreateFile:Int(path:String) Override
		Local stream:TESP32FileStream = TESP32FileStream.Open(volume, Resolve(path), False, WRITE_MODE_OVERWRITE)
		If Not stream Then Return False
		stream.Close()
		Return True
	End Method
	Method CreateDirectory:Int(path:String) Override
		Return _ESP32FSMkdir(volume, Resolve(path)) = 0
	End Method
	Method DeleteFile:Int(path:String) Override
		Local info:SFileStat
		If Not Stat(path, info) Or info.fileType <> FILETYPE_FILE Then Return False
		Return _ESP32FSRemove(volume, Resolve(path)) = 0
	End Method
	Method DeleteDirectory:Int(path:String) Override
		Local info:SFileStat
		If Not Stat(path, info) Or info.fileType <> FILETYPE_DIR Then Return False
		Return _ESP32FSRemove(volume, Resolve(path)) = 0
	End Method
	Method Rename:Int(oldPath:String, newPath:String) Override
		Return _ESP32FSRename(volume, Resolve(oldPath), Resolve(newPath)) = 0
	End Method
	Method OpenDirectory:Byte Ptr(path:String) Override
		Return _ESP32FSDirectoryOpen(volume, Resolve(path))
	End Method
	Method NextDirectoryEntry:String(handle:Byte Ptr) Override
		Return _ESP32FSDirectoryNext(handle)
	End Method
	Method CloseDirectory(handle:Byte Ptr) Override
		_ESP32FSDirectoryClose(handle)
	End Method
End Type

Global _esp32LittleFSBackend:TESP32FileSystemBackend = New TESP32FileSystemBackend(ESP32_STORAGE_LITTLEFS, "littlefs")
Global _esp32SDBackend:TESP32FileSystemBackend = New TESP32FileSystemBackend(ESP32_STORAGE_SD, "sd")
Global _esp32LittleFSMounted:Int
Global _esp32SDMounted:Int
Global _esp32StorageDefaultExplicit:Int

Function _ESP32StorageMountChanged(volume:Int, mounted:Int)
	If volume = ESP32_STORAGE_LITTLEFS Then _esp32LittleFSMounted = mounted Else _esp32SDMounted = mounted
	If Not mounted
		If volume = ESP32_STORAGE_LITTLEFS And DefaultFileSystemBackend() = _esp32LittleFSBackend Then _esp32StorageDefaultExplicit = False
		If volume = ESP32_STORAGE_SD And DefaultFileSystemBackend() = _esp32SDBackend Then _esp32StorageDefaultExplicit = False
	End If
	If _esp32StorageDefaultExplicit Then Return
	If _esp32LittleFSMounted
		SetDefaultFileSystemBackend(_esp32LittleFSBackend)
	Else If _esp32SDMounted
		SetDefaultFileSystemBackend(_esp32SDBackend)
	Else
		SetDefaultFileSystemBackend(Null)
	End If
End Function

Rem
bbdoc: Selects `littlefs` or `sd` as the backend for plain paths.
returns: True when the named volume is mounted and was selected.
End Rem
Function SetDefaultStorageVolume:Int(protocol:String)
	protocol = protocol.Trim().ToLower()
	If protocol = "littlefs" And _esp32LittleFSMounted
		SetDefaultFileSystemBackend(_esp32LittleFSBackend)
	Else If protocol = "sd" And _esp32SDMounted
		SetDefaultFileSystemBackend(_esp32SDBackend)
	Else
		Return False
	End If
	_esp32StorageDefaultExplicit = True
	Return True
End Function

Function DefaultStorageVolume:String()
	If DefaultFileSystemBackend() = _esp32LittleFSBackend Then Return "littlefs"
	If DefaultFileSystemBackend() = _esp32SDBackend Then Return "sd"
	Return ""
End Function
?
