' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP32 wired Ethernet support.
about: The portable lifecycle, link-state and IPv4 API is provided by
Embedded.Network.Ethernet. This module adds ESP-IDF result names and PHY
details while selecting the MAC, PHY and signal mapping from the board profile.
End Rem
Module ESP32.Network.Ethernet
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Network.Ethernet

Private

Extern "C"
	Function _ESP32EthernetResultName:String(result:Int) = "bmx_esp32_ethernet_result_name"
	Function _ESP32EthernetPHYAddress:Int(address:Int Var) = "bmx_esp32_ethernet_phy_address"
	Function _ESP32EthernetMakeDefault:Int() = "bmx_esp32_ethernet_make_default"
	Function _ESP32EthernetIsDefault:Int() = "bmx_esp32_ethernet_is_default"
	Function _ESP32EthernetRoutePriority:Int() = "bmx_esp32_ethernet_route_priority"
	Function _ESP32EthernetSetRoutePriority:Int(priority:Int) = "bmx_esp32_ethernet_set_route_priority"
End Extern

Public

Function EthernetResultName:String(result:Int)
	Return _ESP32EthernetResultName(result)
End Function

Function EthernetPHYAddress:Int(address:Int Var)
	Return _ESP32EthernetPHYAddress(address)
End Function

Function EthernetMakeDefault:Int()
	Return _ESP32EthernetMakeDefault()
End Function

Function EthernetIsDefault:Int()
	Return _ESP32EthernetIsDefault()
End Function

Function EthernetRoutePriority:Int()
	Return _ESP32EthernetRoutePriority()
End Function

Function EthernetSetRoutePriority:Int(priority:Int)
	Return _ESP32EthernetSetRoutePriority(priority)
End Function
?
