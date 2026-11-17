' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP-IDF non-volatile key/value storage.
about: Changes remain pending until #Commit succeeds. Namespaces and keys are
limited by ESP-IDF to 15 ASCII characters. NVS is intended for relatively
small persistent values; use a filesystem for large documents or binary data.
Methods return native esp_err_t values; zero is success and #NVSResultName
provides a readable description.
End Rem
Module ESP32.Storage.NVS
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Rem
bbdoc: Opens an existing namespace for reading only.
End Rem
Const NVSReadOnly:Int = 0

Rem
bbdoc: Opens or creates a namespace for reading and writing.
End Rem
Const NVSReadWrite:Int = 1

Rem
bbdoc: Identifies an unsigned 8-bit NVS value.
End Rem
Const NVSTypeUInt8:Int = $01
Rem
bbdoc: Identifies an unsigned 16-bit NVS value.
End Rem
Const NVSTypeUInt16:Int = $02
Rem
bbdoc: Identifies an unsigned 32-bit NVS value.
End Rem
Const NVSTypeUInt32:Int = $04
Rem
bbdoc: Identifies an unsigned 64-bit NVS value.
End Rem
Const NVSTypeUInt64:Int = $08
Rem
bbdoc: Identifies a signed 8-bit NVS value.
End Rem
Const NVSTypeInt8:Int = $11
Rem
bbdoc: Identifies a signed 16-bit NVS value.
End Rem
Const NVSTypeInt16:Int = $12
Rem
bbdoc: Identifies a signed 32-bit NVS value.
End Rem
Const NVSTypeInt32:Int = $14
Rem
bbdoc: Identifies a signed 64-bit NVS value.
End Rem
Const NVSTypeInt64:Int = $18
Rem
bbdoc: Identifies a zero-terminated UTF-8 string stored by NVS.
End Rem
Const NVSTypeString:Int = $21
Rem
bbdoc: Identifies a 32-bit floating-point NVS value.
End Rem
Const NVSTypeFloat:Int = $24
Rem
bbdoc: Identifies a 64-bit floating-point NVS value.
End Rem
Const NVSTypeDouble:Int = $28
Rem
bbdoc: Identifies an arbitrary binary NVS value.
End Rem
Const NVSTypeBlob:Int = $42
Rem
bbdoc: Matches any NVS value type when an API supports type filtering.
End Rem
Const NVSTypeAny:Int = $ff

Rem
bbdoc: Base value used by ESP-IDF NVS error codes.
End Rem
Const NVSErrorBase:Int = $1100
Rem
bbdoc: The requested NVS partition has not been initialized.
End Rem
Const NVSErrorNotInitialized:Int = NVSErrorBase + $01
Rem
bbdoc: The requested namespace, key, or iterator entry was not found.
End Rem
Const NVSErrorNotFound:Int = NVSErrorBase + $02
Rem
bbdoc: The stored value has a different type from the requested type.
End Rem
Const NVSErrorTypeMismatch:Int = NVSErrorBase + $03
Rem
bbdoc: A write operation was attempted through a read-only handle.
End Rem
Const NVSErrorReadOnly:Int = NVSErrorBase + $04
Rem
bbdoc: The NVS partition does not have enough space for the operation.
End Rem
Const NVSErrorNotEnoughSpace:Int = NVSErrorBase + $05
Rem
bbdoc: A namespace or key name is empty or otherwise invalid.
End Rem
Const NVSErrorInvalidName:Int = NVSErrorBase + $06
Rem
bbdoc: The namespace handle is null, closed, or invalid.
End Rem
Const NVSErrorInvalidHandle:Int = NVSErrorBase + $07
Rem
bbdoc: A key name exceeds ESP-IDF's 15-character limit.
End Rem
Const NVSErrorKeyTooLong:Int = NVSErrorBase + $09
Rem
bbdoc: A supplied buffer is too small for the stored string or blob.
End Rem
Const NVSErrorInvalidLength:Int = NVSErrorBase + $0c
Rem
bbdoc: The NVS partition contains no free pages and may require recovery.
End Rem
Const NVSErrorNoFreePages:Int = NVSErrorBase + $0d
Rem
bbdoc: A string or blob is too large for the NVS partition.
End Rem
Const NVSErrorValueTooLong:Int = NVSErrorBase + $0e
Rem
bbdoc: The requested NVS partition is absent from the partition table.
End Rem
Const NVSErrorPartitionNotFound:Int = NVSErrorBase + $0f
Rem
bbdoc: The NVS partition uses a format newer than this ESP-IDF runtime supports.
End Rem
Const NVSErrorNewVersionFound:Int = NVSErrorBase + $10

Extern "C"
	Rem
	bbdoc: Initializes the default NVS partition.
	returns: ESP_OK on success or an ESP-IDF error code.
	about: #OpenNVS initializes the default partition automatically, so most
	applications do not need to call this function directly.
	End Rem
	Function NVSInitialize:Int() = "bmx_esp32_nvs_initialize"
	Rem
	bbdoc: Returns the result from the most recent failed or successful #OpenNVS call.
	returns: An ESP-IDF result code.
	End Rem
	Function NVSLastOpenError:Int() = "bmx_esp32_nvs_last_open_error"
	Rem
	bbdoc: Returns the symbolic ESP-IDF name for an NVS result code.
	param: Result code returned by an NVS operation.
	returns: A name such as `ESP_OK` or `ESP_ERR_NVS_NOT_FOUND`.
	End Rem
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

Rem
bbdoc: Storage usage statistics for the default NVS partition.
about: NVS stores metadata as entries as well as application values. Strings
and blobs may occupy multiple entries, so an entry count is not a key count.
End Rem
Struct SNVSStatistics
	Rem
	bbdoc: Number of entries currently occupied by namespaces, keys, and values.
	End Rem
	Field usedEntries:UInt
	Rem
	bbdoc: Number of unused entries, including entries reserved internally by NVS.
	End Rem
	Field freeEntries:UInt
	Rem
	bbdoc: Total number of entries in the default NVS partition.
	End Rem
	Field totalEntries:UInt
	Rem
	bbdoc: Number of namespaces currently present in the partition.
	End Rem
	Field namespaceCount:UInt
End Struct

Rem
bbdoc: An open namespace in the default NVS partition.
about: Close a namespace when it is no longer needed. Uncommitted writes are
not made durable merely by closing or deleting the handle.
End Rem
Type TNVSNamespace
	Rem
	bbdoc: Native namespace handle used by this wrapper.
	about: Application code should treat this field as opaque.
	End Rem
	Field handle:Byte Ptr

	Rem
	bbdoc: Reports whether the namespace still owns an open native handle.
	returns: True while the namespace is open.
	End Rem
	Method IsOpen:Int()
		Return handle <> Null
	End Method

	Rem
	bbdoc: Closes the namespace handle.
	about: Call #Commit before closing when pending writes must persist.
	End Rem
	Method Close()
		If handle Then
			_NVSClose handle
			handle = Null
		End If
	End Method

	Rem
	bbdoc: Closes the namespace when the object is collected.
	End Rem
	Method Delete()
		Close()
	End Method

	Rem
	bbdoc: Returns the result of the most recent operation on this namespace.
	returns: An ESP-IDF result code, or #NVSErrorInvalidHandle when closed.
	End Rem
	Method LastError:Int()
		If Not handle Then Return NVSErrorInvalidHandle
		Return _NVSLastError(handle)
	End Method

	Rem
	bbdoc: Makes pending writes and erasures durable in flash.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method Commit:Int()
		Return _NVSCommit(handle)
	End Method

	Rem
	bbdoc: Marks one stored key for erasure.
	param: ASCII key name of at most 15 characters.
	returns: ESP_OK on success or an ESP-IDF error code.
	about: Call #Commit to persist the erasure.
	End Rem
	Method EraseKey:Int(key:String)
		Return _NVSEraseKey(handle, key)
	End Method

	Rem
	bbdoc: Marks every key in this namespace for erasure.
	returns: ESP_OK on success or an ESP-IDF error code.
	about: Other namespaces are unaffected. Call #Commit to persist the erasure.
	End Rem
	Method EraseAll:Int()
		Return _NVSEraseAll(handle)
	End Method

	Rem
	bbdoc: Determines the native type of a stored value.
	param: ASCII key name of at most 15 characters.
	returns: One of the NVSType constants, or -1 when the lookup fails.
	about: #LastError distinguishes a missing key from other failures.
	End Rem
	Method ValueType:Int(key:String)
		Return _NVSValueType(handle, key)
	End Method

	Rem
	bbdoc: Stores a signed 32-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Value to store.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetInt:Int(key:String, value:Int)
		Return _NVSSetInt(handle, key, value)
	End Method

	Rem
	bbdoc: Stores an unsigned 32-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Value to store.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetUInt:Int(key:String, value:UInt)
		Return _NVSSetUInt(handle, key, value)
	End Method

	Rem
	bbdoc: Stores a signed 64-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Value to store.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetLong:Int(key:String, value:Long)
		Return _NVSSetLong(handle, key, value)
	End Method

	Rem
	bbdoc: Stores an unsigned 64-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Value to store.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetULong:Int(key:String, value:ULong)
		Return _NVSSetULong(handle, key, value)
	End Method

	Rem
	bbdoc: Stores a 32-bit floating-point value.
	param: ASCII key name of at most 15 characters.
	param: Value to store.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetFloat:Int(key:String, value:Float)
		Return _NVSSetFloat(handle, key, value)
	End Method

	Rem
	bbdoc: Stores a 64-bit floating-point value.
	param: ASCII key name of at most 15 characters.
	param: Value to store.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetDouble:Int(key:String, value:Double)
		Return _NVSSetDouble(handle, key, value)
	End Method

	Rem
	bbdoc: Stores a BlitzMax string as zero-terminated UTF-8.
	param: ASCII key name of at most 15 characters.
	param: String value to store.
	returns: ESP_OK on success or an ESP-IDF error code.
	about: The key must be ASCII, but the string value may contain Unicode text.
	End Rem
	Method SetString:Int(key:String, value:String)
		Return _NVSSetString(handle, key, value)
	End Method

	Rem
	bbdoc: Stores a managed byte array as an NVS blob.
	param: ASCII key name of at most 15 characters.
	param: Bytes to store.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetBytes:Int(key:String, value:Byte[])
		Return _NVSSetBytes(handle, key, value)
	End Method

	Rem
	bbdoc: Stores caller-owned raw memory as an NVS blob.
	param: ASCII key name of at most 15 characters.
	param: Address of the first byte, or Null when size is zero.
	param: Number of bytes to store.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetBytes:Int(key:String, data:Byte Ptr, size:UInt)
		Return _NVSSetMemory(handle, key, data, size)
	End Method

	Rem
	bbdoc: Reads a signed 32-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Receives the stored value.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method GetInt:Int(key:String, value:Int Var)
		Return _NVSGetInt(handle, key, value)
	End Method

	Rem
	bbdoc: Reads an unsigned 32-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Receives the stored value.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method GetUInt:Int(key:String, value:UInt Var)
		Return _NVSGetUInt(handle, key, value)
	End Method

	Rem
	bbdoc: Reads a signed 64-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Receives the stored value.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method GetLong:Int(key:String, value:Long Var)
		Return _NVSGetLong(handle, key, value)
	End Method

	Rem
	bbdoc: Reads an unsigned 64-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Receives the stored value.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method GetULong:Int(key:String, value:ULong Var)
		Return _NVSGetULong(handle, key, value)
	End Method

	Rem
	bbdoc: Reads a 32-bit floating-point value.
	param: ASCII key name of at most 15 characters.
	param: Receives the stored value.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method GetFloat:Int(key:String, value:Float Var)
		Return _NVSGetFloat(handle, key, value)
	End Method

	Rem
	bbdoc: Reads a 64-bit floating-point value.
	param: ASCII key name of at most 15 characters.
	param: Receives the stored value.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method GetDouble:Int(key:String, value:Double Var)
		Return _NVSGetDouble(handle, key, value)
	End Method

	Rem
	bbdoc: Reads a stored UTF-8 string.
	param: ASCII key name of at most 15 characters.
	returns: The stored string, or an empty string when the operation fails.
	about: Use #TryGetString or inspect #LastError when an empty stored string
	must be distinguished from a failed read.
	End Rem
	Method GetString:String(key:String)
		Return _NVSGetString(handle, key)
	End Method

	Rem
	bbdoc: Attempts to read a stored UTF-8 string without ambiguity.
	param: ASCII key name of at most 15 characters.
	param: Receives the stored string on success.
	returns: True on success; False when the key is absent or another error occurs.
	about: Inspect #LastError to distinguish a missing key from other failures.
	End Rem
	Method TryGetString:Int(key:String, value:String Var)
		value = GetString(key)
		Return LastError() = 0
	End Method

	Rem
	bbdoc: Reads a stored blob into a new managed byte array.
	param: ASCII key name of at most 15 characters.
	returns: The stored bytes, or an empty array when the operation fails.
	about: Use #TryGetBytes or inspect #LastError when an empty stored blob must
	be distinguished from a failed read.
	End Rem
	Method GetBytes:Byte[](key:String)
		Return _NVSGetBytes(handle, key)
	End Method

	Rem
	bbdoc: Reads a stored blob into caller-owned raw memory.
	param: ASCII key name of at most 15 characters.
	param: Destination address, or Null when querying a zero-length value.
	param: Number of writable bytes at data.
	param: Receives the required or copied blob size.
	returns: ESP_OK on success, #NVSErrorInvalidLength when capacity is too small,
	or another ESP-IDF error code.
	End Rem
	Method GetBytes:Int(key:String, data:Byte Ptr, capacity:UInt, actualSize:UInt Var)
		Return _NVSGetMemory(handle, key, data, capacity, actualSize)
	End Method

	Rem
	bbdoc: Attempts to read a stored blob into a managed byte array.
	param: ASCII key name of at most 15 characters.
	param: Receives the stored bytes on success.
	returns: True on success; False when the key is absent or another error occurs.
	about: Inspect #LastError to distinguish a missing key from other failures.
	End Rem
	Method TryGetBytes:Int(key:String, value:Byte[] Var)
		value = GetBytes(key)
		Return LastError() = 0
	End Method
End Type

Rem
bbdoc: Opens a namespace in the default NVS partition.
param: Non-empty ASCII namespace name of at most 15 characters.
param: True to require an existing namespace and prohibit writes.
returns: An open namespace, or Null on failure.
about: The default NVS partition is initialized automatically. When this
function returns Null, call #NVSLastOpenError for the ESP-IDF result code.
End Rem
Function OpenNVS:TNVSNamespace(name:String, openReadOnly:Int = False)
	Local mode:Int = NVSReadWrite
	If openReadOnly Then mode = NVSReadOnly
	Local nativeHandle:Byte Ptr = _NVSOpen(name, mode)
	If Not nativeHandle Then Return Null
	Local result:TNVSNamespace = New TNVSNamespace
	result.handle = nativeHandle
	Return result
End Function

Rem
bbdoc: Reads usage statistics for the default NVS partition.
param: Receives the partition statistics.
returns: ESP_OK on success or an ESP-IDF error code.
End Rem
Function NVSStatistics:Int(statistics:SNVSStatistics Var)
	Return _NVSStatistics(statistics.usedEntries, statistics.freeEntries, statistics.totalEntries, statistics.namespaceCount)
End Function
?
