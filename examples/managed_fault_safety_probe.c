#include <stdint.h>

#include "blitzmax/embedded_platform.h"
#include "blitzmax/embedded_runtime.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct BMXESP32FaultSafetyProbe {
    TaskHandle_t caller;
    BMXEmbeddedObject *object;
    uint32_t owner_root_token;
    volatile void *allocation;
    volatile uint32_t callback_value;
    volatile uint32_t invalid_root_token;
    volatile int32_t callback_dispatched;
    volatile uint32_t completed;
} BMXESP32FaultSafetyProbe;

static void bmx_esp32_fault_safety_callback(void *argument) {
    volatile uint32_t *value = (volatile uint32_t *)argument;
    *value += 1u;
}

static void bmx_esp32_fault_safety_worker(void *argument) {
    BMXESP32FaultSafetyProbe *probe = (BMXESP32FaultSafetyProbe *)argument;
    probe->allocation = bmx_embedded_arena_allocate(16u);
    probe->callback_dispatched = bmx_esp32_managed_callback_dispatch(
        bmx_esp32_fault_safety_callback, (void *)&probe->callback_value);
    probe->invalid_root_token = bmx_embedded_object_root_retain(probe->object);
    bmx_embedded_object_root_release(probe->owner_root_token);
    probe->completed = 1u;
    xTaskNotifyGive(probe->caller);
    vTaskDelete(NULL);
}

int32_t bmx_esp32_managed_fault_safety_probe(
    BMXEmbeddedObject *object, uint32_t root_token) {
    BMXESP32FaultSafetyProbe probe = {
        .caller = xTaskGetCurrentTaskHandle(),
        .object = object,
        .owner_root_token = root_token,
        .allocation = NULL,
        .callback_value = 0u,
        .invalid_root_token = 0u,
        .callback_dispatched = 0,
        .completed = 0u
    };
    const uint32_t used_before = bmx_embedded_arena_used();
    const uint32_t roots_before = bmx_embedded_object_root_count();
    const uint32_t arena_failures_before = bmx_embedded_arena_failure_count();
    const uint32_t object_failures_before = bmx_embedded_object_failure_count();
    const uint32_t violations_before = bmx_esp32_managed_context_violation_count();
    const uint32_t dispatches_before = bmx_esp32_managed_callback_dispatch_count();
    const uint32_t rejections_before = bmx_esp32_managed_callback_rejection_count();
    TaskHandle_t worker = NULL;

    if (!object || !root_token) return 0;
    if (bmx_esp32_managed_callback_dispatch(NULL, NULL)) return 0;
    if (!bmx_esp32_managed_callback_dispatch(
        bmx_esp32_fault_safety_callback, (void *)&probe.callback_value)) return 0;

    if (xTaskCreatePinnedToCore(bmx_esp32_fault_safety_worker,
        "bmx-fault-probe", 2048, &probe, tskIDLE_PRIORITY + 1, &worker, 0) != pdPASS) return 0;
    if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000))) {
        vTaskDelete(worker);
        return 0;
    }

    return probe.completed && !probe.allocation && probe.callback_value == 1u &&
        !probe.callback_dispatched && !probe.invalid_root_token &&
        bmx_embedded_arena_used() == used_before &&
        bmx_embedded_object_root_count() == roots_before &&
        bmx_embedded_arena_failure_count() == arena_failures_before + 1u &&
        bmx_embedded_object_failure_count() == object_failures_before + 2u &&
        bmx_esp32_managed_context_violation_count() == violations_before + 4u &&
        bmx_esp32_managed_callback_dispatch_count() == dispatches_before + 1u &&
        bmx_esp32_managed_callback_rejection_count() == rejections_before + 2u &&
        bmx_embedded_heap_integrity_valid();
}
