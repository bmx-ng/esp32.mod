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
End Extern

Public

Function ThreadBorderRouterUseWiFi:Int()
	Return _UseWiFi()
End Function

Function ThreadBorderRouterUseEthernet:Int()
	Return _UseEthernet()
End Function

Function ThreadBorderRouterStart:Int()
	Return _Start()
End Function

Function ThreadBorderRouterStop:Int()
	Return _Stop()
End Function

Function ThreadBorderRouterRunning:Int()
	Return _Running()
End Function
?
