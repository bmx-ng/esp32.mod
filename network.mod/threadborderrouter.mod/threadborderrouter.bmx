' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP32 Thread border routing over a Wi-Fi or Ethernet backbone.
about: Import this module to build ESP-IDF border router support. Connect a
backbone, select it before starting Thread, then call ThreadBorderRouterStart
after Thread has attached. Requires an FTD-capable ESP32 board with an IEEE
802.15.4 radio and a supported backbone interface.
End Rem
Module ESP32.Network.ThreadBorderRouter
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import ESP32.Network.Thread

Private
Extern "C"
	Function _UseWiFi:Int() = "bmx_esp32_thread_border_router_use_wifi"
	Function _UseEthernet:Int() = "bmx_esp32_thread_border_router_use_ethernet"
	Function _Start:Int() = "bmx_esp32_thread_border_router_start"
	Function _Stop:Int() = "bmx_esp32_thread_border_router_stop"
	Function _Running:Int() = "bmx_esp32_thread_border_router_running"
	Function _State:Int() = "bmx_esp32_thread_border_router_state"
	Function _BackboneIPv6Address:Int(index:Int, value:Byte Ptr, capacity:Int) = "bmx_esp32_thread_border_router_backbone_ipv6_address"
End Extern

Public

Rem
bbdoc: Border routing has not been initialized.
End Rem
Const ThreadBorderRouterUninitialized:Int = 0
Rem
bbdoc: Border routing is disabled.
End Rem
Const ThreadBorderRouterDisabled:Int = 1
Rem
bbdoc: Border routing is configured but stopped.
End Rem
Const ThreadBorderRouterStopped:Int = 2
Rem
bbdoc: Border routing is active.
End Rem
Const ThreadBorderRouterActive:Int = 3

Rem
bbdoc: Select the connected Wi-Fi interface as Thread backbone.
returns: Zero on success or an ESP-IDF error.
about: Call before ThreadStartSaved or ThreadForm.
End Rem
Function ThreadBorderRouterUseWiFi:Int()
	Return _UseWiFi()
End Function

Rem
bbdoc: Select the connected Ethernet interface as Thread backbone.
returns: Zero on success or an ESP-IDF error.
about: Call before ThreadStartSaved or ThreadForm.
End Rem
Function ThreadBorderRouterUseEthernet:Int()
	Return _UseEthernet()
End Function

Rem
bbdoc: Start border routing after Thread attaches.
returns: Zero on success or an ESP-IDF error.
End Rem
Function ThreadBorderRouterStart:Int()
	Return _Start()
End Function

Rem
bbdoc: Stop border routing and release its backbone address.
returns: Zero on success or an ESP-IDF error.
End Rem
Function ThreadBorderRouterStop:Int()
	Return _Stop()
End Function

Rem
bbdoc: Check whether border routing is currently active.
returns: True when active; otherwise False.
End Rem
Function ThreadBorderRouterRunning:Int()
	Return _Running()
End Function

Rem
bbdoc: Get the border routing state.
returns: A ThreadBorderRouter* constant or a negative error.
End Rem
Function ThreadBorderRouterState:Int()
	Return _State()
End Function

Rem
bbdoc: Read a backbone IPv6 address by index.
param: Zero-based result index.
returns: An IPv6 address, or an empty string after the last address.
End Rem
Function ThreadBorderRouterBackboneIPv6Address:String(index:Int)
	If index < 0 Then
		Return ""
	End If

	Local bytes:Byte[48]
	Local length:Int = _BackboneIPv6Address(index, bytes, bytes.length)
	If length <= 0 Then
		Return ""
	End If

	Return String.FromUTF8Bytes(bytes, length)
End Function
?
