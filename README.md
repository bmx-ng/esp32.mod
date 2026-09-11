# esp32.mod

ESP-IDF platform support for BlitzMax NG embedded applications.

The shared compact runtime ABI, allocator, garbage collector, strings, arrays,
objects, and exceptions live in `embedded.mod`. This repository supplies only
the ESP32 platform adapter, ESP-IDF application integration, and ESP32 APIs.

On ESP32, the managed arena is reserved once from internal, byte-addressable
RAM through ESP-IDF's capability allocator. The collector owns that complete
arena for the lifetime of the application; unmanaged ESP-IDF allocations stay
outside it. `-heap` selects the reservation size and must be a multiple of 16
bytes.

The initial supported targets are the original ESP32 and ESP32-S3, both using
the Xtensa backend and ESP-IDF 6.1.

## Build

`bmk` finds ESP-IDF through `IDF_PATH`, the `esp32.idf` custom option, or an
installation under `~/.espressif`. Build an application with:

```sh
bmk makeapp -a -r -l esp32 -g xtensa -heap 64k -o hello examples/hello_world.bmx
```

Select an ESP32-S3 with `-board esp32s3` (the aliases `s3` and
`baguette_s3` are also accepted). Without `-board`, the original ESP32 is
used unless `esp32.target` is set in `custom.bmk`.

The `baguette_s3` profile selects the board's 8 MB flash and its native USB
Serial/JTAG controller as the primary console.

The build publishes `hello.elf`, `hello.bin`, and `hello.map`. ESP-IDF keeps
the bootloader, partition table, flash arguments, and per-application
`sdkconfig` in the source file's `.bmx` build directory.

The current runtime is single-threaded: managed BlitzMax execution must stay
on ESP32 CPU 0 and outside interrupt context.

`examples/managed_smoke.bmx` provides a quick managed-runtime check.
`examples/managed_stress.bmx` repeatedly exercises automatic and explicit
collection, cyclic garbage, inheritance, object and array tracing, strings,
fragment reuse, and retained graph integrity.
