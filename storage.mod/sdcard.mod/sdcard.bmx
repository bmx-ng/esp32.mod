' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: FAT filesystem access to a board-profile-defined native SDMMC slot.
about: Mounting is non-destructive by default. Mounted files are available via
sd:: paths and may be selected as the default storage volume.
End Rem
Module ESP32.Storage.SDCard
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import ESP32.Storage.FileSystem

Extern "C"
	Function _MountSDCard:Int(formatIfMountFailed:Int) = "bmx_esp32_sdcard_mount"
	Function _UnmountSDCard:Int() = "bmx_esp32_sdcard_unmount"
	Function _FormatSDCard:Int() = "bmx_esp32_sdcard_format"
	Function SDCardIsMounted:Int() = "bmx_esp32_sdcard_is_mounted"
	Function SDCardLastError:Int() = "bmx_esp32_sdcard_last_error"
	Function SDCardCapacity:Long() = "bmx_esp32_sdcard_capacity"
	Function SDCardFree:Long() = "bmx_esp32_sdcard_free"
End Extern

Rem
bbdoc: Mounts the inserted SD card.
about: By default an unformatted or unrecognised card is left untouched. Set
@formatIfMountFailed only when the card may be repartitioned and formatted.
End Rem
Function MountSDCard:Int(formatIfMountFailed:Int = False)
	Local mounted:Int = _MountSDCard(formatIfMountFailed) = 0
	_ESP32StorageMountChanged(ESP32_STORAGE_SD, mounted)
	Return mounted
End Function

Rem
bbdoc: Flushes and unmounts the SD card.
End Rem
Function UnmountSDCard:Int()
	Local result:Int = _UnmountSDCard() = 0
	If result Then _ESP32StorageMountChanged(ESP32_STORAGE_SD, False)
	Return result
End Function

Rem
bbdoc: Formats the currently mounted SD card as FAT.
about: This destroys every file in the mounted FAT volume.
End Rem
Function FormatSDCard:Int()
	Return _FormatSDCard() = 0
End Function
?
