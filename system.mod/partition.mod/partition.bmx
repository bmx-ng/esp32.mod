' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Read-only ESP32 flash partition discovery and metadata.
about: This module deliberately exposes no erase or write operations. Validated
application updates and boot selection are provided by ESP32.System.OTA.
End Rem
Module ESP32.System.Partition
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const PartitionTypeApp:Int = $00
Const PartitionTypeData:Int = $01
Const PartitionTypeBootloader:Int = $02
Const PartitionTypeTable:Int = $03
Const PartitionTypeAny:Int = $ff

Const PartitionSubtypeAppFactory:Int = $00
Const PartitionSubtypeAppOTAMin:Int = $10
Const PartitionSubtypeAppOTAMax:Int = $1f
Const PartitionSubtypeAppTest:Int = $20
Const PartitionSubtypeDataOTA:Int = $00
Const PartitionSubtypeDataPHY:Int = $01
Const PartitionSubtypeDataNVS:Int = $02
Const PartitionSubtypeDataCoreDump:Int = $03
Const PartitionSubtypeDataNVSKeys:Int = $04
Const PartitionSubtypeDataFAT:Int = $81
Const PartitionSubtypeDataSPIFFS:Int = $82
Const PartitionSubtypeDataLittleFS:Int = $83
Const PartitionSubtypeAny:Int = $ff

Extern "C"
	Function _PartitionCount:UInt(partitionType:Int, subtype:Int) = "bmx_esp32_partition_count"
	Function _PartitionLabel:String(index:UInt, partitionType:Int, subtype:Int) = "bmx_esp32_partition_label"
	Function _PartitionType:Int(index:UInt, partitionType:Int, subtype:Int) = "bmx_esp32_partition_type"
	Function _PartitionSubtype:Int(index:UInt, partitionType:Int, subtype:Int) = "bmx_esp32_partition_subtype"
	Function _PartitionAddress:UInt(index:UInt, partitionType:Int, subtype:Int) = "bmx_esp32_partition_address"
	Function _PartitionSize:UInt(index:UInt, partitionType:Int, subtype:Int) = "bmx_esp32_partition_size"
	Function _PartitionEraseSize:UInt(index:UInt, partitionType:Int, subtype:Int) = "bmx_esp32_partition_erase_size"
	Function _PartitionEncrypted:Int(index:UInt, partitionType:Int, subtype:Int) = "bmx_esp32_partition_encrypted"
	Function _PartitionReadOnly:Int(index:UInt, partitionType:Int, subtype:Int) = "bmx_esp32_partition_read_only"
	Function _PartitionRunning:Int(index:UInt, partitionType:Int, subtype:Int) = "bmx_esp32_partition_running"
End Extern

Type TESP32Partition
	Field label:String
	Field partitionType:Int
	Field subtype:Int
	Field address:UInt
	Field size:UInt
	Field eraseSize:UInt
	Field encrypted:Int
	Field isReadOnly:Int
	Field running:Int

	Method IsApplication:Int()
		Return partitionType = PartitionTypeApp
	End Method

	Method IsData:Int()
		Return partitionType = PartitionTypeData
	End Method

	Method IsOTAApplication:Int()
		Return partitionType = PartitionTypeApp And subtype >= PartitionSubtypeAppOTAMin And subtype <= PartitionSubtypeAppOTAMax
	End Method
End Type

Function ESP32Partitions:TESP32Partition[](partitionType:Int = PartitionTypeAny, subtype:Int = PartitionSubtypeAny)
	Local count:UInt = _PartitionCount(partitionType, subtype)
	Local result:TESP32Partition[] = New TESP32Partition[Int(count)]
	For Local index:UInt = 0 Until count
		Local partition:TESP32Partition = New TESP32Partition
		partition.label = _PartitionLabel(index, partitionType, subtype)
		partition.partitionType = _PartitionType(index, partitionType, subtype)
		partition.subtype = _PartitionSubtype(index, partitionType, subtype)
		partition.address = _PartitionAddress(index, partitionType, subtype)
		partition.size = _PartitionSize(index, partitionType, subtype)
		partition.eraseSize = _PartitionEraseSize(index, partitionType, subtype)
		partition.encrypted = _PartitionEncrypted(index, partitionType, subtype)
		partition.isReadOnly = _PartitionReadOnly(index, partitionType, subtype)
		partition.running = _PartitionRunning(index, partitionType, subtype)
		result[index] = partition
	Next
	Return result
End Function

Function FindESP32Partition:TESP32Partition(label:String, partitionType:Int = PartitionTypeAny, subtype:Int = PartitionSubtypeAny)
	Local partitions:TESP32Partition[] = ESP32Partitions(partitionType, subtype)
	For Local index:Int = 0 Until partitions.length
		If partitions[index].label = label Then Return partitions[index]
	Next
	Return Null
End Function

Function PartitionTypeName:String(partitionType:Int)
	Select partitionType
		Case PartitionTypeApp Return "application"
		Case PartitionTypeData Return "data"
		Case PartitionTypeBootloader Return "bootloader"
		Case PartitionTypeTable Return "partition table"
	End Select
	Return "custom"
End Function

Function PartitionSubtypeName:String(partitionType:Int, subtype:Int)
	If partitionType = PartitionTypeApp
		If subtype = PartitionSubtypeAppFactory Then Return "factory"
		If subtype >= PartitionSubtypeAppOTAMin And subtype <= PartitionSubtypeAppOTAMax Then Return "ota_" + (subtype - PartitionSubtypeAppOTAMin)
		If subtype = PartitionSubtypeAppTest Then Return "test"
	Else If partitionType = PartitionTypeData
		Select subtype
			Case PartitionSubtypeDataOTA Return "ota"
			Case PartitionSubtypeDataPHY Return "phy"
			Case PartitionSubtypeDataNVS Return "nvs"
			Case PartitionSubtypeDataCoreDump Return "coredump"
			Case PartitionSubtypeDataNVSKeys Return "nvs_keys"
			Case PartitionSubtypeDataFAT Return "fat"
			Case PartitionSubtypeDataSPIFFS Return "spiffs"
			Case PartitionSubtypeDataLittleFS Return "littlefs"
		End Select
	End If
	Return "custom_" + subtype
End Function
?
