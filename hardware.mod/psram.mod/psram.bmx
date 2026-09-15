' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: External PSRAM information for ESP32 targets.
about: ESP-IDF and the selected board profile control PSRAM initialization.
Managed-heap placement is selected separately with bmk's
`-heap-region psram` option.
End Rem
Module ESP32.Hardware.PSRAM
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Extern "C"
	Function PSRAMAvailable:Int() = "bmx_esp32_psram_available"
	Function PSRAMCapacity:UInt() = "bmx_esp32_psram_capacity"
	Function PSRAMFree:UInt() = "bmx_esp32_psram_free"
	Function PSRAMLargestFreeBlock:UInt() = "bmx_esp32_psram_largest_block"
	Function PSRAMContains:Int(address:Byte Ptr) = "bmx_esp32_psram_contains"
	Function ManagedArenaInPSRAM:Int() = "bmx_esp32_managed_arena_in_psram"
End Extern
?
