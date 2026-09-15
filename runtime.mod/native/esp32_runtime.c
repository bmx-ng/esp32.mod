#include <stdint.h>
#include <stdio.h>

#include "sdkconfig.h"
#include "blitzmax/embedded_runtime.h"
#include "blitzmax/embedded_time.h"
#include "blitzmax/embedded_platform.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_psram.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

static void *bmx_esp32_managed_arena;
static uint32_t bmx_esp32_managed_arena_size;
static TaskHandle_t bmx_esp32_managed_owner_task;
static uint32_t bmx_esp32_managed_context_violations;
static uint32_t bmx_esp32_managed_callback_dispatches;
static uint32_t bmx_esp32_managed_callback_rejections;

#ifndef BMX_EMBEDDED_ARENA_IN_PSRAM
#define BMX_EMBEDDED_ARENA_IN_PSRAM 0
#endif

static int32_t bmx_esp32_psram_initialized(void) {
#if CONFIG_SPIRAM
    return esp_psram_is_initialized() ? 1 : 0;
#else
    return 0;
#endif
}

int32_t bmx_esp32_managed_context_valid(void) {
    if (xPortInIsrContext() || xPortGetCoreID() != 0) {
        __atomic_fetch_add(&bmx_esp32_managed_context_violations, 1u, __ATOMIC_RELAXED);
        return 0;
    }
    if (bmx_esp32_managed_owner_task &&
        bmx_esp32_managed_owner_task != xTaskGetCurrentTaskHandle()) {
        __atomic_fetch_add(&bmx_esp32_managed_context_violations, 1u, __ATOMIC_RELAXED);
        return 0;
    }
    return 1;
}

int32_t bmx_esp32_managed_task_bound(void) {
    return bmx_esp32_managed_owner_task != NULL;
}

uint32_t bmx_esp32_managed_context_violation_count(void) {
    return __atomic_load_n(&bmx_esp32_managed_context_violations, __ATOMIC_RELAXED);
}

int32_t bmx_esp32_managed_callback_dispatch(
    BMXESP32ManagedCallback callback, void *argument) {
    if (!callback || !bmx_esp32_managed_owner_task ||
        !bmx_esp32_managed_context_valid()) {
        __atomic_fetch_add(&bmx_esp32_managed_callback_rejections, 1u, __ATOMIC_RELAXED);
        return 0;
    }
    callback(argument);
    __atomic_fetch_add(&bmx_esp32_managed_callback_dispatches, 1u, __ATOMIC_RELAXED);
    return 1;
}

uint32_t bmx_esp32_managed_callback_dispatch_count(void) {
    return __atomic_load_n(&bmx_esp32_managed_callback_dispatches, __ATOMIC_RELAXED);
}

uint32_t bmx_esp32_managed_callback_rejection_count(void) {
    return __atomic_load_n(&bmx_esp32_managed_callback_rejections, __ATOMIC_RELAXED);
}

void *bmx_esp32_managed_arena_acquire(uint32_t capacity, uint32_t alignment) {
    if (!bmx_esp32_managed_context_valid()) return NULL;
    if (bmx_esp32_managed_arena) {
        return capacity == bmx_esp32_managed_arena_size ? bmx_esp32_managed_arena : NULL;
    }
    if (!capacity || !alignment || (alignment & (alignment - 1u))) return NULL;
    bmx_esp32_managed_owner_task = xTaskGetCurrentTaskHandle();
    uint32_t capabilities = MALLOC_CAP_8BIT |
        (BMX_EMBEDDED_ARENA_IN_PSRAM ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL);
    if (BMX_EMBEDDED_ARENA_IN_PSRAM && !bmx_esp32_psram_initialized()) {
        bmx_esp32_managed_owner_task = NULL;
        return NULL;
    }
    bmx_esp32_managed_arena = heap_caps_aligned_alloc(
        (size_t)alignment, (size_t)capacity, capabilities);
    if (bmx_esp32_managed_arena) {
        bmx_esp32_managed_arena_size = capacity;
    } else {
        bmx_esp32_managed_owner_task = NULL;
    }
    return bmx_esp32_managed_arena;
}

uint32_t bmx_esp32_managed_arena_reserved(void) {
    return bmx_esp32_managed_arena_size;
}

int32_t bmx_esp32_managed_arena_valid(void) {
    if (!bmx_esp32_managed_arena || !bmx_esp32_managed_arena_size) return 0;
    const uint8_t *end = (const uint8_t *)bmx_esp32_managed_arena + bmx_esp32_managed_arena_size - 1u;
    int32_t expected_region = BMX_EMBEDDED_ARENA_IN_PSRAM
        ? (esp_ptr_external_ram(bmx_esp32_managed_arena) && esp_ptr_external_ram(end))
        : (esp_ptr_internal(bmx_esp32_managed_arena) && esp_ptr_internal(end));
    return expected_region &&
        esp_ptr_byte_accessible(bmx_esp32_managed_arena) && esp_ptr_byte_accessible(end) &&
        ((uintptr_t)bmx_esp32_managed_arena & 15u) == 0;
}

int32_t bmx_esp32_managed_arena_in_psram(void) {
    return BMX_EMBEDDED_ARENA_IN_PSRAM && bmx_esp32_managed_arena &&
        esp_ptr_external_ram(bmx_esp32_managed_arena);
}

int32_t bmx_esp32_psram_available(void) {
    return bmx_esp32_psram_initialized();
}

uint32_t bmx_esp32_psram_capacity(void) {
#if CONFIG_SPIRAM
    return bmx_esp32_psram_initialized() ? (uint32_t)esp_psram_get_size() : 0;
#else
    return 0;
#endif
}

uint32_t bmx_esp32_psram_free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

uint32_t bmx_esp32_psram_largest_block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

int32_t bmx_esp32_psram_contains(const void *address) {
    return address && esp_ptr_external_ram(address);
}

uint32_t bmx_esp32_internal_heap_free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

uint32_t bmx_esp32_internal_heap_largest_block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

int32_t bmx_esp32_runtime_core(void) {
    return (int32_t)xPortGetCoreID();
}

int32_t bmx_embedded_millisecs(void) {
    return (int32_t)(uint32_t)(esp_timer_get_time() / 1000);
}

void bmx_embedded_delay(int32_t milliseconds) {
    if (milliseconds <= 0) return;
    TickType_t ticks = pdMS_TO_TICKS((uint32_t)milliseconds);
    vTaskDelay(ticks ? ticks : 1);
}

void bmx_embedded_udelay(int32_t microseconds) {
    if (microseconds > 0) esp_rom_delay_us((uint32_t)microseconds);
}

uint64_t bmx_embedded_time_microseconds(void) {
    return (uint64_t)esp_timer_get_time();
}

uint64_t bmx_embedded_time_milliseconds(void) {
    return bmx_embedded_time_microseconds() / 1000u;
}

static void bmx_esp32_sleep_microseconds_chunk(uint32_t microseconds) {
    int64_t started = esp_timer_get_time();
    for (;;) {
        uint64_t elapsed = (uint64_t)(esp_timer_get_time() - started);
        if (elapsed >= microseconds) return;
        uint32_t remaining = microseconds - (uint32_t)elapsed;
        if (remaining > 2000u) {
            TickType_t ticks = pdMS_TO_TICKS((remaining - 1000u) / 1000u);
            if (ticks) {
                vTaskDelay(ticks);
                continue;
            }
        }
        esp_rom_delay_us(remaining);
    }
}

void bmx_embedded_sleep_microseconds(uint64_t microseconds) {
    while (microseconds) {
        uint32_t chunk = microseconds > 60000000u ? 60000000u : (uint32_t)microseconds;
        bmx_esp32_sleep_microseconds_chunk(chunk);
        microseconds -= chunk;
    }
}

void bmx_embedded_sleep_milliseconds(uint32_t milliseconds) {
    bmx_embedded_sleep_microseconds((uint64_t)milliseconds * 1000u);
}

int32_t bmx_esp32_stdio_init_all(void) {
    return 1;
}

int64_t bmx_esp32_stdio_read(void *buffer, int64_t count) {
    if (!buffer || count <= 0) return 0;
    return (int64_t)fread(buffer, 1, (size_t)count, stdin);
}

int64_t bmx_esp32_stdio_write(void *buffer, int64_t count) {
    if (!buffer || count <= 0) return 0;
    return (int64_t)fwrite(buffer, 1, (size_t)count, stdout);
}

void bmx_esp32_stdio_flush(void) {
    fflush(stdout);
    fflush(stderr);
}
