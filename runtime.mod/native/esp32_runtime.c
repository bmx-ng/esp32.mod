#include <stdint.h>
#include <stdio.h>

#include "blitzmax/embedded_runtime.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static void *bmx_esp32_managed_arena;
static uint32_t bmx_esp32_managed_arena_size;

void *bmx_esp32_managed_arena_acquire(uint32_t capacity, uint32_t alignment) {
    if (bmx_esp32_managed_arena) {
        return capacity == bmx_esp32_managed_arena_size ? bmx_esp32_managed_arena : NULL;
    }
    if (!capacity || !alignment || (alignment & (alignment - 1u)) ||
        xPortGetCoreID() != 0 || xPortInIsrContext()) return NULL;
    bmx_esp32_managed_arena = heap_caps_aligned_alloc(
        (size_t)alignment, (size_t)capacity, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (bmx_esp32_managed_arena) bmx_esp32_managed_arena_size = capacity;
    return bmx_esp32_managed_arena;
}

uint32_t bmx_esp32_managed_arena_reserved(void) {
    return bmx_esp32_managed_arena_size;
}

int32_t bmx_esp32_managed_arena_valid(void) {
    if (!bmx_esp32_managed_arena || !bmx_esp32_managed_arena_size) return 0;
    const uint8_t *end = (const uint8_t *)bmx_esp32_managed_arena + bmx_esp32_managed_arena_size - 1u;
    return esp_ptr_internal(bmx_esp32_managed_arena) && esp_ptr_internal(end) &&
        esp_ptr_byte_accessible(bmx_esp32_managed_arena) && esp_ptr_byte_accessible(end) &&
        ((uintptr_t)bmx_esp32_managed_arena & 15u) == 0;
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
