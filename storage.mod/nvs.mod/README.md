# ESP32.Storage.NVS

`ESP32.Storage.NVS` stores small persistent values in the ESP32's
wear-levelled non-volatile key/value store. It is well suited to preferences,
calibration values, counters, provisioning state, and compact device identity
data.

Use LittleFS, FAT, or an SD card for documents and large binary objects. NVS is
designed around many relatively small values and may consume several internal
entries for one string or blob.

## Quick start

Open a namespace, write one or more values, and commit the transaction:

```blitzmax
SuperStrict

Import BRL.StandardIO
Import ESP32.Storage.NVS

Local settings:TNVSNamespace = OpenNVS("settings")
If Not settings Then
	RuntimeError "Open failed: " + NVSResultName(NVSLastOpenError())
End If

Local result:Int = settings.SetInt("volume", 75)
If result = 0 Then result = settings.SetString("device", "Kitchen")
If result = 0 Then result = settings.Commit()
If result <> 0 Then RuntimeError NVSResultName(result)

settings.Close()
```

`OpenNVS` initializes the default partition automatically. An ordinary
application does not need to call `NVSInitialize` first.

## Namespaces and keys

Namespaces keep unrelated components from colliding. Both namespace names and
keys must be non-empty ASCII names of at most 15 characters. String *values*
are stored as UTF-8 and may contain Unicode text.

Choose stable names once firmware has shipped. Renaming a namespace or key does
not migrate an existing value; it creates a different location.

## Reading values and defaults

Typed reads return an ESP-IDF result code:

```blitzmax
Local brightness:Int
Local result:Int = settings.GetInt("brightness", brightness)

If result = NVSErrorNotFound Then
	brightness = 80
Else If result <> 0 Then
	RuntimeError "Read failed: " + NVSResultName(result)
End If
```

NVS retains the type used to create each value. Reading an integer as a string,
or a signed integer as an unsigned integer, returns
`NVSErrorTypeMismatch`. `ValueType` can inspect a key when an application needs
to migrate old data.

`GetString` and managed-array `GetBytes` return an empty value on failure.
Use `TryGetString` or `TryGetBytes` when an empty stored value must be
distinguished from an error, then inspect `LastError` for the precise cause.

## Supported values

| BlitzMax API | Stored NVS type |
| --- | --- |
| `SetInt8` / `GetInt8` | Signed 8-bit integer |
| `SetUInt8` / `GetUInt8` | Unsigned 8-bit integer |
| `SetInt16` / `GetInt16` | Signed 16-bit integer |
| `SetUInt16` / `GetUInt16` | Unsigned 16-bit integer |
| `SetInt` / `GetInt` | Signed 32-bit integer |
| `SetUInt` / `GetUInt` | Unsigned 32-bit integer |
| `SetLong` / `GetLong` | Signed 64-bit integer |
| `SetULong` / `GetULong` | Unsigned 64-bit integer |
| `SetFloat` / `GetFloat` | 32-bit floating point |
| `SetDouble` / `GetDouble` | 64-bit floating point |
| `SetString` / `GetString` | Zero-terminated UTF-8 string |
| `SetBytes` / `GetBytes` | Binary blob |

Blobs can use managed `Byte[]` values or caller-owned `Byte Ptr` memory. The raw
read reports the required size in `actualSize`; when the supplied capacity is
too small it returns `NVSErrorInvalidLength` without overflowing the buffer.

## Inspecting a namespace

`UsedEntryCount` measures the entries occupied by values in one namespace. It
is useful for capacity diagnostics, but it is not a key count because strings
and blobs can span multiple entries.

Use `OpenIterator` to enumerate keys, optionally filtering by an `NVSType`
constant:

```blitzmax
Local iterator:TNVSEntryIterator = settings.OpenIterator()
If iterator Then
	While True
		Print iterator.Key() + ": type " + iterator.ValueType()
		Local result:Int = iterator.Next()
		If result = NVSErrorNotFound Then Exit
		If result <> 0 Then RuntimeError NVSResultName(result)
	Wend
	iterator.Close()
Else If settings.LastError() <> NVSErrorNotFound Then
	RuntimeError NVSResultName(settings.LastError())
End If
```

The iterator starts on its first entry. Close it when finished, particularly
when leaving the loop before its natural end.

## Named NVS partitions

`OpenNVS` uses the standard partition labelled `nvs`. A custom partition table
may provide additional `data,nvs` partitions; open one explicitly with:

```blitzmax
Local factory:TNVSNamespace = OpenNVSPartition("factory_nvs", "calibration", True)
```

The partition label is independent of the namespace name. The selected
partition is initialized automatically. `NVSInitializePartition`,
`NVSDeinitializePartition`, and the optional partition argument to
`NVSStatistics` provide explicit lifecycle and diagnostics when required.

Deinitialization invalidates handles associated with that partition. Close all
of them first.

## Commit and lifetime

Set and erase operations remain pending until `Commit` succeeds. Group related
changes and commit them together:

```blitzmax
Local result:Int = settings.SetInt("width", 320)
If result = 0 Then result = settings.SetInt("height", 240)
If result = 0 Then result = settings.Commit()
```

Closing a namespace does not make uncommitted writes durable. Always check the
commit result before reporting that configuration was saved.

`EraseKey` removes one value and `EraseAll` removes every value in the current
namespace. Both require a subsequent commit. Neither operation affects another
namespace.

## Read-only access

Pass `True` to require an existing namespace and prevent changes:

```blitzmax
Local settings:TNVSNamespace = OpenNVS("settings", True)
```

Opening a missing namespace read-only fails with `NVSErrorNotFound`, available
through `NVSLastOpenError`.

## Purging sensitive values

Ordinary NVS updates and erasures mark old data as deleted, allowing flash
space to be reclaimed efficiently but potentially leaving the old bytes
physically recoverable. ESP-IDF 6.1 adds continuous purge mode:

```blitzmax
Local secrets:TNVSNamespace = OpenNVSWithMode("secrets", NVSReadWritePurge)
```

In this mode, later updates and erasures physically purge the replaced flash
data. It costs additional flash write cycles. Opening in purge mode does not
remove stale data left by earlier ordinary writes; call `PurgeAll` followed by
`Commit` once when migrating an existing namespace:

```blitzmax
Local result:Int = secrets.PurgeAll()
If result = 0 Then result = secrets.Commit()
```

Purging reduces recovery of deleted plaintext but is not a substitute for NVS
encryption, flash encryption, secure boot, or careful key management.

## Error handling

Zero is success. Most operations return an ESP-IDF result code directly:

```blitzmax
Local result:Int = settings.Commit()
If result <> 0 Then
	Print NVSResultName(result) + " (" + result + ")"
End If
```

`LastError` is particularly useful after APIs whose return value is data, such
as `GetString`, `GetBytes`, and `ValueType`. `NVSLastOpenError` serves the same
purpose when `OpenNVS` returns `Null`.

`NVSErrorNoFreePages` and `NVSErrorNewVersionFound` describe partition-level
problems. Recovery may require erasing stored data, so this module does not do
that silently. A product should decide whether to restore defaults, enter a
maintenance mode, or preserve the partition for diagnosis.

`NVSEraseDefaultPartition` and `NVSErasePartition` are explicit destructive
operations for a deliberate factory reset or recovery policy. They erase every
namespace in the selected partition and invalidate all of its open handles.
They are intentionally separate from initialization and ordinary namespace
erasure.

## Capacity, wear, and security

`NVSStatistics` reports used, free, available, and total entries plus the
namespace count for the default or a named partition. `freeEntries` includes
space reserved internally by NVS; `availableEntries` is the more useful figure
for estimating how much application data can still be stored. Entry counts are
not key counts: namespaces consume metadata, and strings and blobs may span
several entries.

NVS wear-levelling makes occasional settings and state updates practical, but
applications should still avoid committing unchanged values in a tight loop.
Buffer rapidly changing measurements in RAM and commit at an interval suited to
the product's durability requirements.

Ordinary NVS data should not be treated as secret. Without ESP-IDF flash and NVS
encryption, someone with physical flash access can read or alter it. Do not
store passwords or long-term private keys here unless the complete device
security configuration protects the partition.

When NVS encryption is enabled in the ESP-IDF build and the required key
partition is present, normal initialization of the default NVS partition uses
that configuration automatically. Provisioning encryption keys and manually
selecting security schemes for custom partitions are device-manufacturing and
firmware-security concerns; this module does not generate or export those keys
at runtime.

## Examples

- [`nvs_settings.bmx`](../../examples/nvs_settings.bmx) demonstrates defaults,
  typed settings, Unicode string values, conditional writes, and commits.
- [`nvs_boot_counter.bmx`](../../examples/nvs_boot_counter.bmx) maintains a
  small persistent counter across resets.
- [`nvs_storage.bmx`](../../examples/nvs_storage.bmx) is the exhaustive
  regression test. It exercises every current value type, raw memory, read-only
  handles, persistence across a deliberate reboot, statistics, and cleanup.
