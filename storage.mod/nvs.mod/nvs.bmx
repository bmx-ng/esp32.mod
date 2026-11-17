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

ModuleInfo "Version: 0.2"
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
bbdoc: Opens a namespace for reading and writing while physically purging replaced or erased data.
about: Purging adds flash writes and is intended for sensitive values whose old
contents must not remain recoverable from ordinary flash inspection.
End Rem
Const NVSReadWritePurge:Int = 2

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

Private

Extern "C"
	Function _NVSInitialize:Int() = "bmx_esp32_nvs_initialize"
	Function _NVSInitializePartition:Int(partitionName:String) = "bmx_esp32_nvs_initialize_partition"
	Function _NVSDeinitialize:Int() = "bmx_esp32_nvs_deinitialize"
	Function _NVSDeinitializePartition:Int(partitionName:String) = "bmx_esp32_nvs_deinitialize_partition"
	Function _NVSErasePartition:Int() = "bmx_esp32_nvs_erase_partition"
	Function _NVSEraseNamedPartition:Int(partitionName:String) = "bmx_esp32_nvs_erase_named_partition"
	Function _NVSLastOpenError:Int() = "bmx_esp32_nvs_last_open_error"
	Function _NVSResultName:String(result:Int) = "bmx_esp32_nvs_result_name"
	Function _NVSOpen:Byte Ptr(name:String, mode:Int) = "bmx_esp32_nvs_open"
	Function _NVSOpenPartition:Byte Ptr(partitionName:String, name:String, mode:Int) = "bmx_esp32_nvs_open_partition"
	Function _NVSClose(handle:Byte Ptr) = "bmx_esp32_nvs_close"
	Function _NVSLastError:Int(handle:Byte Ptr) = "bmx_esp32_nvs_last_error"
	Function _NVSCommit:Int(handle:Byte Ptr) = "bmx_esp32_nvs_commit"
	Function _NVSEraseKey:Int(handle:Byte Ptr, key:String) = "bmx_esp32_nvs_erase_key"
	Function _NVSEraseAll:Int(handle:Byte Ptr) = "bmx_esp32_nvs_erase_all"
	Function _NVSPurgeAll:Int(handle:Byte Ptr) = "bmx_esp32_nvs_purge_all"
	Function _NVSValueType:Int(handle:Byte Ptr, key:String) = "bmx_esp32_nvs_value_type"
	Function _NVSSetInt8:Int(handle:Byte Ptr, key:String, value:Int) = "bmx_esp32_nvs_set_i8"
	Function _NVSSetUInt8:Int(handle:Byte Ptr, key:String, value:Int) = "bmx_esp32_nvs_set_u8"
	Function _NVSSetInt16:Int(handle:Byte Ptr, key:String, value:Int) = "bmx_esp32_nvs_set_i16"
	Function _NVSSetUInt16:Int(handle:Byte Ptr, key:String, value:Int) = "bmx_esp32_nvs_set_u16"
	Function _NVSSetInt:Int(handle:Byte Ptr, key:String, value:Int) = "bmx_esp32_nvs_set_i32"
	Function _NVSSetUInt:Int(handle:Byte Ptr, key:String, value:UInt) = "bmx_esp32_nvs_set_u32"
	Function _NVSSetLong:Int(handle:Byte Ptr, key:String, value:Long) = "bmx_esp32_nvs_set_i64"
	Function _NVSSetULong:Int(handle:Byte Ptr, key:String, value:ULong) = "bmx_esp32_nvs_set_u64"
	Function _NVSSetFloat:Int(handle:Byte Ptr, key:String, value:Float) = "bmx_esp32_nvs_set_float"
	Function _NVSSetDouble:Int(handle:Byte Ptr, key:String, value:Double) = "bmx_esp32_nvs_set_double"
	Function _NVSSetString:Int(handle:Byte Ptr, key:String, value:String) = "bmx_esp32_nvs_set_string"
	Function _NVSSetBytes:Int(handle:Byte Ptr, key:String, value:Byte[]) = "bmx_esp32_nvs_set_blob"
	Function _NVSSetMemory:Int(handle:Byte Ptr, key:String, data:Byte Ptr, size:UInt) = "bmx_esp32_nvs_set_memory"
	Function _NVSGetInt8:Int(handle:Byte Ptr, key:String, value:Int Var) = "bmx_esp32_nvs_get_i8"
	Function _NVSGetUInt8:Int(handle:Byte Ptr, key:String, value:Int Var) = "bmx_esp32_nvs_get_u8"
	Function _NVSGetInt16:Int(handle:Byte Ptr, key:String, value:Int Var) = "bmx_esp32_nvs_get_i16"
	Function _NVSGetUInt16:Int(handle:Byte Ptr, key:String, value:Int Var) = "bmx_esp32_nvs_get_u16"
	Function _NVSGetInt:Int(handle:Byte Ptr, key:String, value:Int Var) = "bmx_esp32_nvs_get_i32"
	Function _NVSGetUInt:Int(handle:Byte Ptr, key:String, value:UInt Var) = "bmx_esp32_nvs_get_u32"
	Function _NVSGetLong:Int(handle:Byte Ptr, key:String, value:Long Var) = "bmx_esp32_nvs_get_i64"
	Function _NVSGetULong:Int(handle:Byte Ptr, key:String, value:ULong Var) = "bmx_esp32_nvs_get_u64"
	Function _NVSGetFloat:Int(handle:Byte Ptr, key:String, value:Float Var) = "bmx_esp32_nvs_get_float"
	Function _NVSGetDouble:Int(handle:Byte Ptr, key:String, value:Double Var) = "bmx_esp32_nvs_get_double"
	Function _NVSGetString:String(handle:Byte Ptr, key:String) = "bmx_esp32_nvs_get_string"
	Function _NVSGetBytes:Byte[](handle:Byte Ptr, key:String) = "bmx_esp32_nvs_get_blob"
	Function _NVSGetMemory:Int(handle:Byte Ptr, key:String, data:Byte Ptr, capacity:UInt, actualSize:UInt Var) = "bmx_esp32_nvs_get_memory"
	Function _NVSUsedEntryCount:Int(handle:Byte Ptr, used:UInt Var) = "bmx_esp32_nvs_used_entry_count"
	Function _NVSOpenIterator:Byte Ptr(handle:Byte Ptr, valueType:Int) = "bmx_esp32_nvs_open_iterator"
	Function _NVSCloseIterator(handle:Byte Ptr) = "bmx_esp32_nvs_close_iterator"
	Function _NVSIteratorNext:Int(handle:Byte Ptr) = "bmx_esp32_nvs_iterator_next"
	Function _NVSIteratorLastError:Int(handle:Byte Ptr) = "bmx_esp32_nvs_iterator_last_error"
	Function _NVSIteratorNamespace:String(handle:Byte Ptr) = "bmx_esp32_nvs_iterator_namespace"
	Function _NVSIteratorKey:String(handle:Byte Ptr) = "bmx_esp32_nvs_iterator_key"
	Function _NVSIteratorType:Int(handle:Byte Ptr) = "bmx_esp32_nvs_iterator_type"
	Function _NVSStatistics:Int(used:UInt Var, free:UInt Var, available:UInt Var, ..
		total:UInt Var, namespaces:UInt Var) = "bmx_esp32_nvs_statistics"
	Function _NVSPartitionStatistics:Int(partitionName:String, used:UInt Var, free:UInt Var, available:UInt Var, ..
		total:UInt Var, namespaces:UInt Var) = "bmx_esp32_nvs_partition_statistics"
End Extern

Public

Rem
bbdoc: Initializes the default NVS partition.
returns: ESP_OK on success or an ESP-IDF error code.
about: #OpenNVS initializes the default partition automatically, so most
applications do not need to call this function directly.
End Rem
Function NVSInitialize:Int()
	Return _NVSInitialize()
End Function

Rem
bbdoc: Initializes a named NVS partition.
param: ASCII partition label from the active partition table.
returns: ESP_OK on success or an ESP-IDF error code.
about: #OpenNVSPartition initializes its selected partition automatically.
End Rem
Function NVSInitializePartition:Int(partitionName:String)
	Return _NVSInitializePartition(partitionName)
End Function

Rem
bbdoc: Deinitializes the default NVS partition.
returns: ESP_OK on success or an ESP-IDF error code.
about: Close every namespace using the partition before deinitializing it.
End Rem
Function NVSDeinitialize:Int()
	Return _NVSDeinitialize()
End Function

Rem
bbdoc: Deinitializes a named NVS partition.
param: ASCII partition label from the active partition table.
returns: ESP_OK on success or an ESP-IDF error code.
about: Close every namespace using the partition before deinitializing it.
End Rem
Function NVSDeinitializePartition:Int(partitionName:String)
	Return _NVSDeinitializePartition(partitionName)
End Function

Rem
bbdoc: Erases every value and namespace in the default NVS partition.
returns: ESP_OK on success or an ESP-IDF error code.
about: This operation is destructive and deinitializes the partition. Every
existing handle becomes invalid; call #NVSInitialize or #OpenNVS afterwards to
use the empty partition again. It is intended for explicit factory reset or
carefully chosen recovery logic, never routine startup.
End Rem
Function NVSEraseDefaultPartition:Int()
	Return _NVSErasePartition()
End Function

Rem
bbdoc: Erases every value and namespace in a named NVS partition.
param: ASCII partition label from the active partition table.
returns: ESP_OK on success or an ESP-IDF error code.
about: This operation is destructive and deinitializes the partition. Every
existing handle becomes invalid; call #NVSInitializePartition or
#OpenNVSPartition afterwards to use the empty partition again.
End Rem
Function NVSErasePartition:Int(partitionName:String)
	Return _NVSEraseNamedPartition(partitionName)
End Function

Rem
bbdoc: Returns the result from the most recent failed or successful namespace open.
returns: An ESP-IDF result code.
End Rem
Function NVSLastOpenError:Int()
	Return _NVSLastOpenError()
End Function

Rem
bbdoc: Returns the symbolic ESP-IDF name for an NVS result code.
param: Result code returned by an NVS operation.
returns: A name such as `ESP_OK` or `ESP_ERR_NVS_NOT_FOUND`.
End Rem
Function NVSResultName:String(result:Int)
	Return _NVSResultName(result)
End Function

Rem
bbdoc: Storage usage statistics for one NVS partition.
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
	bbdoc: Number of entries currently available for application data.
	End Rem
	Field availableEntries:UInt
	Rem
	bbdoc: Total number of entries in the selected NVS partition.
	End Rem
	Field totalEntries:UInt
	Rem
	bbdoc: Number of namespaces currently present in the partition.
	End Rem
	Field namespaceCount:UInt
End Struct

Rem
bbdoc: Iterator over keys in one open NVS namespace.
about: The iterator initially refers to the first matching entry. Read its
properties, then call #Next until that method returns #NVSErrorNotFound. Close
the iterator early when the application does not consume every entry.
End Rem
Type TNVSEntryIterator Final
	Rem
	bbdoc: Native iterator handle used by this wrapper.
	about: Application code should treat this field as opaque.
	End Rem
	Field handle:Byte Ptr
	Rem
	bbdoc: Closes the native iterator and releases its resources.
	End Rem
	Method Close()
		If handle Then
			_NVSCloseIterator(handle)
			handle = Null
		End If
	End Method

	Rem
	bbdoc: Closes the iterator when the object is collected.
	End Rem
	Method Delete()
		Close()
	End Method

	Rem
	bbdoc: Advances to the next matching entry.
	returns: ESP_OK when another entry is available, #NVSErrorNotFound after the
	final entry, or another ESP-IDF error code.
	End Rem
	Method Next:Int()
		If Not handle Then Return NVSErrorInvalidHandle
		Return _NVSIteratorNext(handle)
	End Method

	Rem
	bbdoc: Returns the result of the most recent iterator operation.
	returns: An ESP-IDF result code, or #NVSErrorInvalidHandle when closed.
	End Rem
	Method LastError:Int()
		If Not handle Then Return NVSErrorInvalidHandle
		Return _NVSIteratorLastError(handle)
	End Method

	Rem
	bbdoc: Returns the namespace containing the current entry.
	returns: The namespace name, or an empty string when closed.
	End Rem
	Method NamespaceName:String()
		If Not handle Then Return ""
		Return _NVSIteratorNamespace(handle)
	End Method

	Rem
	bbdoc: Returns the key name of the current entry.
	returns: The key name, or an empty string when closed.
	End Rem
	Method Key:String()
		If Not handle Then Return ""
		Return _NVSIteratorKey(handle)
	End Method

	Rem
	bbdoc: Returns the native type of the current entry.
	returns: One of the NVSType constants, or -1 when closed.
	End Rem
	Method ValueType:Int()
		If Not handle Then Return -1
		Return _NVSIteratorType(handle)
	End Method
End Type

Rem
bbdoc: An open namespace in an NVS partition.
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
	bbdoc: Physically purges flash occupied by values already marked as erased.
	returns: ESP_OK on success or an ESP-IDF error code.
	about: Call #Commit to make the purge durable. This adds flash write cycles.
	Use it when a namespace previously used ordinary read/write mode and stale
	sensitive values need to be removed. It does not erase current live keys.
	End Rem
	Method PurgeAll:Int()
		Return _NVSPurgeAll(handle)
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
	bbdoc: Counts NVS entries occupied by values in this namespace.
	param: Receives the used entry count, excluding the namespace metadata entry.
	returns: ESP_OK on success or an ESP-IDF error code.
	about: Strings and blobs may occupy multiple entries, so this is not a key count.
	End Rem
	Method UsedEntryCount:Int(count:UInt Var)
		Return _NVSUsedEntryCount(handle, count)
	End Method

	Rem
	bbdoc: Opens an iterator over keys in this namespace.
	param: NVSTypeAny or a specific NVSType constant used as a filter.
	returns: An iterator positioned at the first match, or Null when no entry
	matches or another error occurs.
	about: Inspect #LastError when this method returns Null.
	End Rem
	Method OpenIterator:TNVSEntryIterator(valueType:Int = NVSTypeAny)
		Local nativeHandle:Byte Ptr = _NVSOpenIterator(handle, valueType)
		If Not nativeHandle Then Return Null
		Local result:TNVSEntryIterator = New TNVSEntryIterator
		result.handle = nativeHandle
		Return result
	End Method

	Rem
	bbdoc: Stores a signed 8-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Value from -128 through 127.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetInt8:Int(key:String, value:Int)
		Return _NVSSetInt8(handle, key, value)
	End Method

	Rem
	bbdoc: Stores an unsigned 8-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Value from 0 through 255.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetUInt8:Int(key:String, value:Int)
		Return _NVSSetUInt8(handle, key, value)
	End Method

	Rem
	bbdoc: Stores a signed 16-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Value from -32768 through 32767.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetInt16:Int(key:String, value:Int)
		Return _NVSSetInt16(handle, key, value)
	End Method

	Rem
	bbdoc: Stores an unsigned 16-bit integer.
	param: ASCII key name of at most 15 characters.
	param: Value from 0 through 65535.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method SetUInt16:Int(key:String, value:Int)
		Return _NVSSetUInt16(handle, key, value)
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
	bbdoc: Reads a signed 8-bit integer into an Int.
	param: ASCII key name of at most 15 characters.
	param: Receives a value from -128 through 127.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method GetInt8:Int(key:String, value:Int Var)
		Return _NVSGetInt8(handle, key, value)
	End Method

	Rem
	bbdoc: Reads an unsigned 8-bit integer into an Int.
	param: ASCII key name of at most 15 characters.
	param: Receives a value from 0 through 255.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method GetUInt8:Int(key:String, value:Int Var)
		Return _NVSGetUInt8(handle, key, value)
	End Method

	Rem
	bbdoc: Reads a signed 16-bit integer into an Int.
	param: ASCII key name of at most 15 characters.
	param: Receives a value from -32768 through 32767.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method GetInt16:Int(key:String, value:Int Var)
		Return _NVSGetInt16(handle, key, value)
	End Method

	Rem
	bbdoc: Reads an unsigned 16-bit integer into an Int.
	param: ASCII key name of at most 15 characters.
	param: Receives a value from 0 through 65535.
	returns: ESP_OK on success or an ESP-IDF error code.
	End Rem
	Method GetUInt16:Int(key:String, value:Int Var)
		Return _NVSGetUInt16(handle, key, value)
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
	Return OpenNVSWithMode(name, mode)
End Function

Rem
bbdoc: Opens a namespace in the default NVS partition with an explicit mode.
param: Non-empty ASCII namespace name of at most 15 characters.
param: NVSReadOnly, NVSReadWrite, or NVSReadWritePurge.
returns: An open namespace, or Null on failure.
about: The default NVS partition is initialized automatically. Purge mode is
available with ESP-IDF 6.1 and later. When this function returns Null, call
#NVSLastOpenError for the ESP-IDF result code.
End Rem
Function OpenNVSWithMode:TNVSNamespace(name:String, mode:Int)
	Local nativeHandle:Byte Ptr = _NVSOpen(name, mode)
	If Not nativeHandle Then Return Null
	Local result:TNVSNamespace = New TNVSNamespace
	result.handle = nativeHandle
	Return result
End Function

Rem
bbdoc: Opens a namespace in a named NVS partition.
param: ASCII partition label from the active partition table.
param: Non-empty ASCII namespace name of at most 15 characters.
param: True to require an existing namespace and prohibit writes.
returns: An open namespace, or Null on failure.
about: The selected partition is initialized automatically. When this function
returns Null, call #NVSLastOpenError for the ESP-IDF result code.
End Rem
Function OpenNVSPartition:TNVSNamespace(partitionName:String, name:String, openReadOnly:Int = False)
	Local mode:Int = NVSReadWrite
	If openReadOnly Then mode = NVSReadOnly
	Return OpenNVSPartitionWithMode(partitionName, name, mode)
End Function

Rem
bbdoc: Opens a namespace in a named NVS partition with an explicit mode.
param: ASCII partition label from the active partition table.
param: Non-empty ASCII namespace name of at most 15 characters.
param: NVSReadOnly, NVSReadWrite, or NVSReadWritePurge.
returns: An open namespace, or Null on failure.
about: The partition is initialized automatically. Purge mode is available
with ESP-IDF 6.1 and later. When this function returns Null, call
#NVSLastOpenError for the ESP-IDF result code.
End Rem
Function OpenNVSPartitionWithMode:TNVSNamespace(partitionName:String, name:String, mode:Int)
	Local nativeHandle:Byte Ptr = _NVSOpenPartition(partitionName, name, mode)
	If Not nativeHandle Then Return Null
	Local result:TNVSNamespace = New TNVSNamespace
	result.handle = nativeHandle
	Return result
End Function

Rem
bbdoc: Reads usage statistics for an NVS partition.
param: Receives the partition statistics.
param: Optional ASCII partition label; an empty string selects the default NVS partition.
returns: ESP_OK on success or an ESP-IDF error code.
End Rem
Function NVSStatistics:Int(statistics:SNVSStatistics Var, partitionName:String = "")
	If partitionName Then
		Return _NVSPartitionStatistics(partitionName, statistics.usedEntries, statistics.freeEntries, ..
			statistics.availableEntries, statistics.totalEntries, statistics.namespaceCount)
	End If
	Return _NVSStatistics(statistics.usedEntries, statistics.freeEntries, ..
		statistics.availableEntries, statistics.totalEntries, statistics.namespaceCount)
End Function
?
