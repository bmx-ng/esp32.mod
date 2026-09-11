' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Module ESP32.Runtime

ModuleInfo "Version: 1.00"
ModuleInfo "License: zlib/libpng"
ModuleInfo "Platform: ESP32"

?esp32
Import BRL.Blitz

Extern "C"
	Function RuntimeCore:Int() = "bmx_esp32_runtime_core"
	Function ManagedArenaReserved:UInt() = "bmx_esp32_managed_arena_reserved"
	Function ManagedArenaValid:Int() = "bmx_esp32_managed_arena_valid"
	Function InternalHeapFree:UInt() = "bmx_esp32_internal_heap_free"
	Function InternalHeapLargestBlock:UInt() = "bmx_esp32_internal_heap_largest_block"
End Extern
?
