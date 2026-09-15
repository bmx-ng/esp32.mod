#ifndef BLITZMAX_EMBEDDED_PLATFORM_H
#define BLITZMAX_EMBEDDED_PLATFORM_H

#include <stdint.h>

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

void *bmx_esp32_managed_arena_acquire(uint32_t capacity, uint32_t alignment);
int32_t bmx_esp32_managed_context_valid(void);
int32_t bmx_esp32_managed_task_bound(void);
uint32_t bmx_esp32_managed_context_violation_count(void);

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

/* The compact runtime is currently single-threaded. The first arena acquisition
   binds it to one CPU-0 FreeRTOS task; interrupt and all other task contexts are
   rejected after that point. */
#define BMX_EMBEDDED_PLATFORM_CONTEXT_VALID() \
    bmx_esp32_managed_context_valid()
#define BMX_EMBEDDED_PLATFORM_PANIC(message) esp_system_abort(message)
#define BMX_EMBEDDED_PLATFORM_ARENA_ACQUIRE(capacity, alignment) \
    bmx_esp32_managed_arena_acquire((capacity), (alignment))

#endif
