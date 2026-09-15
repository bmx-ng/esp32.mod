' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Import BRL.StandardIO
Import ESP32.System.Partition

Local partitions:TESP32Partition[] = ESP32Partitions()
If Not partitions.length Then RuntimeError "ESP-IDF reported no flash partitions"

Local runningCount:Int
For Local index:Int = 0 Until partitions.length
	Local partition:TESP32Partition = partitions[index]
	If partition.running Then runningCount :+ 1
	Print partition.label + ": " + PartitionTypeName(partition.partitionType) + ..
		"/" + PartitionSubtypeName(partition.partitionType, partition.subtype) + ..
		", address=" + partition.address + ", size=" + partition.size + ..
		", erase=" + partition.eraseSize + ", encrypted=" + partition.encrypted + ..
		", readOnly=" + partition.isReadOnly + ", running=" + partition.running
Next

If runningCount <> 1 Then RuntimeError "Expected exactly one running application partition"
Local nvs:TESP32Partition = FindESP32Partition("nvs", PartitionTypeData, PartitionSubtypeDataNVS)
If Not nvs Then RuntimeError "The standard NVS partition was not found"
If nvs.size = 0 Or nvs.eraseSize = 0 Then RuntimeError "NVS partition geometry is invalid"

Print "ESP32 partition inspection passed"
