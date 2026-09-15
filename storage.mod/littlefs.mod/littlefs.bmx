' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Power-safe persistent LittleFS storage in ESP32 internal flash.
about: Importing this module mounts the board profile's data partition named
`storage` and makes it the default filesystem. A completely erased partition
is formatted automatically; nonblank unrecognised data is preserved.
End Rem
Module ESP32.Storage.LittleFS
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import ESP32.Storage.FileSystem

Extern "C"
	Function _MountLittleFS:Int(formatBlank:Int) = "bmx_esp32_littlefs_mount"
	Function _UnmountLittleFS:Int() = "bmx_esp32_littlefs_unmount"
	Function _FormatLittleFS:Int() = "bmx_esp32_littlefs_format"
	Function LittleFSIsMounted:Int() = "bmx_esp32_littlefs_is_mounted"
	Function LittleFSLastError:Int() = "bmx_esp32_littlefs_last_error"
	Function LittleFSCapacity:Long() = "bmx_esp32_littlefs_capacity"
	Function LittleFSUsed:Long() = "bmx_esp32_littlefs_used"
End Extern

Rem
bbdoc: Mounts the internal `storage` partition.
about: When @formatBlank is true, only a completely erased partition may be
formatted automatically.
End Rem
Function MountLittleFS:Int(formatBlank:Int = True)
	Local mounted:Int = _MountLittleFS(formatBlank) = 0
	_ESP32StorageMountChanged(ESP32_STORAGE_LITTLEFS, mounted)
	Return mounted
End Function

Rem
bbdoc: Flushes and unmounts internal LittleFS storage.
End Rem
Function UnmountLittleFS:Int()
	Local result:Int = _UnmountLittleFS() = 0
	If result Then _ESP32StorageMountChanged(ESP32_STORAGE_LITTLEFS, False)
	Return result
End Function

Rem
bbdoc: Erases, formats, and remounts internal LittleFS storage.
about: This destroys every file in the `storage` partition.
End Rem
Function FormatLittleFS:Int()
	Local result:Int = _FormatLittleFS() = 0
	_ESP32StorageMountChanged(ESP32_STORAGE_LITTLEFS, result)
	Return result
End Function

Global _esp32LittleFSAutoMounted:Int = MountLittleFS(True)
?
