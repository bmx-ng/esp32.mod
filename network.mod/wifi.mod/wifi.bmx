' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP32 wireless LAN support.
about: The portable scanning, connection, link-state and IPv4 API is provided
by Embedded.Network.WiFi. This module adds ESP32 radio controls, detailed
station information and a SoftAP which can coexist with station mode.
End Rem
Module ESP32.Network.WiFi
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Network.WiFi

Const WiFiPowerSaveNone:Int = 0
Const WiFiPowerSaveMinimumModem:Int = 1
Const WiFiPowerSaveMaximumModem:Int = 2

Const WiFiSecondChannelNone:Int = 0
Const WiFiSecondChannelAbove:Int = 1
Const WiFiSecondChannelBelow:Int = 2

Const WiFiBandwidth20MHz:Int = 1
Const WiFiBandwidth40MHz:Int = 2
Const WiFiBandwidth80MHz:Int = 3
Const WiFiBandwidth160MHz:Int = 4
Const WiFiBandwidth80Plus80MHz:Int = 5

Const WiFiProtocol80211B:UInt = $01
Const WiFiProtocol80211G:UInt = $02
Const WiFiProtocol80211N:UInt = $04
Const WiFiProtocolLongRange:UInt = $08
Const WiFiProtocol80211A:UInt = $10
Const WiFiProtocol80211AC:UInt = $20
Const WiFiProtocol80211AX:UInt = $40

Struct SESP32WiFiStationInfo
	Field ssid:String
	Field bssid:Byte[]
	Field channel:Int
	Field secondaryChannel:Int
	Field rssi:Int
	Field security:Int
	Field bandwidth:Int
	Field protocols:UInt
End Struct

Private

Extern "C"
	Function _ESP32WiFiResultName:String(result:Int) = "bmx_esp32_wifi_result_name"
	Function _ESP32WiFiSetPowerSave:Int(mode:Int) = "bmx_esp32_wifi_set_power_save"
	Function _ESP32WiFiGetPowerSave:Int(mode:Int Var) = "bmx_esp32_wifi_get_power_save"
	Function _ESP32WiFiSetMaximumTransmitPower:Int(power:Int) = "bmx_esp32_wifi_set_maximum_transmit_power"
	Function _ESP32WiFiGetMaximumTransmitPower:Int(power:Int Var) = "bmx_esp32_wifi_get_maximum_transmit_power"
	Function _ESP32WiFiSetStationProtocols:Int(protocols:UInt) = "bmx_esp32_wifi_set_station_protocols"
	Function _ESP32WiFiGetStationProtocols:Int(protocols:UInt Var) = "bmx_esp32_wifi_get_station_protocols"
	Function _ESP32WiFiSetStationBandwidth:Int(bandwidth:Int) = "bmx_esp32_wifi_set_station_bandwidth"
	Function _ESP32WiFiGetStationBandwidth:Int(bandwidth:Int Var) = "bmx_esp32_wifi_get_station_bandwidth"
	Function _ESP32WiFiGetStationInfo:Int(ssid:Byte Ptr, ssidLength:Int Var, bssid:Byte Ptr, ..
		channel:Int Var, secondaryChannel:Int Var, rssi:Int Var, security:Int Var, ..
		bandwidth:Int Var, protocols:UInt Var) = "bmx_esp32_wifi_get_station_info"
	Function _ESP32WiFiStartAccessPoint:Int(ssid:Byte Ptr, ssidLength:UInt, password:Byte Ptr, ..
		passwordLength:UInt, authentication:UInt, channel:UInt, maximumConnections:UInt, ..
		hidden:Int) = "bmx_esp32_wifi_start_access_point"
	Function _ESP32WiFiStopAccessPoint:Int() = "bmx_esp32_wifi_stop_access_point"
	Function _ESP32WiFiAccessPointActive:Int() = "bmx_esp32_wifi_access_point_active"
	Function _ESP32WiFiAccessPointClientCount:Int(count:UInt Var) = "bmx_esp32_wifi_access_point_client_count"
	Function _ESP32WiFiAccessPointIPv4Address:UInt() = "bmx_esp32_wifi_access_point_ipv4_address"
	Function _ESP32WiFiAccessPointIPv4Netmask:UInt() = "bmx_esp32_wifi_access_point_ipv4_netmask"
	Function _ESP32WiFiAccessPointIPv4Gateway:UInt() = "bmx_esp32_wifi_access_point_ipv4_gateway"
End Extern

Function _ESP32IPv4String:String(value:UInt)
	If value = 0 Then Return ""
	Return String(value & $ff) + "." + String((value Shr 8) & $ff) + "." + ..
		String((value Shr 16) & $ff) + "." + String((value Shr 24) & $ff)
End Function

Public

Function WiFiResultName:String(result:Int)
	Return _ESP32WiFiResultName(result)
End Function

Function WiFiSetPowerSave:Int(mode:Int)
	Return _ESP32WiFiSetPowerSave(mode)
End Function

Function WiFiGetPowerSave:Int(mode:Int Var)
	Return _ESP32WiFiGetPowerSave(mode)
End Function

Function WiFiSetMaximumTransmitPower:Int(powerQuarterDBm:Int)
	Return _ESP32WiFiSetMaximumTransmitPower(powerQuarterDBm)
End Function

Function WiFiGetMaximumTransmitPower:Int(powerQuarterDBm:Int Var)
	Return _ESP32WiFiGetMaximumTransmitPower(powerQuarterDBm)
End Function

Function WiFiSetStationProtocols:Int(protocols:UInt)
	Return _ESP32WiFiSetStationProtocols(protocols)
End Function

Function WiFiGetStationProtocols:Int(protocols:UInt Var)
	Return _ESP32WiFiGetStationProtocols(protocols)
End Function

Function WiFiSetStationBandwidth:Int(bandwidth:Int)
	Return _ESP32WiFiSetStationBandwidth(bandwidth)
End Function

Function WiFiGetStationBandwidth:Int(bandwidth:Int Var)
	Return _ESP32WiFiGetStationBandwidth(bandwidth)
End Function

Function WiFiGetStationInfo:Int(info:SESP32WiFiStationInfo Var)
	info.ssid = ""
	info.bssid = Null
	info.channel = 0
	info.secondaryChannel = 0
	info.rssi = 0
	info.security = 0
	info.bandwidth = 0
	info.protocols = 0
	Local ssidBytes:Byte[32]
	Local bssidBytes:Byte[6]
	Local ssidLength:Int
	Local result:Int = _ESP32WiFiGetStationInfo(ssidBytes, ssidLength, bssidBytes, ..
		info.channel, info.secondaryChannel, info.rssi, info.security, info.bandwidth, info.protocols)
	If result = 0 Then
		info.ssid = String.FromUTF8Bytes(ssidBytes, ssidLength)
		info.bssid = New Byte[6]
		MemCopy(info.bssid, bssidBytes, 6)
	End If
	Return result
End Function

Function WiFiStartAccessPoint:Int(ssid:String, password:String = "", ..
		authentication:UInt = WiFiAuthenticationWPA2AESPSK, channel:UInt = 1, ..
		maximumConnections:UInt = 4, hidden:Int = False)
	If Not ssid Then Return -5
	Local ssidLength:Size_T, passwordLength:Size_T
	Local ssidBytes:Byte Ptr = ssid.ToUTF8String(ssidLength)
	Local passwordBytes:Byte Ptr
	If password Then passwordBytes = password.ToUTF8String(passwordLength)
	Local result:Int = _ESP32WiFiStartAccessPoint(ssidBytes, UInt(ssidLength), passwordBytes, ..
		UInt(passwordLength), authentication, channel, maximumConnections, hidden)
	MemFree(ssidBytes)
	If passwordBytes Then MemFree(passwordBytes)
	Return result
End Function

Function WiFiStopAccessPoint:Int()
	Return _ESP32WiFiStopAccessPoint()
End Function

Function WiFiAccessPointActive:Int()
	Return _ESP32WiFiAccessPointActive()
End Function

Function WiFiAccessPointClientCount:Int(count:UInt Var)
	Return _ESP32WiFiAccessPointClientCount(count)
End Function

Function WiFiAccessPointIPv4Address:String()
	Return _ESP32IPv4String(_ESP32WiFiAccessPointIPv4Address())
End Function

Function WiFiAccessPointIPv4Netmask:String()
	Return _ESP32IPv4String(_ESP32WiFiAccessPointIPv4Netmask())
End Function

Function WiFiAccessPointIPv4Gateway:String()
	Return _ESP32IPv4String(_ESP32WiFiAccessPointIPv4Gateway())
End Function
?
