' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP-IDF non-volatile key/value storage.
about: Changes remain pending until #Commit succeeds. Namespaces and keys are
limited by ESP-IDF to 15 UTF-8 bytes. Methods return native esp_err_t values;
zero is success and #NVSResultName provides a readable description.
End Rem
Module ESP32.Storage.NVS
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const NVSReadOnly:Int = 0
Const NVSReadWrite:Int = 1

Const NVSTypeUInt8:Int = $01
Const NVSTypeUInt16:Int = $02
Const NVSTypeUInt32:Int = $04
Const NVSTypeUInt64:Int = $08
Const NVSTypeInt8:Int = $11
Const NVSTypeInt16:Int = $12
Const NVSTypeInt32:Int = $14
Const NVSTypeInt64:Int = $18
Const NVSTypeString:Int = $21
Const NVSTypeFloat:Int = $24
Const NVSTypeDouble:Int = $28
Const NVSTypeBlob:Int = $42
Const NVSTypeAny:Int = $ff

Const NVSErrorBase:Int = $1100
Const NVSErrorNotInitialized:Int = NVSErrorBase + $01
Const NVSErrorNotFound:Int = NVSErrorBase + $02
Const NVSErrorTypeMismatch:Int = NVSErrorBase + $03
Const NVSErrorReadOnly:Int = NVSErrorBase + $04
Const NVSErrorNotEnoughSpace:Int = NVSErrorBase + $05
Const NVSErrorInvalidName:Int = NVSErrorBase + $06
Const NVSErrorInvalidHandle:Int = NVSErrorBase + $07
Const NVSErrorKeyTooLong:Int = NVSErrorBase + $09
Const NVSErrorInvalidLength:Int = NVSErrorBase + $0c
Const NVSErrorNoFreePages:Int = NVSErrorBase + $0d
Const NVSErrorValueTooLong:Int = NVSErrorBase + $0e
Const NVSErrorPartitionNotFound:Int = NVSErrorBase + $0f
Const NVSErrorNewVersionFound:Int = NVSErrorBase + $10

Extern "C"
	Function NVSInitialize:Int() = "bmx_esp32_nvs_initialize"
	Function NVSLastOpenError:Int() = "bmx_esp32_nvs_last_open_error"
	Function NVSResultName:String(result:Int) = "bmx_esp32_nvs_result_name"
	Function _NVSOpen:Byte Ptr(name:String, mode:Int) = "bmx_esp32_nvs_open"
	Function _NVSClose(handle:Byte Ptr) = "bmx_esp32_nvs_close"
	Function _NVSLastError:Int(handle:Byte Ptr) = "bmx_esp32_nvs_last_error"
	Function _NVSCommit:Int(handle:Byte Ptr) = "bmx_esp32_nvs_commit"
	Function _NVSEraseKey:Int(handle:Byte Ptr, key:String) = "bmx_esp32_nvs_erase_key"
	Function _NVSEraseAll:Int(handle:Byte Ptr) = "bmx_esp32_nvs_erase_all"
	Function _NVSValueType:Int(handle:Byte Ptr, key:String) = "bmx_esp32_nvs_value_type"
	Function _NVSSetInt:Int(handle:Byte Ptr, key:String, value:Int) = "bmx_esp32_nvs_set_i32"
	Function _NVSSetUInt:Int(handle:Byte Ptr, key:String, value:UInt) = "bmx_esp32_nvs_set_u32"
	Function _NVSSetLong:Int(handle:Byte Ptr, key:String, value:Long) = "bmx_esp32_nvs_set_i64"
	Function _NVSSetULong:Int(handle:Byte Ptr, key:String, value:ULong) = "bmx_esp32_nvs_set_u64"
	Function _NVSSetFloat:Int(handle:Byte Ptr, key:String, value:Float) = "bmx_esp32_nvs_set_float"
	Function _NVSSetDouble:Int(handle:Byte Ptr, key:String, value:Double) = "bmx_esp32_nvs_set_double"
	Function _NVSSetString:Int(handle:Byte Ptr, key:String, value:String) = "bmx_esp32_nvs_set_string"
	Function _NVSSetBytes:Int(handle:Byte Ptr, key:String, value:Byte[]) = "bmx_esp32_nvs_set_blob"
	Function _NVSSetMemory:Int(handle:Byte Ptr, key:String, data:Byte Ptr, size:UInt) = "bmx_esp32_nvs_set_memory"
	Function _NVSGetInt:Int(handle:Byte Ptr, key:String, value:Int Var) = "bmx_esp32_nvs_get_i32"
	Function _NVSGetUInt:Int(handle:Byte Ptr, key:String, value:UInt Var) = "bmx_esp32_nvs_get_u32"
	Function _NVSGetLong:Int(handle:Byte Ptr, key:String, value:Long Var) = "bmx_esp32_nvs_get_i64"
	Function _NVSGetULong:Int(handle:Byte Ptr, key:String, value:ULong Var) = "bmx_esp32_nvs_get_u64"
	Function _NVSGetFloat:Int(handle:Byte Ptr, key:String, value:Float Var) = "bmx_esp32_nvs_get_float"
	Function _NVSGetDouble:Int(handle:Byte Ptr, key:String, value:Double Var) = "bmx_esp32_nvs_get_double"
	Function _NVSGetString:String(handle:Byte Ptr, key:String) = "bmx_esp32_nvs_get_string"
	Function _NVSGetBytes:Byte[](handle:Byte Ptr, key:String) = "bmx_esp32_nvs_get_blob"
	Function _NVSGetMemory:Int(handle:Byte Ptr, key:String, data:Byte Ptr, capacity:UInt, actualSize:UInt Var) = "bmx_esp32_nvs_get_memory"
	Function _NVSStatistics:Int(used:UInt Var, free:UInt Var, total:UInt Var, namespaces:UInt Var) = "bmx_esp32_nvs_statistics"
End Extern

Struct SNVSStatistics
	Field usedEntries:UInt
	Field freeEntries:UInt
	Field totalEntries:UInt
	Field namespaceCount:UInt
End Struct

Type TNVSNamespace
	Field handle:Byte Ptr

	Method IsOpen:Int()
		Return handle <> Null
	End Method

	Method Close()
		If handle Then
			_NVSClose handle
			handle = Null
		End If
	End Method

	Method Delete()
		Close()
	End Method

	Method LastError:Int()
		If Not handle Then Return NVSErrorInvalidHandle
		Return _NVSLastError(handle)
	End Method

	Method Commit:Int()
		Return _NVSCommit(handle)
	End Method

	Method EraseKey:Int(key:String)
		Return _NVSEraseKey(handle, key)
	End Method

	Method EraseAll:Int()
		Return _NVSEraseAll(handle)
	End Method

	Method ValueType:Int(key:String)
		Return _NVSValueType(handle, key)
	End Method

	Method SetInt:Int(key:String, value:Int)
		Return _NVSSetInt(handle, key, value)
	End Method

	Method SetUInt:Int(key:String, value:UInt)
		Return _NVSSetUInt(handle, key, value)
	End Method

	Method SetLong:Int(key:String, value:Long)
		Return _NVSSetLong(handle, key, value)
	End Method

	Method SetULong:Int(key:String, value:ULong)
		Return _NVSSetULong(handle, key, value)
	End Method

	Method SetFloat:Int(key:String, value:Float)
		Return _NVSSetFloat(handle, key, value)
	End Method

	Method SetDouble:Int(key:String, value:Double)
		Return _NVSSetDouble(handle, key, value)
	End Method

	Method SetString:Int(key:String, value:String)
		Return _NVSSetString(handle, key, value)
	End Method

	Method SetBytes:Int(key:String, value:Byte[])
		Return _NVSSetBytes(handle, key, value)
	End Method

	Method SetBytes:Int(key:String, data:Byte Ptr, size:UInt)
		Return _NVSSetMemory(handle, key, data, size)
	End Method

	Method GetInt:Int(key:String, value:Int Var)
		Return _NVSGetInt(handle, key, value)
	End Method

	Method GetUInt:Int(key:String, value:UInt Var)
		Return _NVSGetUInt(handle, key, value)
	End Method

	Method GetLong:Int(key:String, value:Long Var)
		Return _NVSGetLong(handle, key, value)
	End Method

	Method GetULong:Int(key:String, value:ULong Var)
		Return _NVSGetULong(handle, key, value)
	End Method

	Method GetFloat:Int(key:String, value:Float Var)
		Return _NVSGetFloat(handle, key, value)
	End Method

	Method GetDouble:Int(key:String, value:Double Var)
		Return _NVSGetDouble(handle, key, value)
	End Method

	Method GetString:String(key:String)
		Return _NVSGetString(handle, key)
	End Method

	Method TryGetString:Int(key:String, value:String Var)
		value = GetString(key)
		Return LastError() = 0
	End Method

	Method GetBytes:Byte[](key:String)
		Return _NVSGetBytes(handle, key)
	End Method

	Method GetBytes:Int(key:String, data:Byte Ptr, capacity:UInt, actualSize:UInt Var)
		Return _NVSGetMemory(handle, key, data, capacity, actualSize)
	End Method

	Method TryGetBytes:Int(key:String, value:Byte[] Var)
		value = GetBytes(key)
		Return LastError() = 0
	End Method
End Type

Function OpenNVS:TNVSNamespace(name:String, openReadOnly:Int = False)
	Local mode:Int = NVSReadWrite
	If openReadOnly Then mode = NVSReadOnly
	Local nativeHandle:Byte Ptr = _NVSOpen(name, mode)
	If Not nativeHandle Then Return Null
	Local result:TNVSNamespace = New TNVSNamespace
	result.handle = nativeHandle
	Return result
End Function

Function NVSStatistics:Int(statistics:SNVSStatistics Var)
	Return _NVSStatistics(statistics.usedEntries, statistics.freeEntries, statistics.totalEntries, statistics.namespaceCount)
End Function
?
