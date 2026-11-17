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

## Capacity, wear, and security

`NVSStatistics` reports usage for the default partition. Entry counts are not
key counts: namespaces consume metadata, and strings and blobs may span several
entries.

NVS wear-levelling makes occasional settings and state updates practical, but
applications should still avoid committing unchanged values in a tight loop.
Buffer rapidly changing measurements in RAM and commit at an interval suited to
the product's durability requirements.

Ordinary NVS data should not be treated as secret. Without ESP-IDF flash and NVS
encryption, someone with physical flash access can read or alter it. Do not
store passwords or long-term private keys here unless the complete device
security configuration protects the partition.

## Examples

- [`nvs_settings.bmx`](../../examples/nvs_settings.bmx) demonstrates defaults,
  typed settings, Unicode string values, conditional writes, and commits.
- [`nvs_boot_counter.bmx`](../../examples/nvs_boot_counter.bmx) maintains a
  small persistent counter across resets.
- [`nvs_storage.bmx`](../../examples/nvs_storage.bmx) is the exhaustive
  regression test. It exercises every current value type, raw memory, read-only
  handles, persistence across a deliberate reboot, statistics, and cleanup.

