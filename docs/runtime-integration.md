# Runtime integration and contributor notes

This document is for contributors adding native ESP-IDF integrations or
debugging the embedded runtime. Application developers should normally use the
event-based `Embedded.*` and `ESP32.*` modules described in the main README.

## Runtime ownership

The compact runtime ABI, allocator, precise garbage collector, strings, arrays,
objects, and exceptions are implemented by `embedded.mod`. `esp32.mod` supplies
the ESP32 managed arena, ESP-IDF application integration, and target adapters.

The managed arena is reserved once from internal, byte-addressable RAM through
ESP-IDF's capability allocator, unless a validated profile explicitly places it
in PSRAM. The collector owns that complete arena for the application lifetime;
unmanaged ESP-IDF allocations remain outside it.

The first managed-arena acquisition binds the runtime to the current FreeRTOS
task on ESP32 CPU 0. Later managed operations from another task, CPU 1, or
interrupt context are rejected and counted by
`ManagedContextViolationCount()`.

## Native callback contract

Native integrations must enter BlitzMax through
`bmx_esp32_managed_callback_dispatch()`. The dispatcher invokes callbacks only
on the CPU-0 task that owns the managed arena.

Interrupt handlers and other FreeRTOS tasks must enqueue plain native data and
let the owner task drain it. They must never allocate, collect, throw, or call
BlitzMax code directly. Existing GPIO, timer, UART, Wi-Fi, socket, and BLE
adapters demonstrate this deferred-event pattern.

Raw managed pointers are borrowed only for the duration of a native call.
Native code that keeps an Object after the call returns must retain it on the
owner task with `bmx_embedded_object_root_retain()` and release the returned
token there with `bmx_embedded_object_root_release()`. Strings and arrays must
remain reachable through that rooted Object instead of being retained as
untracked raw pointers.

A managed exception must be caught before returning through the native callback
boundary. Cleanup paths must not assume that finalization or allocation cannot
fail.

## Adding an adapter

When adding an ESP-IDF integration:

1. Keep ESP-IDF handles and callback payloads in native storage.
2. Validate controller, pin, buffer, and length arguments before entering the
   driver.
3. Copy or root managed data that must outlive a native call.
4. Queue callbacks raised by interrupts or foreign tasks.
5. Drain those queues from the managed owner task through the normal system or
   event APIs.
6. Make initialization and teardown repeatable, including partial-failure
   cleanup.
7. Add both a shared conformance fixture where the contract is portable and an
   ESP32 example for target-specific behaviour.
8. Verify that unused components remain excluded when their module is not
   imported.

## Runtime validation

The runtime-focused examples are intentionally more exhaustive than ordinary
application samples:

- `examples/managed_smoke.bmx` checks basic managed allocation and collection.
- `examples/managed_stress.bmx` exercises automatic and explicit collection,
  cyclic garbage, inheritance, tracing, strings, fragment reuse, and retained
  graph integrity.
- `examples/managed_fault_safety.bmx` checks owner-task dispatch, foreign-task
  rejection, managed root lifetime, and heap integrity.
- `examples/managed_oom.bmx` exercises allocation failure, collection and retry,
  raw-memory resize preservation, catchable Object/Array/String failures,
  finalizer-exception unwinding, and later recovery.

Run the ESP32 build matrix with:

```sh
tests/run_bmk_smoke.sh
```

Select another installed profile with `ESP32_TEST_TARGET`, for example:

```sh
ESP32_TEST_TARGET=baguette_c3 tests/run_bmk_smoke.sh
```

The matrix compiles the application examples and the shared `embedded.mod`
conformance fixtures. Hardware-facing examples should additionally be uploaded
to representative physical boards before publishing target support.
