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
	Function ManagedContextValid:Int() = "bmx_esp32_managed_context_valid"
	Function ManagedTaskBound:Int() = "bmx_esp32_managed_task_bound"
	Function ManagedContextViolationCount:UInt() = "bmx_esp32_managed_context_violation_count"
	Function ManagedCallbackDispatchCount:UInt() = "bmx_esp32_managed_callback_dispatch_count"
	Function ManagedCallbackRejectionCount:UInt() = "bmx_esp32_managed_callback_rejection_count"
	Function ManagedArenaReserved:UInt() = "bmx_esp32_managed_arena_reserved"
	Function ManagedArenaValid:Int() = "bmx_esp32_managed_arena_valid"
	Function InternalHeapFree:UInt() = "bmx_esp32_internal_heap_free"
	Function InternalHeapLargestBlock:UInt() = "bmx_esp32_internal_heap_largest_block"
	Function ManagedArenaInPSRAM:Int() = "bmx_esp32_managed_arena_in_psram"
End Extern
?
