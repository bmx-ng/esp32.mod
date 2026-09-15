#include <stdint.h>

#include "blitzmax/embedded_runtime.h"
#include "blitzmax/embedded_platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct BMXESP32TaskIsolationProbe {
    TaskHandle_t caller;
    void *volatile allocation;
    volatile uint32_t callback_value;
    volatile int32_t callback_dispatched;
    volatile uint32_t completed;
} BMXESP32TaskIsolationProbe;

static void bmx_esp32_runtime_task_isolation_callback(void *argument) {
    volatile uint32_t *value = (volatile uint32_t *)argument;
    *value += 1u;
}

static void bmx_esp32_runtime_task_isolation_worker(void *argument) {
    BMXESP32TaskIsolationProbe *probe = (BMXESP32TaskIsolationProbe *)argument;
    probe->allocation = bmx_embedded_arena_allocate(16u);
    probe->callback_dispatched = bmx_esp32_managed_callback_dispatch(
        bmx_esp32_runtime_task_isolation_callback, (void *)&probe->callback_value);
    probe->completed = 1u;
    xTaskNotifyGive(probe->caller);
    vTaskDelete(NULL);
}

int32_t bmx_esp32_runtime_task_isolation_probe(void) {
    BMXESP32TaskIsolationProbe probe = {
        .caller = xTaskGetCurrentTaskHandle(),
        .allocation = NULL,
        .callback_value = 0u,
        .callback_dispatched = 0,
        .completed = 0u
    };
    const uint32_t used_before = bmx_embedded_arena_used();
    const uint32_t high_water_before = bmx_embedded_arena_high_water();
    const uint32_t allocations_before = bmx_embedded_arena_allocation_count();
    const uint32_t failures_before = bmx_embedded_arena_failure_count();
    const uint32_t violations_before = bmx_esp32_managed_context_violation_count();
    const uint32_t dispatches_before = bmx_esp32_managed_callback_dispatch_count();
    const uint32_t rejections_before = bmx_esp32_managed_callback_rejection_count();
    TaskHandle_t worker = NULL;

    if (!bmx_esp32_managed_callback_dispatch(
        bmx_esp32_runtime_task_isolation_callback, (void *)&probe.callback_value)) return 0;

    if (xTaskCreatePinnedToCore(bmx_esp32_runtime_task_isolation_worker,
        "bmx-runtime-probe", 2048, &probe, tskIDLE_PRIORITY + 1, &worker, 0) != pdPASS) return 0;
    if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000))) {
        vTaskDelete(worker);
        return 0;
    }

    return probe.completed && !probe.allocation && probe.callback_value == 1u &&
        !probe.callback_dispatched &&
        bmx_embedded_arena_used() == used_before &&
        bmx_embedded_arena_high_water() == high_water_before &&
        bmx_embedded_arena_allocation_count() == allocations_before &&
        bmx_embedded_arena_failure_count() == failures_before + 1u &&
        bmx_esp32_managed_context_violation_count() == violations_before + 2u &&
        bmx_esp32_managed_callback_dispatch_count() == dispatches_before + 1u &&
        bmx_esp32_managed_callback_rejection_count() == rejections_before + 1u;
}
