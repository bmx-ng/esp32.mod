#ifndef BLITZMAX_EMBEDDED_PLATFORM_H
#define BLITZMAX_EMBEDDED_PLATFORM_H

#include <stdint.h>

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#if defined(BMX_EMBEDDED_MULTICORE) && BMX_EMBEDDED_MULTICORE
#include "freertos/task.h"
#endif

void *bmx_esp32_managed_arena_acquire(uint32_t capacity, uint32_t alignment);
int32_t bmx_esp32_managed_context_valid(void);
int32_t bmx_esp32_managed_task_bound(void);
uint32_t bmx_esp32_managed_context_violation_count(void);
#if defined(BMX_EMBEDDED_MULTICORE) && BMX_EMBEDDED_MULTICORE
uint32_t bmx_esp32_managed_gc_context_index(void);
int32_t bmx_esp32_managed_gc_bind(uint32_t slot);
void bmx_esp32_managed_gc_unbind(uint32_t slot);
int32_t bmx_esp32_managed_gc_worker_start(void (*entry)(void));
int32_t bmx_esp32_managed_gc_worker_join(void);
#endif

typedef void (*BMXESP32ManagedCallback)(void *argument);

/* Native code must enter managed code through this gate. It dispatches only
   on the CPU-0 FreeRTOS task which owns the managed arena. Producers running
   in an ISR or another task must first hand off plain native data to that
   owner task. A managed Object retained by native code across calls must be
   protected with bmx_embedded_object_root_retain/release. */
int32_t bmx_esp32_managed_callback_dispatch(
    BMXESP32ManagedCallback callback, void *argument);
uint32_t bmx_esp32_managed_callback_dispatch_count(void);
uint32_t bmx_esp32_managed_callback_rejection_count(void);

/* The ordinary build binds the arena to one CPU-0 FreeRTOS task. The opt-in
   multicore build also recognizes a registered worker by task handle. */
#define BMX_EMBEDDED_PLATFORM_CONTEXT_VALID() \
    bmx_esp32_managed_context_valid()
#if defined(BMX_EMBEDDED_MULTICORE) && BMX_EMBEDDED_MULTICORE
#define BMX_EMBEDDED_PLATFORM_CONTEXT_INDEX() bmx_esp32_managed_gc_context_index()
#define BMX_EMBEDDED_PLATFORM_GC_BIND(slot) bmx_esp32_managed_gc_bind(slot)
#define BMX_EMBEDDED_PLATFORM_GC_UNBIND(slot) bmx_esp32_managed_gc_unbind(slot)
#define BMX_EMBEDDED_PLATFORM_GC_WORKER_START(entry) bmx_esp32_managed_gc_worker_start(entry)
#define BMX_EMBEDDED_PLATFORM_GC_WORKER_JOIN() bmx_esp32_managed_gc_worker_join()
#define BMX_EMBEDDED_PLATFORM_GC_WAIT() taskYIELD()
#else
#define BMX_EMBEDDED_PLATFORM_CONTEXT_INDEX() xPortGetCoreID()
#endif
#define BMX_EMBEDDED_PLATFORM_PANIC(message) esp_system_abort(message)
#define BMX_EMBEDDED_PLATFORM_ARENA_ACQUIRE(capacity, alignment) \
    bmx_esp32_managed_arena_acquire((capacity), (alignment))

#endif
